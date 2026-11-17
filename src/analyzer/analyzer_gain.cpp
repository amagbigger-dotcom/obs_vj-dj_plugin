#include "analyzer/analyzer_gain.hpp"

#include <cmath>
#include <vector>
#include <algorithm>

namespace {

// K-súlyozó biquad szűrő együtthatók (ITU-R BS.1770)
struct BiquadCoeffs {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
};

// High-shelf előszűrő (Stage 1: ~1.5 kHz, +4 dB)
BiquadCoeffs calcStage1(float sr)
{
    BiquadCoeffs c;
    const double db = 3.99984385397;
    const double f0 = 1681.9744509555319;
    const double Q = 0.7071752369274193;
    const double K = std::tan(3.14159265358979323846 * f0 / sr);
    const double Vh = std::pow(10.0, db / 20.0);
    const double Vb = std::pow(Vh, 0.499666774155);

    const double a0 = 1.0 + K / Q + K * K;
    c.b0 = static_cast<float>((Vh + Vb * K / Q + K * K) / a0);
    c.b1 = static_cast<float>(2.0 * (K * K - Vh) / a0);
    c.b2 = static_cast<float>((Vh - Vb * K / Q + K * K) / a0);
    c.a1 = static_cast<float>(2.0 * (K * K - 1.0) / a0);
    c.a2 = static_cast<float>((1.0 - K / Q + K * K) / a0);
    return c;
}

// High-pass szűrő (Stage 2: ~38 Hz)
BiquadCoeffs calcStage2(float sr)
{
    BiquadCoeffs c;
    const double f0 = 38.13547087613982;
    const double Q = 0.5003270373253953;
    const double K = std::tan(3.14159265358979323846 * f0 / sr);

    const double a0 = 1.0 + K / Q + K * K;
    c.b0 = static_cast<float>(1.0 / a0);
    c.b1 = static_cast<float>(-2.0 / a0);
    c.b2 = static_cast<float>(1.0 / a0);
    c.a1 = static_cast<float>(2.0 * (K * K - 1.0) / a0);
    c.a2 = static_cast<float>((1.0 - K / Q + K * K) / a0);
    return c;
}

inline float processBiquad(float in, const BiquadCoeffs &c, float &w1, float &w2)
{
    const float w0 = in - c.a1 * w1 - c.a2 * w2;
    const float out = c.b0 * w0 + c.b1 * w1 + c.b2 * w2;
    w2 = w1;
    w1 = w0;
    return out;
}

} // namespace

AnalyzerGain::GainResult AnalyzerGain::analyze(const std::vector<float> &mono_samples,
                                              uint32_t sample_rate)
{
    GainResult result;
    if (mono_samples.empty() || sample_rate < 8000) {
        return result;
    }

    const float sr = static_cast<float>(sample_rate);
    const BiquadCoeffs stage1 = calcStage1(sr);
    const BiquadCoeffs stage2 = calcStage2(sr);

    float s1_w1 = 0.0f, s1_w2 = 0.0f;
    float s2_w1 = 0.0f, s2_w2 = 0.0f;

    float peak = 0.0f;
    const size_t total = mono_samples.size();
    std::vector<float> filtered(total);

    for (size_t i = 0; i < total; ++i) {
        const float s = mono_samples[i];
        const float a = std::abs(s);
        if (a > peak) peak = a;

        const float y1 = processBiquad(s, stage1, s1_w1, s1_w2);
        const float y2 = processBiquad(y1, stage2, s2_w1, s2_w2);
        filtered[i] = y2;
    }

    result.peak_amplitude = peak;

    // 400 ms blokkok 75% átfedéssel (100 ms ugrás)
    const size_t block_len = static_cast<size_t>(sample_rate * 0.400f);
    const size_t hop_len = static_cast<size_t>(sample_rate * 0.100f);

    if (total < block_len || block_len == 0 || hop_len == 0) {
        result.integrated_lufs = -14.0f;
        result.replay_gain_db = 0.0f;
        result.success = true;
        return result;
    }

    std::vector<double> block_powers;
    for (size_t pos = 0; pos + block_len <= total; pos += hop_len) {
        double sum_sq = 0.0;
        for (size_t i = 0; i < block_len; ++i) {
            const double v = filtered[pos + i];
            sum_sq += v * v;
        }
        const double ms = sum_sq / static_cast<double>(block_len);
        block_powers.push_back(ms);
    }

    if (block_powers.empty()) {
        result.success = false;
        return result;
    }

    // 1. Abszolút kapuzás (-70 LUFS: z_j > 10^(-7.0))
    const double abs_thresh = std::pow(10.0, -7.0);
    double abs_sum = 0.0;
    size_t abs_count = 0;

    for (double p : block_powers) {
        if (p > abs_thresh) {
            abs_sum += p;
            abs_count++;
        }
    }

    if (abs_count == 0) {
        result.integrated_lufs = -70.0f;
        result.replay_gain_db = 0.0f;
        result.success = true;
        return result;
    }

    // 2. Relatív kapuzás (Gamma_r = -0.691 + 10 log10(abs_mean) - 10 dB)
    const double abs_mean = abs_sum / static_cast<double>(abs_count);
    const double rel_thresh = abs_mean * 0.1; // -10 dB rel

    double final_sum = 0.0;
    size_t final_count = 0;

    for (double p : block_powers) {
        if (p > rel_thresh && p > abs_thresh) {
            final_sum += p;
            final_count++;
        }
    }

    if (final_count > 0) {
        const double final_mean = final_sum / static_cast<double>(final_count);
        const double lufs = -0.691 + 10.0 * std::log10(std::max(1e-12, final_mean));
        result.integrated_lufs = static_cast<float>(lufs);
        // Cél: -14.0 LUFS standard
        result.replay_gain_db = static_cast<float>(-14.0 - lufs);
        result.success = true;
    } else {
        result.integrated_lufs = -14.0f;
        result.replay_gain_db = 0.0f;
        result.success = true;
    }

    return result;
}
