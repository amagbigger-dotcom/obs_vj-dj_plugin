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
    turntable_decay = 1.0f;
    turntable_braking = false;
    turntable_spinning = false;
    turntable_spin_anchor = 0;

    // 6. Ramp Delay & Mulholland állapot
    ramp_phase = 0.0f;
    mulholland_bp_p0 = 0.0f;
    mulholland_bp_p1 = 0.0f;

    // 7. Transpose Stretch granular puffer (4096 minta)
    grain_buf.assign(4096, 0.0f);
    grain_write_pos = 0;
    grain_phase_a = 0.0f;
    grain_phase_b = 0.5f;

    tpt_s_lp = 0.0f;
    tpt_s_hp = 0.0f;
    cur_flt = 0.0f;
    cur_flt_s2 = 0.0f;
    prev_dry_gain = 1.0f;
    prev_wet_gain = 0.0f;
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
    turntable_decay = 1.0f;
    turntable_braking = false;
    turntable_spinning = false;
    turntable_spin_anchor = 0;

    ramp_phase = 0.0f;
    mulholland_bp_p0 = 0.0f;
    mulholland_bp_p1 = 0.0f;

    std::fill(grain_buf.begin(), grain_buf.end(), 0.0f);
    grain_write_pos = 0;
    grain_phase_a = 0.0f;
    grain_phase_b = 0.5f;

    tpt_s_lp = 0.0f;
    tpt_s_hp = 0.0f;
    cur_flt = 0.0f;
    cur_flt_s2 = 0.0f;
    prev_dry_gain = 1.0f;
    prev_wet_gain = 0.0f;
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

    // =========================================================================
    // 1. Bipoláris DJ TPT Szűrő (Kattogásmentes VA Integrátor + C1 Trajektória)
    // =========================================================================
    // 2-fokozatú kritikus csillapítású szűrő a potméter mozgásra (~12 ms időállandó)
    // Megszünteti a hirtelen kézmozdulatok által okozott sebességtöréseket (C1 folytonosság)
    const float alpha_flt = 0.0035f;

    // A Beatmasher, Turntable és Transpose Stretch effektek a tiszta, teljes sávszélességű
    // zenei jelet kapják (kick/snare punch megőrzése, zéró némulás!).
    // A Filter, Delay, Reverb, Flanger, Ramp Delay és Mulholland effekteknél
    // a bipoláris TPT szűrő formálja a frekvenciatartományt.
    const bool apply_bipolar_filter = (type == MixerFxType::FILTER ||
                                       type == MixerFxType::DELAY ||
                                       type == MixerFxType::REVERB ||
                                       type == MixerFxType::FLANGER ||
                                       type == MixerFxType::RAMP_DELAY ||
                                       type == MixerFxType::MULHOLLAND);

    for (size_t i = 0; i < num_frames; ++i) {
        st.cur_flt += alpha_flt * (target_flt - st.cur_flt);
        st.cur_flt_s2 += alpha_flt * (st.cur_flt - st.cur_flt_s2);
        const float flt_val = st.cur_flt_s2;

        if (!apply_bipolar_filter)
            continue;

        float x = channel_data[i];

        // 1. TPT High-Pass Szűrő (Bilineáris Trapézintegrátor)
        // Ha flt_val <= 0 (balra tekerve vagy középállásban), a vágás 20 Hz (teljes átengedés felül)
        // Ha flt_val > 0 (jobbra tekerve), logaritmikus görbe 20 Hz-ről 7200 Hz-ig ("csak tiszta cicergés maradjon")
        float fc_hp = 20.0f;
        float sizzle_boost = 1.0f;
        if (flt_val > 0.0005f) {
            const float norm_hp = std::min(flt_val, 1.0f);
            const float exp_norm_hp = std::pow(norm_hp, 1.25f);
            fc_hp = 20.0f * std::pow(360.0f, exp_norm_hp); // 20 * 360 = 7200 Hz
            sizzle_boost = 1.0f + std::max(0.0f, (norm_hp - 0.70f) * 1.6f); // +3.5 dB sheen
        }
        const float w_hp = PI * std::clamp(fc_hp, 10.0f, sr * 0.45f) / sr;
        const float g_hp = std::tan(w_hp);
        const float g_hp_div = g_hp / (1.0f + g_hp);

        const float v_hp = (x - st.tpt_s_hp) * g_hp_div;
        const float y_lp_hp = v_hp + st.tpt_s_hp;
        st.tpt_s_hp = 2.0f * v_hp + st.tpt_s_hp;
        if (std::abs(st.tpt_s_hp) < 1e-15f) st.tpt_s_hp = 0.0f; // Anti-denormal védelem
        const float x_hp = (x - y_lp_hp) * sizzle_boost;

        // 2. TPT Low-Pass Szűrő (Bilineáris Trapézintegrátor)
        // Ha flt_val >= 0 (jobbra tekerve vagy középállásban), a vágás 20000 Hz (teljes átengedés alul)
        // Ha flt_val < 0 (balra tekerve), logaritmikus görbe 20000 Hz-ről 30 Hz-ig (mély szub-basszus)
        float fc_lp = 20000.0f;
        if (flt_val < -0.0005f) {
            const float norm_lp = std::max(1.0f + flt_val, 0.0f);
            const float exp_norm_lp = std::pow(norm_lp, 1.15f);
            fc_lp = 30.0f * std::pow(666.67f, exp_norm_lp);
        }
        const float w_lp = PI * std::clamp(fc_lp, 10.0f, sr * 0.45f) / sr;
        const float g_lp = std::tan(w_lp);
        const float g_lp_div = g_lp / (1.0f + g_lp);

        const float v_lp = (x_hp - st.tpt_s_lp) * g_lp_div;
        const float y_lp = v_lp + st.tpt_s_lp;
        st.tpt_s_lp = 2.0f * v_lp + st.tpt_s_lp;
        if (std::abs(st.tpt_s_lp) < 1e-15f) st.tpt_s_lp = 0.0f; // Anti-denormal védelem

        channel_data[i] = y_lp;
    }

    const float amount = std::clamp(std::abs(st.cur_flt_s2), 0.0f, 1.0f);

    // Alap ütemidő (negyedhang másodpercben)
    float beat_sec = 0.46875f; // 128 BPM (468.75 ms)
    if (bpm > 30.0 && bpm < 250.0) {
        beat_sec = static_cast<float>(60.0 / bpm);
    }

    // =========================================================================
    // 0. 🔴 FILTER (Sima Bipoláris DJ Filter) vagy KÖZÉPÁLLÁS (Bypass)
    // =========================================================================
    // Ha az alap Sima Filter aktív, vagy a potméter középállásban van ÉS a lecsengés lefutott,
    // azonnali transzparens átengedés (1.0x Unity Gain, zéró szintváltozás a listából választáskor!)
    if (type == MixerFxType::FILTER || (amount < 0.003f && st.prev_wet_gain < 0.001f)) {
        st.prev_dry_gain = 1.0f;
        st.prev_wet_gain = 0.0f;
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

        const float target_dry = 1.0f - amount * 0.18f;
        const float target_wet = amount * 1.45f;
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

        // Feedback sávszűrő: 6000 Hz LPF, 160 Hz HPF
        const float alpha_damp_lp = std::clamp(2.0f * PI * 6000.0f / sr, 0.01f, 0.95f);
        const float alpha_damp_hp = std::clamp(2.0f * PI * 160.0f / sr, 0.01f, 0.95f);

        const size_t buf_size = st.delay_buf.size();

        for (size_t i = 0; i < num_frames; ++i) {
            cur_dry += dry_step;
            cur_wet += wet_step;
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

            channel_data[i] = in_sample * cur_dry + delayed * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }

    // =========================================================================
    // 2. 🟢 REVERB (Tempo-Synced Plate Reverb with 280Hz Send Low-Cut & Controlled Gain)
    // =========================================================================
    if (type == MixerFxType::REVERB) {
        // Tempo-szinkronizált RT60 lecsengési idő: 2.0 beattől 12.0 beatig (akár 6.5s lecsengés!)
        const float rt60_sec = std::clamp((2.0f + amount * 10.0f) * beat_sec, 0.6f, 7.0f);

        // Bemeneti Low-Cut (240 Hz HPF)
        const float alpha_send_hp = std::clamp(2.0f * PI * 240.0f / sr, 0.01f, 0.95f);

        const float send_gain = std::min(amount * 1.25f, 1.0f);
        const float target_dry = 1.0f - amount * 0.22f;
        const float target_wet = amount * 1.15f; // Tágas, határozott wet szint
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

        // Alacsonyabb csillapítás: a fényes lemez (plate) csillogás hosszan, zeneien lebeg
        const float damp = 0.12f + amount * 0.07f;

        const float lfo_freq = (bpm > 30.0 && bpm < 250.0) ? static_cast<float>(bpm / (60.0 * 4.0)) : 0.5f;
        const float lfo_phase_inc = 2.0f * PI * lfo_freq / sr;

        const auto &comb_lens = (channel_idx == 1) ? COMB_LENGTHS_R : COMB_LENGTHS_L;

        std::array<float, 4> comb_feedback;
        for (size_t c = 0; c < 4; ++c) {
            const float nom_len = static_cast<float>((comb_lens[c] * static_cast<size_t>(sr)) / 48000);
            comb_feedback[c] = std::clamp(std::pow(10.0f, -3.0f * nom_len / (rt60_sec * sr)), 0.65f, 0.978f);
        }

        for (size_t i = 0; i < num_frames; ++i) {
            cur_dry += dry_step;
            cur_wet += wet_step;
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

            channel_data[i] = in_sample * cur_dry + ap_sig * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }

    // =========================================================================
    // 3. 🟡 FLANGER (Constant-Power Stereo Jet Flanger with 120Hz Feedback Low-Cut)
    // =========================================================================
    if (type == MixerFxType::FLANGER) {
        // Zenei szintezés: 0%-nál pontosan 1.0x (0 dB), tekeréskor dinamikus sweep hangerővesztés nélkül
        const float target_dry = 1.0f - amount * 0.20f;
        const float target_wet = amount * 1.05f;
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

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
            cur_dry += dry_step;
            cur_wet += wet_step;
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

            channel_data[i] = in_sample * cur_dry + flg_delayed * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }

    // =========================================================================
    // 4. 🟣 BEATMASHER 2 (Rhythmic Glitch Stutter 1/2 .. 1/32 Beat Loop Capture)
    // =========================================================================
    if (type == MixerFxType::BEATMASHER) {
        const size_t buf_size = st.beatmash_buf.size();

        if (amount < 0.04f) {
            // Effekt inaktív: folyamatos puffer feltöltés, tiszta átengedés
            st.beatmash_active = false;
            for (size_t i = 0; i < num_frames; ++i) {
                st.beatmash_buf[st.beatmash_write_pos] = channel_data[i];
                st.beatmash_write_pos = (st.beatmash_write_pos + 1) % buf_size;
            }
            st.prev_dry_gain = 1.0f;
            st.prev_wet_gain = 0.0f;
            return;
        }

        const float target_dry = 1.0f - amount * 0.90f;
        const float target_wet = amount * 1.05f;
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

        // 8-fokozatú progresszív ritmikai leosztás holtsávok nélkül:
        // 1/2 -> 3/8 -> 1/4 -> 3/16 -> 1/8 -> 1/12 (triola) -> 1/16 -> 1/24 (triola) -> 1/32
        float beat_mult = 0.50f;
        if (amount >= 0.92f)      beat_mult = 0.03125f; // 1/32
        else if (amount >= 0.84f) beat_mult = 0.04167f; // 1/24 (triplet)
        else if (amount >= 0.74f) beat_mult = 0.0625f;  // 1/16
        else if (amount >= 0.62f) beat_mult = 0.08333f; // 1/12 (triplet)
        else if (amount >= 0.50f) beat_mult = 0.125f;   // 1/8
        else if (amount >= 0.38f) beat_mult = 0.1875f;  // 3/16 (dotted 8th)
        else if (amount >= 0.25f) beat_mult = 0.25f;    // 1/4
        else if (amount >= 0.14f) beat_mult = 0.375f;   // 3/8 (dotted quarter)
        else                      beat_mult = 0.50f;    // 1/2

        const float slice_samples = std::max(64.0f, beat_mult * beat_sec * sr);

        // Első bekapcsoláskor rögzítjük az anchor pontot
        if (!st.beatmash_active) {
            st.beatmash_active = true;
            st.beatmash_freeze_anchor = st.beatmash_write_pos;
            st.beatmash_playback_pos = 0.0f;
        }

        for (size_t i = 0; i < num_frames; ++i) {
            cur_dry += dry_step;
            cur_wet += wet_step;
            const float in_sample = channel_data[i]; // Tiszta, teljes sávszélességű audió

            // Folyamatos háttér-pufferelés
            st.beatmash_buf[st.beatmash_write_pos] = in_sample;
            st.beatmash_write_pos = (st.beatmash_write_pos + 1) % buf_size;

            st.beatmash_playback_pos += 1.0f;
            if (st.beatmash_playback_pos >= slice_samples) {
                st.beatmash_playback_pos -= slice_samples;
            }

            // Kattanásmentesítő 48-mintás Hann-ablakozott mikro-crossfade a szelet peremeinél
            float window = 1.0f;
            const float fade_len = std::min(48.0f, slice_samples * 0.25f);
            if (st.beatmash_playback_pos < fade_len) {
                const float phase = (st.beatmash_playback_pos / fade_len) * 0.5f * PI;
                window = std::sin(phase);
            } else if (st.beatmash_playback_pos > (slice_samples - fade_len)) {
                const float phase = ((slice_samples - st.beatmash_playback_pos) / fade_len) * 0.5f * PI;
                window = std::sin(phase);
            }

            const size_t r_idx = (st.beatmash_freeze_anchor + static_cast<size_t>(st.beatmash_playback_pos)) % buf_size;
            const float stutter_sample = st.beatmash_buf[r_idx] * window;

            channel_data[i] = in_sample * cur_dry + stutter_sample * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }

    // =========================================================================
    // 5. 🟠 TURNTABLE FX (Physical Vinyl Motor Simulation: Brake on Left / Spinback on Right)
    // =========================================================================
    if (type == MixerFxType::TURNTABLE) {
        const size_t buf_size = st.turntable_buf.size();

        if (amount < 0.04f) {
            st.turntable_speed = 1.0f;
            st.turntable_decay = 1.0f;
            st.turntable_braking = false;
            st.turntable_spinning = false;
            st.turntable_read_pos = static_cast<float>(st.turntable_write_pos);
            for (size_t i = 0; i < num_frames; ++i) {
                st.turntable_buf[st.turntable_write_pos] = channel_data[i];
                st.turntable_write_pos = (st.turntable_write_pos + 1) % buf_size;
            }
            st.prev_dry_gain = 1.0f;
            st.prev_wet_gain = 0.0f;
            return;
        }

        // Amikor a Turntable FX aktív, a tiszta futó zene (Dry) teljesen elhalkul,
        // kizárólag a fizikai lemez lelassulása vagy visszapörgése hallható!
        const float target_dry = 0.0f;
        const float target_wet = 1.0f;
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

        const bool is_brake = (st.cur_flt_s2 < 0.0f); // Balra = Vinyl Brake, Jobbra = Spinback

        if (is_brake) {
            st.turntable_spinning = false;
            if (!st.turntable_braking) {
                st.turntable_braking = true;
                st.turntable_speed = 1.0f;
                st.turntable_read_pos = static_cast<float>(st.turntable_write_pos);
            }
        } else {
            st.turntable_braking = false;
            if (!st.turntable_spinning) {
                st.turntable_spinning = true;
                st.turntable_speed = -(2.5f + amount * 3.0f);
                st.turntable_decay = 1.0f;
                st.turntable_spin_anchor = st.turntable_write_pos;
                st.turntable_read_pos = static_cast<float>(st.turntable_write_pos);
            }
        }

        // Fékezési ráta mintánként a potméter szögének függvényében
        const float brake_rate = (0.00006f + amount * 0.00028f) * (48000.0f / sr);

        for (size_t i = 0; i < num_frames; ++i) {
            cur_dry += dry_step;
            cur_wet += wet_step;
            const float in_sample = channel_data[i];

            st.turntable_buf[st.turntable_write_pos] = in_sample;
            st.turntable_write_pos = (st.turntable_write_pos + 1) % buf_size;

            float vinyl_sample = 0.0f;

            if (is_brake) {
                // Vinyl lassulás 0.0-ig
                st.turntable_speed = std::max(0.0f, st.turntable_speed - brake_rate);
                st.turntable_read_pos += st.turntable_speed;

                while (st.turntable_read_pos >= static_cast<float>(buf_size))
                    st.turntable_read_pos -= static_cast<float>(buf_size);

                const size_t idx0 = static_cast<size_t>(st.turntable_read_pos) % buf_size;
                const size_t idx1 = (idx0 + 1) % buf_size;
                const float frac = st.turntable_read_pos - static_cast<float>(idx0);
                const float raw_sample = st.turntable_buf[idx0] * (1.0f - frac) + st.turntable_buf[idx1] * frac;

                // Ha a lemez megállt (< 0.03x sebesség), lágy koszinuszos némítás lép életbe (zéró DC zúgás)
                float stop_fade = 1.0f;
                if (st.turntable_speed < 0.03f) {
                    stop_fade = st.turntable_speed / 0.03f;
                }
                vinyl_sample = raw_sample * stop_fade;
            } else {
                // Spinback lecsengés
                st.turntable_speed *= 0.99993f;
                st.turntable_decay *= 0.99994f;

                st.turntable_read_pos += st.turntable_speed;
                while (st.turntable_read_pos < 0.0f)
                    st.turntable_read_pos += static_cast<float>(buf_size);
                while (st.turntable_read_pos >= static_cast<float>(buf_size))
                    st.turntable_read_pos -= static_cast<float>(buf_size);

                const size_t idx0 = static_cast<size_t>(st.turntable_read_pos) % buf_size;
                const size_t idx1 = (idx0 + 1) % buf_size;
                const float frac = st.turntable_read_pos - static_cast<float>(idx0);
                const float raw_sample = st.turntable_buf[idx0] * (1.0f - frac) + st.turntable_buf[idx1] * frac;

                vinyl_sample = raw_sample * st.turntable_decay;
            }

            channel_data[i] = in_sample * cur_dry + vinyl_sample * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }

    // =========================================================================
    // 6. 🚀 RAMP DELAY (Barber-Pole Dual-Tap Doppler Siren Riser - Zero Pops)
    // =========================================================================
    if (type == MixerFxType::RAMP_DELAY) {
        const size_t buf_size = st.delay_buf.size();

        // 4-beat gyorsuló rámpa ütem-frekvencia
        const float ramp_freq = (bpm > 30.0 && bpm < 250.0) ? static_cast<float>(bpm / (60.0 * 4.0)) : 0.5f;
        const float ramp_inc = 2.0f * PI * ramp_freq / sr;

        const float send_gain = std::min(amount * 1.30f, 1.0f);
        const float feedback = 0.55f + amount * 0.30f;

        const float target_dry = 1.0f - amount * 0.20f;
        const float target_wet = amount * 1.40f;
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

        // Felfelé pásztázó sávszűrő a sziréna rezonanciájához (400 Hz -> 2600 Hz)
        const float bp_freq = 400.0f + amount * 2200.0f;
        const float alpha_bp = std::clamp(2.0f * PI * bp_freq / sr, 0.01f, 0.95f);

        for (size_t i = 0; i < num_frames; ++i) {
            cur_dry += dry_step;
            cur_wet += wet_step;
            const float in_sample = channel_data[i];

            st.ramp_phase += ramp_inc;
            if (st.ramp_phase >= 2.0f * PI)
                st.ramp_phase -= 2.0f * PI;

            // Két egymást 180°-kal eltolt fázisú tap (Barber-Pole elv)
            const float phase_a = st.ramp_phase;
            float phase_b = st.ramp_phase + PI;
            if (phase_b >= 2.0f * PI)
                phase_b -= 2.0f * PI;

            // Hann súlyozó ablakok: amikor a fázis 0-nál átugrik, a tap súlya 0.0000! Zéró kattanás!
            const float win_a = 0.5f * (1.0f - std::cos(phase_a));
            const float win_b = 0.5f * (1.0f - std::cos(phase_b));

            // Delay idők mindkét taphez (összenyomódó idők -> Doppler emelkedés)
            const float norm_a = phase_a / (2.0f * PI);
            const float sweep_a = 1.0f - 0.75f * amount * norm_a;
            const float dly_time_a = std::clamp(0.50f * beat_sec * sweep_a, 0.02f, 1.5f);
            const float dly_samples_a = dly_time_a * sr;

            const float norm_b = phase_b / (2.0f * PI);
            const float sweep_b = 1.0f - 0.75f * amount * norm_b;
            const float dly_time_b = std::clamp(0.50f * beat_sec * sweep_b, 0.02f, 1.5f);
            const float dly_samples_b = dly_time_b * sr;

            // Tap A olvasás
            float r_pos_a = static_cast<float>(st.delay_write_pos) - dly_samples_a;
            while (r_pos_a < 0.0f) r_pos_a += static_cast<float>(buf_size);
            const size_t idx0_a = static_cast<size_t>(r_pos_a) % buf_size;
            const size_t idx1_a = (idx0_a + 1) % buf_size;
            const float frac_a = r_pos_a - static_cast<float>(idx0_a);
            const float delayed_a = st.delay_buf[idx0_a] * (1.0f - frac_a) + st.delay_buf[idx1_a] * frac_a;

            // Tap B olvasás
            float r_pos_b = static_cast<float>(st.delay_write_pos) - dly_samples_b;
            while (r_pos_b < 0.0f) r_pos_b += static_cast<float>(buf_size);
            const size_t idx0_b = static_cast<size_t>(r_pos_b) % buf_size;
            const size_t idx1_b = (idx0_b + 1) % buf_size;
            const float frac_b = r_pos_b - static_cast<float>(idx0_b);
            const float delayed_b = st.delay_buf[idx0_b] * (1.0f - frac_b) + st.delay_buf[idx1_b] * frac_b;

            // Folytonos összegzés a két ablakozott tapből (win_a + win_b = 1.0)
            const float delayed = delayed_a * win_a + delayed_b * win_b;

            // Sziréna sávszűrés és szaturáció a visszacsatoló ágban
            st.mulholland_bp_p0 += alpha_bp * (delayed - st.mulholland_bp_p0);
            float siren_sig = st.mulholland_bp_p0 * feedback;
            siren_sig = std::clamp(siren_sig, -1.2f, 1.2f);
            siren_sig = siren_sig - 0.333f * siren_sig * siren_sig * siren_sig;

            st.delay_buf[st.delay_write_pos] = in_sample * send_gain + siren_sig;
            st.delay_write_pos = (st.delay_write_pos + 1) % buf_size;

            channel_data[i] = in_sample * cur_dry + delayed * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }

    // =========================================================================
    // 7. 🔥 MULHOLLAND DRIVE (Asymmetric Tube Overdrive + Resonant Feedback Growl)
    // =========================================================================
    if (type == MixerFxType::MULHOLLAND) {
        // Analóg csöves előerősítés: 1.0x .. 4.5x (erőteljes, zenei overdrive karakter)
        const float pre_gain = 1.0f + amount * 3.5f;
        const float feedback = amount * 0.70f;

        // RMS Auto-Gain normalizáló a hangerőrobbanás és a master betorzulás megelőzésére
        const float auto_gain_norm = 1.0f / std::sqrt(1.0f + amount * 2.2f);

        const float target_dry = 1.0f - amount * 0.60f;
        const float target_wet = amount * auto_gain_norm;
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

        // 80 Hz Low-Cut a drive bemeneten (megvédi a szub-basszust a sárosodástól)
        const float alpha_hp = std::clamp(2.0f * PI * 80.0f / sr, 0.005f, 0.90f);

        // Rezonáns sávszűrő frekvencia (350 Hz .. 1800 Hz süvítés)
        const float bp_freq = 350.0f + amount * 1450.0f;
        const float alpha_bp = std::clamp(2.0f * PI * bp_freq / sr, 0.01f, 0.90f);

        for (size_t i = 0; i < num_frames; ++i) {
            cur_dry += dry_step;
            cur_wet += wet_step;
            const float in_sample = channel_data[i];

            // 80 Hz mélyvágás az overdrive áramkör előtt
            st.delay_damp_hp += alpha_hp * (in_sample - st.delay_damp_hp);
            const float drive_input = in_sample - st.delay_damp_hp;

            // Rezonáns visszacsatolt felharmonikus szűrés
            st.mulholland_bp_p0 += alpha_bp * (drive_input - st.mulholland_bp_p0);
            st.mulholland_bp_p1 += alpha_bp * (st.mulholland_bp_p0 - st.mulholland_bp_p1);
            const float resonant_sig = (st.mulholland_bp_p0 - st.mulholland_bp_p1) * 2.0f;

            // Aszimmetrikus analóg csöves telítés (Tube Overdrive)
            const float sat_in = (drive_input * pre_gain) + resonant_sig * feedback;
            float tube_sig = sat_in / (1.0f + std::abs(sat_in) * 0.40f);
            if (tube_sig > 0.0f) {
                // Csöves páros felharmonikus aszimmetria
                tube_sig = tube_sig + 0.22f * tube_sig * tube_sig;
            }
            tube_sig = std::tanh(tube_sig);

            channel_data[i] = in_sample * cur_dry + tube_sig * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }

    // =========================================================================
    // 8. 🎚️ TRANSPOSE STRETCH (Full-Bandwidth Granular Octave Pitch Riser / Dropper)
    // =========================================================================
    if (type == MixerFxType::TRANSPOSE) {
        const size_t buf_size = st.grain_buf.size();

        if (amount < 0.04f) {
            st.grain_phase_a = 0.0f;
            st.grain_phase_b = 0.5f;
            for (size_t i = 0; i < num_frames; ++i) {
                st.grain_buf[st.grain_write_pos] = channel_data[i];
                st.grain_write_pos = (st.grain_write_pos + 1) % buf_size;
            }
            st.prev_dry_gain = 1.0f;
            st.prev_wet_gain = 0.0f;
            return;
        }

        // Constant-power crossfade száraz és transzponált jel között (zéró hangerő-esés!)
        const float target_dry = std::cos(amount * 0.5f * PI);
        const float target_wet = std::sin(amount * 0.5f * PI);
        const float inv_frames = 1.0f / static_cast<float>(num_frames);
        const float dry_step = (target_dry - st.prev_dry_gain) * inv_frames;
        const float wet_step = (target_wet - st.prev_wet_gain) * inv_frames;
        float cur_dry = st.prev_dry_gain;
        float cur_wet = st.prev_wet_gain;

        // Pitch arány: Jobbra = felhangolás +12 félhangig (+1 oktáv, ratio 2.0)
        //              Balra  = lehangolás -12 félhangig (-1 oktáv, ratio 0.5)
        const float semitones = (st.cur_flt_s2 >= 0.0f) ? (amount * 12.0f) : (-amount * 12.0f);
        const float pitch_ratio = std::pow(2.0f, semitones / 12.0f);

        const float phase_rate = (pitch_ratio - 1.0f) / static_cast<float>(GRAIN_SIZE_SAMPLES);

        for (size_t i = 0; i < num_frames; ++i) {
            cur_dry += dry_step;
            cur_wet += wet_step;
            const float in_sample = channel_data[i]; // Tiszta, teljes sávszélességű audió bemenet!

            st.grain_buf[st.grain_write_pos] = in_sample;
            const size_t cur_write = st.grain_write_pos;
            st.grain_write_pos = (st.grain_write_pos + 1) % buf_size;

            st.grain_phase_a += phase_rate;
            while (st.grain_phase_a >= 1.0f) st.grain_phase_a -= 1.0f;
            while (st.grain_phase_a < 0.0f)  st.grain_phase_a += 1.0f;

            st.grain_phase_b += phase_rate;
            while (st.grain_phase_b >= 1.0f) st.grain_phase_b -= 1.0f;
            while (st.grain_phase_b < 0.0f)  st.grain_phase_b += 1.0f;

            // Hann-ablakozott Grain A olvasás
            const float delay_a = st.grain_phase_a * static_cast<float>(GRAIN_SIZE_SAMPLES);
            float r_pos_a = static_cast<float>(cur_write) - delay_a;
            while (r_pos_a < 0.0f) r_pos_a += static_cast<float>(buf_size);
            const size_t idx0_a = static_cast<size_t>(r_pos_a) % buf_size;
            const size_t idx1_a = (idx0_a + 1) % buf_size;
            const float frac_a = r_pos_a - static_cast<float>(idx0_a);
            const float sample_a = st.grain_buf[idx0_a] * (1.0f - frac_a) + st.grain_buf[idx1_a] * frac_a;
            const float win_a = 0.5f * (1.0f - std::cos(2.0f * PI * st.grain_phase_a));

            // Hann-ablakozott Grain B olvasás
            const float delay_b = st.grain_phase_b * static_cast<float>(GRAIN_SIZE_SAMPLES);
            float r_pos_b = static_cast<float>(cur_write) - delay_b;
            while (r_pos_b < 0.0f) r_pos_b += static_cast<float>(buf_size);
            const size_t idx0_b = static_cast<size_t>(r_pos_b) % buf_size;
            const size_t idx1_b = (idx0_b + 1) % buf_size;
            const float frac_b = r_pos_b - static_cast<float>(idx0_b);
            const float sample_b = st.grain_buf[idx0_b] * (1.0f - frac_b) + st.grain_buf[idx1_b] * frac_b;
            const float win_b = 0.5f * (1.0f - std::cos(2.0f * PI * st.grain_phase_b));

            const float pitched = sample_a * win_a + sample_b * win_b;

            channel_data[i] = in_sample * cur_dry + pitched * cur_wet;
        }
        st.prev_dry_gain = target_dry;
        st.prev_wet_gain = target_wet;
        return;
    }
}

