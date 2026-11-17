#include "analyzer/analyzer_waveform.hpp"

#include <cmath>
#include <algorithm>
#include <vector>

namespace {
constexpr size_t kOverviewBuckets = 500;
}

AnalyzerWaveform::WaveformResult AnalyzerWaveform::generate(const std::vector<float> &mono_samples,
                                                           uint32_t sample_rate)
{
    WaveformResult res;
    const size_t total_samples = mono_samples.size();
    if (total_samples == 0 || sample_rate == 0)
        return res;

    // 1. 3-Band Frequency Separation & Kick Punch Filter
    // Low: < 250 Hz (Bass / Kick drum)
    // Mid: 250 Hz - 2500 Hz (Vocals / Synths / Snares)
    // High: > 2500 Hz (Cymbals / Hats / Air)
    const float fc_low = 250.0f;
    const float alpha_l = std::clamp(2.0f * 3.14159f * fc_low / static_cast<float>(sample_rate), 0.001f, 0.99f);
    const float fc_mid = 2500.0f;
    const float alpha_m = std::clamp(2.0f * 3.14159f * fc_mid / static_cast<float>(sample_rate), 0.001f, 0.99f);
    const float fc_kick = 160.0f;
    const float alpha_k = std::clamp(2.0f * 3.14159f * fc_kick / static_cast<float>(sample_rate), 0.001f, 0.99f);

    float lp_l1 = 0.0f, lp_l2 = 0.0f;
    float lp_m1 = 0.0f, lp_m2 = 0.0f;
    float lp_k1 = 0.0f, lp_k2 = 0.0f;

    std::vector<float> low_band(total_samples, 0.0f);
    std::vector<float> mid_band(total_samples, 0.0f);
    std::vector<float> high_band(total_samples, 0.0f);
    res.kick_samples.assign(total_samples, 0.0f);

    float max_kick_all = 0.001f;
    float max_full_all = 0.001f;
    float max_l_all = 0.001f;
    float max_m_all = 0.001f;
    float max_h_all = 0.001f;

    for (size_t i = 0; i < total_samples; ++i) {
        const float in = mono_samples[i];

        // 160 Hz kick punch filter for onset/BPM
        lp_k1 += alpha_k * (in - lp_k1);
        lp_k2 += alpha_k * (lp_k1 - lp_k2);
        res.kick_samples[i] = lp_k2;

        // 250 Hz Lowpass
        lp_l1 += alpha_l * (in - lp_l1);
        lp_l2 += alpha_l * (lp_l1 - lp_l2);
        const float l_val = lp_l2;

        // 2500 Hz Lowpass
        lp_m1 += alpha_m * (in - lp_m1);
        lp_m2 += alpha_m * (lp_m1 - lp_m2);
        const float m_lowpass = lp_m2;

        const float m_val = m_lowpass - l_val;
        const float h_val = in - m_lowpass;

        low_band[i] = l_val;
        mid_band[i] = m_val;
        high_band[i] = h_val;

        const float abs_k = std::abs(lp_k2);
        const float abs_f = std::abs(in);
        const float abs_l = std::abs(l_val);
        const float abs_m = std::abs(m_val);
        const float abs_h = std::abs(h_val);

        if (abs_k > max_kick_all) max_kick_all = abs_k;
        if (abs_f > max_full_all) max_full_all = abs_f;
        if (abs_l > max_l_all) max_l_all = abs_l;
        if (abs_m > max_m_all) max_m_all = abs_m;
        if (abs_h > max_h_all) max_h_all = abs_h;
    }

    // 2. 3-Band Overview Waveform (500 points with spectral separation)
    res.overview_peaks.resize(kOverviewBuckets, 0.05f);
    res.overview_low.resize(kOverviewBuckets, 0.05f);
    res.overview_mid.resize(kOverviewBuckets, 0.05f);
    res.overview_high.resize(kOverviewBuckets, 0.05f);
    const size_t samples_per_bucket = std::max<size_t>(1, total_samples / kOverviewBuckets);

    std::vector<float> bucket_max(kOverviewBuckets, 0.0f);
    std::vector<float> bucket_l(kOverviewBuckets, 0.0f);
    std::vector<float> bucket_m(kOverviewBuckets, 0.0f);
    std::vector<float> bucket_h(kOverviewBuckets, 0.0f);
    float max_l_b = 0.001f, max_m_b = 0.001f, max_h_b = 0.001f, max_peak_b = 0.001f;

    for (size_t b = 0; b < kOverviewBuckets; ++b) {
        const size_t start = b * samples_per_bucket;
        const size_t end = std::min(start + samples_per_bucket, total_samples);
        const size_t count = std::max<size_t>(1, end - start);

        float peak_val = 0.0f;
        float sum_l = 0.0f, sum_m = 0.0f, sum_h = 0.0f;
        for (size_t i = start; i < end; ++i) {
            peak_val = std::max(peak_val, std::abs(mono_samples[i]));
            sum_l += std::abs(low_band[i]);
            sum_m += std::abs(mid_band[i]);
            sum_h += std::abs(high_band[i]);
        }
        bucket_max[b] = peak_val;
        bucket_l[b] = sum_l / static_cast<float>(count);
        bucket_m[b] = sum_m / static_cast<float>(count);
        bucket_h[b] = sum_h / static_cast<float>(count);

        if (peak_val > max_peak_b) max_peak_b = peak_val;
        if (bucket_l[b] > max_l_b) max_l_b = bucket_l[b];
        if (bucket_m[b] > max_m_b) max_m_b = bucket_m[b];
        if (bucket_h[b] > max_h_b) max_h_b = bucket_h[b];
    }

    const float max_band_energy = std::max({max_l_b, max_m_b, max_h_b, 0.001f});
    for (size_t b = 0; b < kOverviewBuckets; ++b) {
        res.overview_peaks[b] = std::clamp(bucket_max[b] / max_peak_b, 0.05f, 1.0f);
        res.overview_low[b]   = std::clamp(bucket_l[b] / max_band_energy, 0.01f, 1.0f);
        res.overview_mid[b]   = std::clamp(bucket_m[b] / max_band_energy, 0.01f, 1.0f);
        res.overview_high[b]  = std::clamp(bucket_h[b] / max_band_energy, 0.01f, 1.0f);
    }

    // 2b. Lassú, zenei makro-struktúrát és fokozódást követő szűrő (Macro-Tonus & Structural Build-up Follower)
    // 8 bucket-os (~5.5 másodperc, 2-3 zenei ütem) simítás, ami kiszűri az egyes dobütések rángatózását,
    // és a zene valódi strukturális dinamikáját, felépítését (intro -> kiállás -> build-up -> drop) mutatja
    constexpr int kSmoothRadius = 8;
    std::vector<float> kernel(2 * kSmoothRadius + 1);
    float k_sum = 0.0f;
    for (int r = -kSmoothRadius; r <= kSmoothRadius; ++r) {
        const float w = std::exp(-static_cast<float>(r * r) / (2.0f * 3.5f * 3.5f));
        kernel[r + kSmoothRadius] = w;
        k_sum += w;
    }
    for (float &w : kernel) w /= k_sum;

    std::vector<float> smooth_l(kOverviewBuckets, 0.0f);
    std::vector<float> smooth_m(kOverviewBuckets, 0.0f);
    std::vector<float> smooth_h(kOverviewBuckets, 0.0f);

    for (int b = 0; b < static_cast<int>(kOverviewBuckets); ++b) {
        float sl = 0.0f, sm = 0.0f, sh = 0.0f;
        for (int r = -kSmoothRadius; r <= kSmoothRadius; ++r) {
            const int idx = std::clamp(b + r, 0, static_cast<int>(kOverviewBuckets - 1));
            const float kw = kernel[r + kSmoothRadius];
            sl += res.overview_low[idx] * kw;
            sm += res.overview_mid[idx] * kw;
            sh += res.overview_high[idx] * kw;
        }
        smooth_l[b] = sl;
        smooth_m[b] = sm;
        smooth_h[b] = sh;
    }

    res.overview_low = smooth_l;
    res.overview_mid = smooth_m;
    res.overview_high = smooth_h;

    res.overview_tonus.resize(kOverviewBuckets, 0.5f);
    float max_tonus = 0.001f;
    std::vector<float> raw_tonus(kOverviewBuckets, 0.0f);
    for (size_t b = 0; b < kOverviewBuckets; ++b) {
        // Tónus = Zenei feszültség: magas és közép frekvenciás telítettség a mély energiával kombinálva
        const float t_val = smooth_l[b] * 0.7f + smooth_m[b] * 1.3f + smooth_h[b] * 1.5f;
        raw_tonus[b] = t_val;
        if (t_val > max_tonus) max_tonus = t_val;
    }
    for (size_t b = 0; b < kOverviewBuckets; ++b) {
        res.overview_tonus[b] = std::clamp(raw_tonus[b] / max_tonus, 0.05f, 1.0f);
    }

    // 3. Ultra-High-Resolution Detailed Waveform (100 points/sec, 10ms window)
    const size_t samples_per_10ms = std::max<size_t>(1, static_cast<size_t>(sample_rate / 100));
    const size_t num_detailed = total_samples / samples_per_10ms;
    res.detailed_peaks.resize(num_detailed, 0.05f);
    res.detailed_low.resize(num_detailed, 0.05f);
    res.detailed_mid.resize(num_detailed, 0.05f);
    res.detailed_high.resize(num_detailed, 0.05f);

    for (size_t w = 0; w < num_detailed; ++w) {
        const size_t start = w * samples_per_10ms;
        const size_t end = std::min(start + samples_per_10ms, total_samples);

        float peak_kick = 0.0f;
        float peak_full = 0.0f;
        float peak_l = 0.0f;
        float peak_m = 0.0f;
        float peak_h = 0.0f;
        for (size_t i = start; i < end; ++i) {
            peak_kick = std::max(peak_kick, std::abs(res.kick_samples[i]));
            peak_full = std::max(peak_full, std::abs(mono_samples[i]));
            peak_l = std::max(peak_l, std::abs(low_band[i]));
            peak_m = std::max(peak_m, std::abs(mid_band[i]));
            peak_h = std::max(peak_h, std::abs(high_band[i]));
        }

        const float norm_k = peak_kick / max_kick_all;
        const float norm_f = peak_full / max_full_all;
        const float env = 0.10f * norm_f + 0.90f * std::pow(norm_k, 2.2f);
        res.detailed_peaks[w] = std::clamp(env, 0.05f, 1.0f);
        res.detailed_low[w] = std::clamp(peak_l / max_l_all, 0.05f, 1.0f);
        res.detailed_mid[w] = std::clamp(peak_m / max_m_all, 0.05f, 1.0f);
        res.detailed_high[w] = std::clamp(peak_h / max_h_all, 0.05f, 1.0f);
    }

    res.snare_samples = std::move(mid_band);
    return res;
}
