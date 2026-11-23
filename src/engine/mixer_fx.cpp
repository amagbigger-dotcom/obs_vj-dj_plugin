#include "engine/mixer_fx.hpp"

namespace {

constexpr float PI = 3.14159265358979323846f;

// Prím fésűszűrő és allpass méretek 48 kHz-re optimalizálva
// Jobb csatornán +23 minta eltolás a gazdag sztereó szélességhez
constexpr std::array<size_t, 4> COMB_LENGTHS_L = {1422, 1491, 1557, 1617};
constexpr std::array<size_t, 4> COMB_LENGTHS_R = {1445, 1514, 1580, 1640};

constexpr std::array<size_t, 2> ALLPASS_LENGTHS_L = {556, 441};
constexpr std::array<size_t, 2> ALLPASS_LENGTHS_R = {579, 464};

// Grain méret a Transpose Stretch granular pitch shifterhez (40 ms @ 48kHz = 1920 minta)
constexpr size_t GRAIN_SIZE_SAMPLES = 1920;

} // namespace

const char *MixerFxProcessor::name(MixerFxType type) noexcept
{
    switch (type) {
    case MixerFxType::FILTER:     return "Filter";
    case MixerFxType::DELAY:      return "Delay";
    case MixerFxType::REVERB:     return "Reverb";
    case MixerFxType::FLANGER:    return "Flanger";
    case MixerFxType::BEATMASHER: return "Beatmasher 2";
    case MixerFxType::TURNTABLE:  return "Turntable FX";
    case MixerFxType::RAMP_DELAY: return "Ramp Delay";
    case MixerFxType::MULHOLLAND: return "Mulholland Drive";
    case MixerFxType::TRANSPOSE:  return "Transpose Stretch";
    default:                      return "Filter";
    }
}

const char *MixerFxProcessor::shortName(MixerFxType type) noexcept
{
    switch (type) {
    case MixerFxType::FILTER:     return "FLT";
    case MixerFxType::DELAY:      return "DLY";
    case MixerFxType::REVERB:     return "REV";
    case MixerFxType::FLANGER:    return "FLG";
    case MixerFxType::BEATMASHER: return "BTM";
    case MixerFxType::TURNTABLE:  return "TTF";
    case MixerFxType::RAMP_DELAY: return "RMP";
    case MixerFxType::MULHOLLAND: return "MUL";
    case MixerFxType::TRANSPOSE:  return "TRS";
    default:                      return "FLT";
    }
}

const char *MixerFxProcessor::colorHex(MixerFxType type) noexcept
{
    switch (type) {
    case MixerFxType::FILTER:     return "#dc2626"; // Piros / Red
    case MixerFxType::DELAY:      return "#0284c7"; // Kék / Blue
    case MixerFxType::REVERB:     return "#16a34a"; // Zöld / Green
    case MixerFxType::FLANGER:    return "#ca8a04"; // Sárga / Yellow
    case MixerFxType::BEATMASHER: return "#9333ea"; // Lila / Purple
    case MixerFxType::TURNTABLE:  return "#ea580c"; // Narancs / Orange
    case MixerFxType::RAMP_DELAY: return "#06b6d4"; // Ciánkék / Cyan
    case MixerFxType::MULHOLLAND: return "#e11d48"; // Vérvörös / Crimson
    case MixerFxType::TRANSPOSE:  return "#db2777"; // Rózsaszín / Pink
    default:                      return "#dc2626";
    }
}

