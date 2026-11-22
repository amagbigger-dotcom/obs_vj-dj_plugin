#include "engine/engine_dsp.hpp"

void EngineDsp::process(float *channel_data,
                        size_t num_frames,
                        size_t channel_idx,
                        size_t sample_rate,
                        float g_low,
                        float g_mid,
                        float g_hi,
                        float f_val,
                        EngineDspState &st,
                        LiveEnvelopes *out_envelopes,
                        MixerFxType fx_type,
                        double bpm)
{
    if (!channel_data || num_frames == 0)
        return;

    const float sr = (sample_rate > 0) ? static_cast<float>(sample_rate) : 48000.0f;
    const float w_low = 2.0f * 3.14159265f * 250.0f / sr;
    const float alpha_low = std::clamp(w_low / (1.0f + w_low), 0.001f, 0.95f);

    const float w_high = 2.0f * 3.14159265f * 5000.0f / sr;
    const float alpha_high = std::clamp(w_high / (1.0f + w_high), 0.001f, 0.95f);

    // Initialize smoothed values on first block
    if (st.cur_low < 0.001f) st.cur_low = g_low;
    if (st.cur_mid < 0.001f) st.cur_mid = g_mid;
    if (st.cur_hi < 0.001f)  st.cur_hi  = g_hi;

    const float smooth_rate = 0.002f; // ~10 ms smooth time at 48kHz (imperceptible latency, zero crackle)

    float sum_sq_in = 0.0f;
    float sum_sq_low = 0.0f;
    float sum_sq_mid = 0.0f;
    float sum_sq_high = 0.0f;

    // 1. 3-sávos EQ szűrés és szétbontás
    for (size_t i = 0; i < num_frames; ++i) {
        const float in = channel_data[i];

        st.cur_low += (g_low - st.cur_low) * smooth_rate;
        st.cur_mid += (g_mid - st.cur_mid) * smooth_rate;
        st.cur_hi  += (g_hi  - st.cur_hi)  * smooth_rate;

        st.lp += alpha_low * (in - st.lp);
        st.hp += alpha_high * (in - st.hp);

        const float low_band = st.lp;
        const float high_band = in - st.hp;
        const float mid_band = in - low_band - high_band;

        sum_sq_in += in * in;
        sum_sq_low += low_band * low_band;
        sum_sq_mid += mid_band * mid_band;
        sum_sq_high += high_band * high_band;

        channel_data[i] = low_band * st.cur_low + mid_band * st.cur_mid + high_band * st.cur_hi;
    }

    // 2. Traktor S2 Bipoláris DJ Filter + Dedikált Mixer FX (Filter / Delay / Reverb / Flanger)
    MixerFxProcessor::process(channel_data, num_frames, static_cast<size_t>(sr),
                             f_val, fx_type, bpm, st.fx, channel_idx);

    // 3. Kimeneti korlátozás (-1.2 .. +1.2)
    for (size_t i = 0; i < num_frames; ++i) {
        channel_data[i] = std::clamp(channel_data[i], -1.2f, 1.2f);
    }

    // 4. Élő zenei vizualizációs energiaszintek
    if (out_envelopes && channel_idx == 0 && num_frames > 0) {
        const float inv = 1.0f / static_cast<float>(num_frames);
        out_envelopes->amp  = std::clamp(std::sqrt(sum_sq_in   * inv) * 2.5f, 0.0f, 1.0f);
        out_envelopes->bass = std::clamp(std::sqrt(sum_sq_low  * inv) * 3.8f, 0.0f, 1.0f);
        out_envelopes->mid  = std::clamp(std::sqrt(sum_sq_mid  * inv) * 3.2f, 0.0f, 1.0f);
        out_envelopes->high = std::clamp(std::sqrt(sum_sq_high * inv) * 3.0f, 0.0f, 1.0f);
    }
}
