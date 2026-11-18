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
                        LiveEnvelopes *out_envelopes)
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

    for (size_t i = 0; i < num_frames; ++i) {
        const float in = channel_data[i];

        // Smooth parameters per sample to eliminate all zipper noise & crackle
        st.cur_low += (g_low - st.cur_low) * smooth_rate;
        st.cur_mid += (g_mid - st.cur_mid) * smooth_rate;
        st.cur_hi  += (g_hi  - st.cur_hi)  * smooth_rate;
        st.cur_flt += (f_val - st.cur_flt) * smooth_rate;

        st.lp += alpha_low * (in - st.lp);
        st.hp += alpha_high * (in - st.hp);

        const float low_band = st.lp;
        const float high_band = in - st.hp;
        const float mid_band = in - low_band - high_band;

        sum_sq_in += in * in;
        sum_sq_low += low_band * low_band;
        sum_sq_mid += mid_band * mid_band;
        sum_sq_high += high_band * high_band;

        float out = low_band * st.cur_low + mid_band * st.cur_mid + high_band * st.cur_hi;

        // DJ Filter: continuous tracking prevents stale DC offsets and pops
        if (st.cur_flt < -0.001f) {
            // Low Pass: cutoff from ~18000 Hz down to 200 Hz
            const float norm = 1.0f + st.cur_flt;
            const float cutoff = 200.0f + norm * norm * 17800.0f;
            const float a_lp = std::clamp(2.0f * 3.14159265f * cutoff / sr, 0.001f, 0.95f);
            st.dj_lp += a_lp * (out - st.dj_lp);
            st.dj_hp += 0.05f * (0.0f - st.dj_hp);
            out = st.dj_lp;
        } else if (st.cur_flt > 0.001f) {
            // High Pass: cutoff from ~20 Hz up to 8000 Hz
            const float norm = st.cur_flt;
            const float cutoff = 20.0f + norm * norm * 7980.0f;
            const float a_hp = std::clamp(2.0f * 3.14159265f * cutoff / sr, 0.001f, 0.95f);
            st.dj_hp += a_hp * (out - st.dj_hp);
            st.dj_lp += 0.05f * (out - st.dj_lp);
            out = out - st.dj_hp;
        } else {
            st.dj_lp += 0.05f * (out - st.dj_lp);
            st.dj_hp += 0.05f * (0.0f - st.dj_hp);
        }

        channel_data[i] = std::clamp(out, -1.2f, 1.2f);
    }

    if (out_envelopes && channel_idx == 0 && num_frames > 0) {
        const float inv = 1.0f / static_cast<float>(num_frames);
        out_envelopes->amp  = std::clamp(std::sqrt(sum_sq_in   * inv) * 2.5f, 0.0f, 1.0f);
        out_envelopes->bass = std::clamp(std::sqrt(sum_sq_low  * inv) * 3.8f, 0.0f, 1.0f);
        out_envelopes->mid  = std::clamp(std::sqrt(sum_sq_mid  * inv) * 3.2f, 0.0f, 1.0f);
        out_envelopes->high = std::clamp(std::sqrt(sum_sq_high * inv) * 3.0f, 0.0f, 1.0f);
    }
}