void MixerFxState::init(size_t sample_rate, size_t channel_idx)
{
    const size_t sr = (sample_rate > 0) ? sample_rate : 48000;

    // 1. Delay puffer: 2.0 másodperc előrefoglalás (zéró heap allokáció a render ciklusban)
    const size_t max_delay = sr * 2;
    delay_buf.assign(max_delay, 0.0f);
    delay_write_pos = 0;
    delay_damp_lp = 0.0f;
    delay_damp_hp = 0.0f;
    delay_send_hp = 0.0f;
    cur_delay_samples = static_cast<float>(sr) * 0.35156f;

    // 2. Reverb comb és allpass pufferek (+32 minta biztonsági margó a lebegő LFO modulációhoz)
    const auto &comb_lens = (channel_idx == 1) ? COMB_LENGTHS_R : COMB_LENGTHS_L;
    for (size_t i = 0; i < 4; ++i) {
        const size_t len = (comb_lens[i] * sr) / 48000 + 32;
        comb_buf[i].assign(len, 0.0f);
        comb_pos[i] = 0;
        comb_filter[i] = 0.0f;
    }

    const auto &ap_lens = (channel_idx == 1) ? ALLPASS_LENGTHS_R : ALLPASS_LENGTHS_L;
    for (size_t i = 0; i < 2; ++i) {
        const size_t len = (ap_lens[i] * sr) / 48000;
        allpass_buf[i].assign(len, 0.0f);
        allpass_pos[i] = 0;
    }
    reverb_lfo_phase = 0.0f;
    reverb_send_hp = 0.0f;

    // 3. Flanger puffer (1024 minta bőségesen elég a max 10ms pásztázáshoz)
    flanger_buf.assign(1024, 0.0f);
    flanger_write_pos = 0;
    flanger_lfo_phase = 0.0f;
    flanger_fb_hp = 0.0f;

    // 4. Beatmasher 2 puffer (2.0 másodperc előrefoglalás)
    beatmash_buf.assign(max_delay, 0.0f);
    beatmash_write_pos = 0;
    beatmash_freeze_anchor = 0;
    beatmash_playback_pos = 0.0f;
    beatmash_active = false;

    // 5. Turntable FX puffer (2.0 másodperc előrefoglalás)
    turntable_buf.assign(max_delay, 0.0f);
    turntable_write_pos = 0;
    turntable_read_pos = 0.0f;
    turntable_speed = 1.0f;

    // 6. Ramp Delay & Mulholland állapot
    ramp_phase = 0.0f;
    mulholland_bp_p0 = 0.0f;
    mulholland_bp_p1 = 0.0f;

    // 7. Transpose Stretch granular puffer (4096 minta)
    grain_buf.assign(4096, 0.0f);
    grain_write_pos = 0;
    grain_phase_a = 0.0f;
    grain_phase_b = 0.5f;

    dj_lp = 0.0f;
    dj_hp = 0.0f;
    cur_flt = 0.0f;
    initialized = true;
}

void MixerFxState::reset()
{
    std::fill(delay_buf.begin(), delay_buf.end(), 0.0f);
    delay_write_pos = 0;
    delay_damp_lp = 0.0f;
    delay_damp_hp = 0.0f;
    delay_send_hp = 0.0f;

    for (size_t i = 0; i < 4; ++i) {
        std::fill(comb_buf[i].begin(), comb_buf[i].end(), 0.0f);
        comb_pos[i] = 0;
        comb_filter[i] = 0.0f;
    }

    for (size_t i = 0; i < 2; ++i) {
        std::fill(allpass_buf[i].begin(), allpass_buf[i].end(), 0.0f);
        allpass_pos[i] = 0;
    }
    reverb_lfo_phase = 0.0f;
    reverb_send_hp = 0.0f;

    std::fill(flanger_buf.begin(), flanger_buf.end(), 0.0f);
    flanger_write_pos = 0;
    flanger_lfo_phase = 0.0f;
    flanger_fb_hp = 0.0f;

    std::fill(beatmash_buf.begin(), beatmash_buf.end(), 0.0f);
    beatmash_write_pos = 0;
    beatmash_freeze_anchor = 0;
    beatmash_playback_pos = 0.0f;
    beatmash_active = false;

    std::fill(turntable_buf.begin(), turntable_buf.end(), 0.0f);
    turntable_write_pos = 0;
    turntable_read_pos = 0.0f;
    turntable_speed = 1.0f;

    ramp_phase = 0.0f;
    mulholland_bp_p0 = 0.0f;
    mulholland_bp_p1 = 0.0f;

    std::fill(grain_buf.begin(), grain_buf.end(), 0.0f);
    grain_write_pos = 0;
    grain_phase_a = 0.0f;
    grain_phase_b = 0.5f;

    dj_lp = 0.0f;
    dj_hp = 0.0f;
    cur_flt = 0.0f;
}

