#include "engine/mixer_fx.hpp"

namespace {

constexpr float PI = 3.14159265358979323846f;

// Prím fésűszűrő és allpass méretek 48 kHz-re optimalizálva
// Jobb csatornán +23 minta eltolás a gazdag sztereó szélességhez
constexpr std::array<size_t, 4> COMB_LENGTHS_L = {1422, 1491, 1557, 1617};
constexpr std::array<size_t, 4> COMB_LENGTHS_R = {1445, 1514, 1580, 1640};

constexpr std::array<size_t, 2> ALLPASS_LENGTHS_L = {556, 441};
constexpr std::array<size_t, 2> ALLPASS_LENGTHS_R = {579, 464};

} // namespace

void MixerFxState::init(size_t sample_rate, size_t channel_idx)
{
    const size_t sr = (sample_rate > 0) ? sample_rate : 48000;

    // 1. Delay puffer: 2.0 másodperc előrefoglalás (zéró heap allokáció a render ciklusban)
    const size_t max_delay = sr * 2;
    delay_buf.assign(max_delay, 0.0f);
    delay_write_pos = 0;
    delay_damp_lp = 0.0f;
    delay_damp_hp = 0.0f;
    cur_delay_samples = static_cast<float>(sr) * 0.35156f;

    // 2. Reverb comb és allpass pufferek
    const auto &comb_lens = (channel_idx == 1) ? COMB_LENGTHS_R : COMB_LENGTHS_L;
    for (size_t i = 0; i < 4; ++i) {
        // Skálázás mintavételezési frekvenciához (48kHz bázis)
        const size_t len = (comb_lens[i] * sr) / 48000;
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

    // 3. Flanger puffer (1024 minta bőségesen elég a max 10ms pásztázáshoz)
    flanger_buf.assign(1024, 0.0f);
    flanger_write_pos = 0;
    flanger_lfo_phase = 0.0f;

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

    for (size_t i = 0; i < 4; ++i) {
        std::fill(comb_buf[i].begin(), comb_buf[i].end(), 0.0f);
        comb_pos[i] = 0;
        comb_filter[i] = 0.0f;
    }

    for (size_t i = 0; i < 2; ++i) {
        std::fill(allpass_buf[i].begin(), allpass_buf[i].end(), 0.0f);
        allpass_pos[i] = 0;
    }

    std::fill(flanger_buf.begin(), flanger_buf.end(), 0.0f);
    flanger_write_pos = 0;
    flanger_lfo_phase = 0.0f;

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
            // Low Pass: cutoff 18 000 Hz-ről le egészen 50 Hz-ig (igazi mély szub-basszusig)
            const float norm = 1.0f + st.cur_flt;
            const float cutoff = 50.0f + norm * norm * 17950.0f;
            const float a_lp = std::clamp(2.0f * PI * cutoff / sr, 0.001f, 0.95f);
            st.dj_lp += a_lp * (out - st.dj_lp);
            st.dj_hp += 0.05f * (0.0f - st.dj_hp);
            out = st.dj_lp;
        } else if (st.cur_flt > 0.001f) {
            // High Pass: cutoff 20 Hz-ről fel max ~2400 Hz-re (~70% zenei energiavágás,
            // nem némítja el a zenét 100%-nál, az ének és magasak tiszták maradnak az effekthez)
            const float norm = st.cur_flt;
            const float cutoff = 20.0f + norm * norm * 2400.0f;
            const float a_hp = std::clamp(2.0f * PI * cutoff / sr, 0.001f, 0.95f);
            st.dj_hp += a_hp * (out - st.dj_hp);
            st.dj_lp += 0.05f * (out - st.dj_lp);
            out = out - st.dj_hp;
        } else {
            // Bypassed / flat átmenet DC offset csillapítással
            st.dj_lp += 0.05f * (out - st.dj_lp);
            st.dj_hp += 0.05f * (0.0f - st.dj_hp);
        }

        channel_data[i] = out;
    }

    const float amount = std::clamp(std::abs(st.cur_flt), 0.0f, 1.0f);

    // Ha az alap 🔴 Sima Filter aktív, kész vagyunk
    if (type == MixerFxType::FILTER) {
        return;
    }

    // =========================================================================
    // 2. 🔵 Kék: Filter + Delay (Tempo-Synced Dub Echo & Tail Spillover)
    // =========================================================================
    if (type == MixerFxType::FILTER_DELAY) {
        // Cél késleltetési idő: 3/4 ütem (Dotted 8th) BPM szinkronnal
        float target_delay_sec = 0.35156f; // 128 BPM alapértelmezett (351.5 ms)
        if (bpm > 30.0 && bpm < 250.0) {
            target_delay_sec = static_cast<float>((60.0 / bpm) * 0.75);
        }
        target_delay_sec = std::clamp(target_delay_sec, 0.05f, 1.5f);
        const float target_samples = target_delay_sec * sr;

        // Finom delay-idő simítás
        st.cur_delay_samples += (target_samples - st.cur_delay_samples) * 0.001f;

        // Erőteljes, zenei jelenlét és feedback gerjedésvédelemmel (max 0.78)
        const float send_gain = amount * 0.90f;
        const float feedback = 0.40f + amount * 0.38f; // 0.40 .. 0.78
        const float dry_gain = 1.0f - amount * 0.20f;
        const float wet_gain = amount * 1.50f;

        // Feedback sávszűrő (analóg szalagcsillapítás: 4200 Hz LPF, 140 Hz HPF)
        const float alpha_damp_lp = std::clamp(2.0f * PI * 4200.0f / sr, 0.01f, 0.95f);
        const float alpha_damp_hp = std::clamp(2.0f * PI * 140.0f / sr, 0.01f, 0.95f);

        const size_t buf_size = st.delay_buf.size();

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

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

            // Lágy telítés (Soft saturation: x - 1/3 x^3) -> gerjedés kizárva
            fb_sig = std::clamp(fb_sig, -1.2f, 1.2f);
            fb_sig = fb_sig - 0.333f * fb_sig * fb_sig * fb_sig;

            // Beírás a pufferbe: bemenet send + visszacsatolt lecsengés
            st.delay_buf[st.delay_write_pos] = in_sample * send_gain + fb_sig * feedback;
            st.delay_write_pos = (st.delay_write_pos + 1) % buf_size;

            // Kimeneti keverék: dinamikus száraz jel + hangsúlyos visszhang
            channel_data[i] = in_sample * dry_gain + delayed * wet_gain;
        }
        return;
    }

    // =========================================================================
    // 3. 🟢 Zöld: Filter + Reverb (4-Comb + 2-AllPass Freeverb & Tail Spillover)
    // =========================================================================
    if (type == MixerFxType::FILTER_REVERB) {
        // Erőteljes zengés küldés és tágas tér (room size max 0.88, zéró gerjedés)
        const float send_gain = amount * 0.85f;
        const float room_size = 0.72f + amount * 0.16f; // 0.72 .. 0.88
        const float damp = 0.22f;                        // Magashang elnyelés
        const float dry_gain = 1.0f - amount * 0.20f;
        const float wet_gain = amount * 1.50f;

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];
            const float send_sample = in_sample * send_gain;

            float comb_sum = 0.0f;
            for (size_t c = 0; c < 4; ++c) {
                const size_t len = st.comb_buf[c].size();
                const float out_comb = st.comb_buf[c][st.comb_pos[c]];
                st.comb_filter[c] = out_comb * (1.0f - damp) + st.comb_filter[c] * damp;
                st.comb_buf[c][st.comb_pos[c]] = send_sample + st.comb_filter[c] * room_size;
                st.comb_pos[c] = (st.comb_pos[c] + 1) % len;
                comb_sum += out_comb;
            }

            float ap_sig = comb_sum * 0.25f;
            for (size_t a = 0; a < 2; ++a) {
                const size_t len = st.allpass_buf[a].size();
                const float buf_out = st.allpass_buf[a][st.allpass_pos[a]];
                const float ap_in = ap_sig;
                st.allpass_buf[a][st.allpass_pos[a]] = ap_in + buf_out * 0.5f;
                ap_sig = buf_out - ap_in * 0.5f;
                st.allpass_pos[a] = (st.allpass_pos[a] + 1) % len;
            }

            channel_data[i] = in_sample * dry_gain + ap_sig * wet_gain;
        }
        return;
    }

    // =========================================================================
    // 4. 🟡 Sárga: Filter + Flanger (Stereo 90° Quadrature Jet Sweep)
    // =========================================================================
    if (type == MixerFxType::FILTER_FLANGER) {
        const float dry_gain = 1.0f - amount * 0.15f;
        const float wet_gain = amount * 1.30f;
        const float feedback = 0.45f + amount * 0.35f; // 0.45 .. 0.80 (stabil, erőteljes Jet)

        // LFO frekvencia: ütemhez igazított 2 taktusos (8 ütem) söprés, vagy ~0.25 Hz
        float lfo_freq = 0.25f;
        if (bpm > 30.0 && bpm < 250.0) {
            lfo_freq = static_cast<float>(bpm / (60.0 * 8.0));
        }
        const float phase_inc = 2.0f * PI * lfo_freq / sr;

        // Késleltetési tartomány: bázis 1.5 ms (72 minta), modulációs mélység max 2.5 ms (120 minta)
        const float base_samples = 0.0015f * sr;
        const float depth_samples = 0.0025f * sr * (0.5f + amount * 0.5f);

        const size_t buf_size = st.flanger_buf.size();

        for (size_t i = 0; i < num_frames; ++i) {
            const float in_sample = channel_data[i];

            st.flanger_lfo_phase += phase_inc;
            if (st.flanger_lfo_phase > 2.0f * PI)
                st.flanger_lfo_phase -= 2.0f * PI;

            // Sztereó kvadratúra: jobb csatornán +90° fáziseltolás a tágas 3D térhatásért
            const float ch_phase = st.flanger_lfo_phase + ((channel_idx == 1) ? 1.5707963f : 0.0f);
            const float lfo = 0.5f * (1.0f + std::sin(ch_phase));
            const float cur_delay = base_samples + depth_samples * lfo;

            float r_pos = static_cast<float>(st.flanger_write_pos) - cur_delay;
            while (r_pos < 0.0f) r_pos += static_cast<float>(buf_size);
            const size_t idx0 = static_cast<size_t>(r_pos) % buf_size;
            const size_t idx1 = (idx0 + 1) % buf_size;
            const float frac = r_pos - static_cast<float>(static_cast<size_t>(r_pos));
            const float flg_delayed = st.flanger_buf[idx0] * (1.0f - frac) + st.flanger_buf[idx1] * frac;

            st.flanger_buf[st.flanger_write_pos] = std::clamp(in_sample + flg_delayed * feedback, -1.2f, 1.2f);
            st.flanger_write_pos = (st.flanger_write_pos + 1) % buf_size;

            channel_data[i] = in_sample * dry_gain + flg_delayed * wet_gain;
        }
        return;
    }
}