void MixerFxProcessor::process(float *channel_data,
                               size_t num_frames,
                               size_t sample_rate,
                               float target_flt,
                               MixerFxType type,
                               double bpm,
                               MixerFxState &st,
                               size_t channel_idx)
{
    if (!channel_data || num_frames == 0)
        return;

    const float sr = (sample_rate > 0) ? static_cast<float>(sample_rate) : 48000.0f;
    if (!st.initialized) {
        st.init(static_cast<size_t>(sr), channel_idx);
    }

    const float smooth_rate = 0.002f; // ~10 ms simítás a cipzár-zaj ellen

    // =========================================================================
    // 1. Bipoláris DJ Filter Feldolgozása (AKUSZTIKAILAG OPTIMALIZÁLT GÖRBÉKKEL)
    // =========================================================================
    for (size_t i = 0; i < num_frames; ++i) {
        st.cur_flt += (target_flt - st.cur_flt) * smooth_rate;
        float out = channel_data[i];

        if (st.cur_flt < -0.001f) {
            // Low Pass: logaritmikus görbe 20 000 Hz-ről le egészen 30 Hz-ig (mély szub-basszusig)
            const float norm = 1.0f + st.cur_flt;
            const float exp_norm = std::pow(norm, 1.15f);
            const float cutoff = 30.0f * std::pow(666.67f, exp_norm);
            const float a_lp = std::clamp(2.0f * PI * cutoff / sr, 0.001f, 0.95f);
            st.dj_lp += a_lp * (out - st.dj_lp);
            st.dj_hp += 0.05f * (0.0f - st.dj_hp);
            out = st.dj_lp;
        } else if (st.cur_flt > 0.001f) {
            // High Pass: logaritmikus görbe 20 Hz-ről fel 3400 Hz-ig ("éppen csak cicergés maradjon")
            const float norm = st.cur_flt;
            const float exp_norm = std::pow(norm, 1.15f);
            const float cutoff = 20.0f * std::pow(170.0f, exp_norm);
            const float a_hp = std::clamp(2.0f * PI * cutoff / sr, 0.001f, 0.95f);
            st.dj_hp += a_hp * (out - st.dj_hp);
            st.dj_lp += 0.05f * (out - st.dj_lp);

            // High-Frequency Sizzle Sheen kiemelés a tartomány végén (75%..100%),
            // hogy a lábcin / tányér cicergés kristálytiszta és fényes maradjon, zéró némulás
            const float sizzle_boost = 1.0f + std::max(0.0f, (norm - 0.75f) * 1.2f);
            out = (out - st.dj_hp) * sizzle_boost;
        } else {
            // Bypassed / flat átmenet DC offset csillapítással
            st.dj_lp += 0.05f * (out - st.dj_lp);
            st.dj_hp += 0.05f * (0.0f - st.dj_hp);
        }

        channel_data[i] = out;
    }

    const float amount = std::clamp(std::abs(st.cur_flt), 0.0f, 1.0f);

    // Alap ütemidő (negyedhang másodpercben)
    float beat_sec = 0.46875f; // 128 BPM (468.75 ms)
    if (bpm > 30.0 && bpm < 250.0) {
        beat_sec = static_cast<float>(60.0 / bpm);
    }

    // =========================================================================
    // 0. 🔴 FILTER (Sima Bipoláris DJ Filter) vagy KÖZÉPÁLLÁS (Bypass)
    // =========================================================================
    // Ha az alap Sima Filter aktív, vagy a potméter középállásban van (< 0.3%),
    // azonnali transzparens átengedés (1.0x Unity Gain, zéró szintváltozás a listából választáskor!)
    if (type == MixerFxType::FILTER || amount < 0.003f) {
        return;
    }

    // =========================================================================
    // 1. 🔵 DELAY (Tempo-Synced 3/4 Dotted-8th Echo with +12ms Haas Stereo Spread)
    // =========================================================================
    if (type == MixerFxType::DELAY) {
        // Alap késleltetési idő: 3/4 ütem (pontozott nyolcad) mindkét csatornán
        // Jobb csatornán +12ms Haas késleltetés a sztereó térhatásért fáziskioltás nélkül
        const float haas_offset_sec = (channel_idx == 1) ? 0.012f : 0.0f;
        const float target_delay_sec = std::clamp(beat_sec * 0.75f + haas_offset_sec, 0.05f, 1.8f);
        const float target_samples = target_delay_sec * sr;

        st.cur_delay_samples += (target_samples - st.cur_delay_samples) * 0.002f;

        // Bemeneti Low-Cut (160 Hz HPF) a delay küldésen a lábdob és basszus bedörgés ellen
        const float alpha_send_hp = std::clamp(2.0f * PI * 160.0f / sr, 0.01f, 0.95f);

        const float send_gain = std::min(amount * 1.25f, 1.0f);
        const float feedback = 0.40f + amount * 0.38f; // 0.40 .. 0.78
        const float dry_gain = 1.0f - amount * 0.18f;
        const float wet_gain = amount * 1.45f;

        // Feedback sávszűrő: 6000 Hz LPF, 160 Hz HPF
        const float alpha_damp_lp = std::clamp(2.0f * PI * 6000.0f / sr, 0.01f, 0.95f);
        const float alpha_damp_hp = std::clamp(2.0f * PI * 160.0f / sr, 0.01f, 0.95f);

        const size_t buf_size = st.delay_buf.size();

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            // Bemenet mélyvágása az effekt küldés előtt
            st.delay_send_hp += alpha_send_hp * (in_sample - st.delay_send_hp);
            const float send_in = (in_sample - st.delay_send_hp) * send_gain;

            // Olvasás a körkörös pufferből tört-indexű lineáris interpolációval
            float r_pos = static_cast<float>(st.delay_write_pos) - st.cur_delay_samples;
            while (r_pos < 0.0f) r_pos += static_cast<float>(buf_size);
            const size_t idx0 = static_cast<size_t>(r_pos) % buf_size;
            const size_t idx1 = (idx0 + 1) % buf_size;
            const float frac = r_pos - static_cast<float>(static_cast<size_t>(r_pos));
            const float delayed = st.delay_buf[idx0] * (1.0f - frac) + st.delay_buf[idx1] * frac;

            // Szalagos feedback csillapítás
            st.delay_damp_lp += alpha_damp_lp * (delayed - st.delay_damp_lp);
            st.delay_damp_hp += alpha_damp_hp * (st.delay_damp_lp - st.delay_damp_hp);
            float fb_sig = st.delay_damp_lp - st.delay_damp_hp;

            // Lágy köbös telítés (Soft cubic saturation) -> analóg melegség, gerjedés kizárva
            fb_sig = std::clamp(fb_sig, -1.2f, 1.2f);
            fb_sig = fb_sig - 0.333f * fb_sig * fb_sig * fb_sig;

            st.delay_buf[st.delay_write_pos] = send_in + fb_sig * feedback;
            st.delay_write_pos = (st.delay_write_pos + 1) % buf_size;

            channel_data[i] = in_sample * dry_gain + delayed * wet_gain;
        }
        return;
    }

    // =========================================================================
    // 2. 🟢 REVERB (Tempo-Synced Plate Reverb with 280Hz Send Low-Cut & Controlled Gain)
    // =========================================================================
    if (type == MixerFxType::REVERB) {
        // Tempo-szinkronizált RT60 lecsengési idő: 1.5 beattől 5.0 beatig
        const float rt60_sec = std::clamp((1.5f + amount * 3.5f) * beat_sec, 0.4f, 4.5f);

        // Bemeneti Low-Cut (280 Hz HPF) az NI referencia alapján: "reduce bass before reverb"
        const float alpha_send_hp = std::clamp(2.0f * PI * 280.0f / sr, 0.01f, 0.95f);

        const float send_gain = std::min(amount * 1.20f, 1.0f);
        const float dry_gain = 1.0f - amount * 0.15f;
        const float wet_gain = amount * 0.90f; // Megfékezett wet szint: nincs hangerőrobbanás
        const float damp = 0.28f + amount * 0.12f;

        const float lfo_freq = (bpm > 30.0 && bpm < 250.0) ? static_cast<float>(bpm / (60.0 * 4.0)) : 0.5f;
        const float lfo_phase_inc = 2.0f * PI * lfo_freq / sr;

        const auto &comb_lens = (channel_idx == 1) ? COMB_LENGTHS_R : COMB_LENGTHS_L;

        std::array<float, 4> comb_feedback;
        for (size_t c = 0; c < 4; ++c) {
            const float nom_len = static_cast<float>((comb_lens[c] * static_cast<size_t>(sr)) / 48000);
            comb_feedback[c] = std::clamp(std::pow(10.0f, -3.0f * nom_len / (rt60_sec * sr)), 0.60f, 0.92f);
        }

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            st.reverb_send_hp += alpha_send_hp * (in_sample - st.reverb_send_hp);
            const float send_sample = (in_sample - st.reverb_send_hp) * send_gain;

            st.reverb_lfo_phase += lfo_phase_inc;
            if (st.reverb_lfo_phase > 2.0f * PI)
                st.reverb_lfo_phase -= 2.0f * PI;

            float comb_sum = 0.0f;
            for (size_t c = 0; c < 4; ++c) {
                const size_t buf_cap = st.comb_buf[c].size();
                const float nom_len = static_cast<float>((comb_lens[c] * static_cast<size_t>(sr)) / 48000);

                const float mod_offset = nom_len + std::sin(st.reverb_lfo_phase + static_cast<float>(c) * 1.5707963f) * 3.5f;
                float r_pos = static_cast<float>(st.comb_pos[c]) - mod_offset;
                while (r_pos < 0.0f) r_pos += static_cast<float>(buf_cap);
                while (r_pos >= static_cast<float>(buf_cap)) r_pos -= static_cast<float>(buf_cap);

                const size_t idx0 = static_cast<size_t>(r_pos);
                const size_t idx1 = (idx0 + 1) % buf_cap;
                const float frac = r_pos - static_cast<float>(idx0);
                const float out_comb = st.comb_buf[c][idx0] * (1.0f - frac) + st.comb_buf[c][idx1] * frac;

                st.comb_filter[c] = out_comb * (1.0f - damp) + st.comb_filter[c] * damp;

                // Lágy szaturáció a fésű visszacsatolásban
                float fb_c = st.comb_filter[c] * comb_feedback[c];
                fb_c = std::clamp(fb_c, -1.0f, 1.0f);
                fb_c = fb_c - 0.333f * fb_c * fb_c * fb_c;

                st.comb_buf[c][st.comb_pos[c]] = send_sample + fb_c;
                st.comb_pos[c] = (st.comb_pos[c] + 1) % buf_cap;

                comb_sum += out_comb;
            }

            // 0.18x skálázás 4 fésű összegzéséhez: megvédi az RMS szintet
            float ap_sig = comb_sum * 0.18f;
            for (size_t a = 0; a < 2; ++a) {
                const size_t ap_len = st.allpass_buf[a].size();
                const float buf_out = st.allpass_buf[a][st.allpass_pos[a]];
                const float ap_in = ap_sig;
                st.allpass_buf[a][st.allpass_pos[a]] = ap_in + buf_out * 0.50f;
                ap_sig = buf_out - ap_in * 0.50f;
                st.allpass_pos[a] = (st.allpass_pos[a] + 1) % ap_len;
            }

            channel_data[i] = in_sample * dry_gain + ap_sig * wet_gain;
        }
        return;
    }

    // =========================================================================
    // 3. 🟡 FLANGER (Constant-Power Stereo Jet Flanger with 120Hz Feedback Low-Cut)
    // =========================================================================
    if (type == MixerFxType::FLANGER) {
        // Zenei szintezés: 0%-nál pontosan 1.0x (0 dB), tekeréskor dinamikus sweep hangerővesztés nélkül
        const float dry_gain = 1.0f - amount * 0.20f;
        const float wet_gain = amount * 1.05f;
        const float feedback = 0.45f + amount * 0.35f;

        float lfo_freq = 0.25f;
        if (bpm > 30.0 && bpm < 250.0) {
            lfo_freq = static_cast<float>(bpm / (60.0 * 8.0)); // 2 teljes ütem (8 beat)
        }
        const float phase_inc = 2.0f * PI * lfo_freq / sr;

        const float base_samples = 0.0012f * sr;
        const float depth_samples = 0.0022f * sr * (0.5f + amount * 0.5f);

        // 120 Hz HPF visszacsatolásban a szub-basszus gerjedés és feldúsulás kizárására
        const float alpha_fb_hp = std::clamp(2.0f * PI * 120.0f / sr, 0.01f, 0.95f);

        const size_t buf_size = st.flanger_buf.size();

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            st.flanger_lfo_phase += phase_inc;
            if (st.flanger_lfo_phase > 2.0f * PI)
                st.flanger_lfo_phase -= 2.0f * PI;

            const float ch_phase = st.flanger_lfo_phase + ((channel_idx == 1) ? 1.5707963f : 0.0f);
            const float lfo = 0.5f * (1.0f + std::sin(ch_phase));
            const float cur_delay = base_samples + depth_samples * lfo;

            float r_pos = static_cast<float>(st.flanger_write_pos) - cur_delay;
            while (r_pos < 0.0f) r_pos += static_cast<float>(buf_size);
            const size_t idx0 = static_cast<size_t>(r_pos) % buf_size;
            const size_t idx1 = (idx0 + 1) % buf_size;
            const float frac = r_pos - static_cast<float>(static_cast<size_t>(r_pos));
            const float flg_delayed = st.flanger_buf[idx0] * (1.0f - frac) + st.flanger_buf[idx1] * frac;

            st.flanger_fb_hp += alpha_fb_hp * (flg_delayed - st.flanger_fb_hp);
            float fb_sig = (flg_delayed - st.flanger_fb_hp) * feedback;
            fb_sig = std::clamp(fb_sig, -1.2f, 1.2f);
            fb_sig = fb_sig - 0.333f * fb_sig * fb_sig * fb_sig;

            st.flanger_buf[st.flanger_write_pos] = in_sample + fb_sig;
            st.flanger_write_pos = (st.flanger_write_pos + 1) % buf_size;

            channel_data[i] = in_sample * dry_gain + flg_delayed * wet_gain;
        }
        return;
    }

    // =========================================================================
    // 4. 🟣 BEATMASHER 2 (Rhythmic Glitch Stutter 1/2 .. 1/32 Beat Loop Capture)
    // =========================================================================
    if (type == MixerFxType::BEATMASHER) {
        const size_t buf_size = st.beatmash_buf.size();

        if (amount < 0.05f) {
            // Effekt inaktív: folyamatos puffer feltöltés, tiszta átengedés
            st.beatmash_active = false;
            for (size_t i = 0; i < num_frames; ++i) {
                st.beatmash_buf[st.beatmash_write_pos] = channel_data[i];
                st.beatmash_write_pos = (st.beatmash_write_pos + 1) % buf_size;
            }
            return;
        }

        // Slice hossza az amount függvényében: 1/2 beat (<0.25) -> 1/4 -> 1/8 -> 1/16 -> 1/32 (>=0.90)
        float beat_mult = 0.50f;
        if (amount >= 0.90f)      beat_mult = 0.03125f; // 1/32
        else if (amount >= 0.75f) beat_mult = 0.0625f;  // 1/16
        else if (amount >= 0.50f) beat_mult = 0.125f;   // 1/8
        else if (amount >= 0.25f) beat_mult = 0.25f;    // 1/4

        const float slice_samples = std::max(64.0f, beat_mult * beat_sec * sr);

        // Első bekapcsoláskor rögzítjük az anchor pontot
        if (!st.beatmash_active) {
            st.beatmash_active = true;
            st.beatmash_freeze_anchor = st.beatmash_write_pos;
            st.beatmash_playback_pos = 0.0f;
        }

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            // Folyamatos háttér-pufferelés
            st.beatmash_buf[st.beatmash_write_pos] = in_sample;
            st.beatmash_write_pos = (st.beatmash_write_pos + 1) % buf_size;

            st.beatmash_playback_pos += 1.0f;
            if (st.beatmash_playback_pos >= slice_samples) {
                st.beatmash_playback_pos -= slice_samples;
            }

            // Kattanásmentesítő 32-mintás lineáris crossfade a szelet peremeinél
            float window = 1.0f;
            if (st.beatmash_playback_pos < 32.0f) {
                window = st.beatmash_playback_pos / 32.0f;
            } else if (st.beatmash_playback_pos > (slice_samples - 32.0f)) {
                window = (slice_samples - st.beatmash_playback_pos) / 32.0f;
            }

            const size_t r_idx = (st.beatmash_freeze_anchor + static_cast<size_t>(st.beatmash_playback_pos)) % buf_size;
            const float stutter_sample = st.beatmash_buf[r_idx] * window;

            channel_data[i] = in_sample * (1.0f - amount) + stutter_sample * amount;
        }
        return;
    }

    // =========================================================================
    // 5. 🟠 TURNTABLE FX (Vinyl Brake on Left / Spinback on Right)
    // =========================================================================
    if (type == MixerFxType::TURNTABLE) {
        const size_t buf_size = st.turntable_buf.size();

        if (amount < 0.05f) {
            st.turntable_speed = 1.0f;
            st.turntable_read_pos = static_cast<float>(st.turntable_write_pos);
            for (size_t i = 0; i < num_frames; ++i) {
                st.turntable_buf[st.turntable_write_pos] = channel_data[i];
                st.turntable_write_pos = (st.turntable_write_pos + 1) % buf_size;
            }
            return;
        }

        const bool is_brake = (st.cur_flt < 0.0f); // Balra tekerve = Vinyl Brake, Jobbra = Spinback

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];
            st.turntable_buf[st.turntable_write_pos] = in_sample;
            st.turntable_write_pos = (st.turntable_write_pos + 1) % buf_size;

            if (is_brake) {
                // Vinyl fékezés: sebesség fokozatosan lassul 0-ra
                const float target_spd = std::max(0.0f, 1.0f - amount * 1.05f);
                st.turntable_speed += (target_spd - st.turntable_speed) * 0.003f;
                st.turntable_read_pos += st.turntable_speed;
            } else {
                // Vinyl visszapörgetés (Spinback): hátrafelé gyorsulva olvas
                const float spin_spd = -(1.0f + amount * 3.5f);
                st.turntable_read_pos += spin_spd;
            }

            while (st.turntable_read_pos < 0.0f) st.turntable_read_pos += static_cast<float>(buf_size);
            while (st.turntable_read_pos >= static_cast<float>(buf_size)) st.turntable_read_pos -= static_cast<float>(buf_size);

            const size_t idx0 = static_cast<size_t>(st.turntable_read_pos) % buf_size;
            const size_t idx1 = (idx0 + 1) % buf_size;
            const float frac = st.turntable_read_pos - static_cast<float>(idx0);
            const float vinyl_sample = st.turntable_buf[idx0] * (1.0f - frac) + st.turntable_buf[idx1] * frac;

            channel_data[i] = in_sample * (1.0f - amount) + vinyl_sample * amount;
        }
        return;
    }

    // =========================================================================
    // 6. 🚀 RAMP DELAY (Accelerating Doppler Pitch-Rising Siren Riser)
    // =========================================================================
    if (type == MixerFxType::RAMP_DELAY) {
        const size_t buf_size = st.delay_buf.size();

        // Ütemhez szinkronizált 4-beat gyorsuló rámpa
        const float ramp_freq = (bpm > 30.0 && bpm < 250.0) ? static_cast<float>(bpm / (60.0 * 4.0)) : 0.5f;
        const float ramp_inc = 2.0f * PI * ramp_freq / sr;

        const float send_gain = std::min(amount * 1.30f, 1.0f);
        const float feedback = 0.55f + amount * 0.32f; // 0.55 .. 0.87
        const float dry_gain = 1.0f - amount * 0.20f;
        const float wet_gain = amount * 1.35f;

        // Felfelé pásztázó sávszűrő a sziréna rezonanciájához (400 Hz -> 2600 Hz)
        const float bp_freq = 400.0f + amount * 2200.0f;
        const float alpha_bp = std::clamp(2.0f * PI * bp_freq / sr, 0.01f, 0.95f);

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            st.ramp_phase += ramp_inc;
            if (st.ramp_phase > 2.0f * PI)
                st.ramp_phase -= 2.0f * PI;

            // Rámpa: a delay idő ütemesen összenyomódik -> Doppler frekvencia-emelkedés (UFO sziréna effekt)
            const float ramp_norm = st.ramp_phase / (2.0f * PI);
            const float sweep_factor = (1.0f - 0.75f * amount * ramp_norm);
            const float dly_time_sec = std::clamp(0.50f * beat_sec * sweep_factor, 0.02f, 1.5f);
            const float dly_samples = dly_time_sec * sr;

            float r_pos = static_cast<float>(st.delay_write_pos) - dly_samples;
            while (r_pos < 0.0f) r_pos += static_cast<float>(buf_size);
            const size_t idx0 = static_cast<size_t>(r_pos) % buf_size;
            const size_t idx1 = (idx0 + 1) % buf_size;
            const float frac = r_pos - static_cast<float>(idx0);
            const float delayed = st.delay_buf[idx0] * (1.0f - frac) + st.delay_buf[idx1] * frac;

            // Sziréna sávszűrés és szaturáció
            st.mulholland_bp_p0 += alpha_bp * (delayed - st.mulholland_bp_p0);
            float siren_sig = st.mulholland_bp_p0 * feedback;
            siren_sig = std::clamp(siren_sig, -1.2f, 1.2f);
            siren_sig = siren_sig - 0.333f * siren_sig * siren_sig * siren_sig;

            st.delay_buf[st.delay_write_pos] = in_sample * send_gain + siren_sig;
            st.delay_write_pos = (st.delay_write_pos + 1) % buf_size;

            channel_data[i] = in_sample * dry_gain + delayed * wet_gain;
        }
        return;
    }

    // =========================================================================
    // 7. 🔥 MULHOLLAND DRIVE (Asymmetric Tube Overdrive + Resonant Feedback Growl)
    // =========================================================================
    if (type == MixerFxType::MULHOLLAND) {
        // Zenei analóg előerősítés: 1.0x .. 2.2x (zéró hard-clipping túlhajtás)
        const float pre_gain = 1.0f + amount * 1.20f;
        const float feedback = amount * 0.40f;

        // Valódi Wet/Dry crossfade: ahogy nő az overdrive, úgy halkul a tiszta jel
        const float dry_gain = 1.0f - amount * 0.65f;
        const float wet_gain = amount * 0.78f;

        // 80 Hz Low-Cut a torzító érzékelőjén: megvédi a szub-basszust a négyszögesedéstől és intermodulációtól
        const float alpha_hp = std::clamp(2.0f * PI * 80.0f / sr, 0.005f, 0.90f);

        // Rezonáns sávszűrő frekvencia (300 Hz .. 1400 Hz zenei felharmonikus süvítés)
        const float bp_freq = 300.0f + amount * 1100.0f;
        const float alpha_bp = std::clamp(2.0f * PI * bp_freq / sr, 0.01f, 0.90f);

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            // 80 Hz mélyvágás az overdrive áramkör előtt
            st.delay_damp_hp += alpha_hp * (in_sample - st.delay_damp_hp);
            const float drive_input = in_sample - st.delay_damp_hp;

            // Rezonáns visszacsatolt felharmonikus szűrés
            st.mulholland_bp_p0 += alpha_bp * (drive_input - st.mulholland_bp_p0);
            st.mulholland_bp_p1 += alpha_bp * (st.mulholland_bp_p0 - st.mulholland_bp_p1);
            const float resonant_sig = (st.mulholland_bp_p0 - st.mulholland_bp_p1) * 1.5f;

            // Aszimmetrikus analóg csöves telítés (Tube saturation)
            float driven = (drive_input * pre_gain) + resonant_sig * feedback;
            driven = std::tanh(driven);
            if (driven > 0.0f) {
                // Csöves páros felharmonikus aszimmetria
                driven = driven / (1.0f + 0.30f * driven);
            }

            // Normált kimeneti szint a hangerőugrás és recsegés megelőzésére
            const float out_wet = driven * 0.90f;

            channel_data[i] = in_sample * dry_gain + out_wet * wet_gain;
        }
        return;
    }

    // =========================================================================
    // 8. 🎚️ TRANSPOSE STRETCH (Dual-Grain Real-Time Octave Pitch Riser / Dropper)
    // =========================================================================
    if (type == MixerFxType::TRANSPOSE) {
        const size_t buf_size = st.grain_buf.size();

        if (amount < 0.05f) {
            st.grain_phase_a = 0.0f;
            st.grain_phase_b = 0.5f;
            for (size_t i = 0; i < num_frames; ++i) {
                st.grain_buf[st.grain_write_pos] = channel_data[i];
                st.grain_write_pos = (st.grain_write_pos + 1) % buf_size;
            }
            return;
        }

        // Pitch arány: Jobbra = felhangolás +12 félhangig (+1 oktáv, ratio 2.0)
        //              Balra  = lehangolás -12 félhangig (-1 oktáv, ratio 0.5)
        const float semitones = (st.cur_flt >= 0.0f) ? (amount * 12.0f) : (-amount * 12.0f);
        const float pitch_ratio = std::pow(2.0f, semitones / 12.0f);

        // Két átlapolt háromszög ablakozású grain (Phase A és Phase B 180° eltolással)
        const float phase_rate = (pitch_ratio - 1.0f) / static_cast<float>(GRAIN_SIZE_SAMPLES);

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            st.grain_buf[st.grain_write_pos] = in_sample;
            const size_t cur_write = st.grain_write_pos;
            st.grain_write_pos = (st.grain_write_pos + 1) % buf_size;

            st.grain_phase_a += phase_rate;
            while (st.grain_phase_a >= 1.0f) st.grain_phase_a -= 1.0f;
            while (st.grain_phase_a < 0.0f)  st.grain_phase_a += 1.0f;

            st.grain_phase_b += phase_rate;
            while (st.grain_phase_b >= 1.0f) st.grain_phase_b -= 1.0f;
            while (st.grain_phase_b < 0.0f)  st.grain_phase_b += 1.0f;

            // Grain A olvasás és háromszög ablakozás
            const float delay_a = st.grain_phase_a * static_cast<float>(GRAIN_SIZE_SAMPLES);
            float r_pos_a = static_cast<float>(cur_write) - delay_a;
            while (r_pos_a < 0.0f) r_pos_a += static_cast<float>(buf_size);
            const size_t idx_a = static_cast<size_t>(r_pos_a) % buf_size;
            const float win_a = 1.0f - std::abs(2.0f * st.grain_phase_a - 1.0f);

            // Grain B olvasás és háromszög ablakozás
            const float delay_b = st.grain_phase_b * static_cast<float>(GRAIN_SIZE_SAMPLES);
            float r_pos_b = static_cast<float>(cur_write) - delay_b;
            while (r_pos_b < 0.0f) r_pos_b += static_cast<float>(buf_size);
            const size_t idx_b = static_cast<size_t>(r_pos_b) % buf_size;
            const float win_b = 1.0f - std::abs(2.0f * st.grain_phase_b - 1.0f);

            const float pitched = st.grain_buf[idx_a] * win_a + st.grain_buf[idx_b] * win_b;

            channel_data[i] = in_sample * (1.0f - amount) + pitched * amount;
        }
        return;
    }
}

