#include "analyzer/analyzer_beats.hpp"

#include <cmath>
#include <algorithm>
#include <vector>

AnalyzerBeats::BeatResult AnalyzerBeats::analyze(const std::vector<float> &kick_samples,
                                                 size_t total_samples,
                                                 uint32_t sample_rate)
{
    BeatResult res;
    if (total_samples == 0 || sample_rate == 0 || kick_samples.empty())
        return res;

    // 1. Onset Novelty Curve at ~500 Hz for BPM autocorrelation
    const size_t samples_per_2ms = std::max<size_t>(1, static_cast<size_t>(sample_rate / 500));
    const double onset_rate = static_cast<double>(sample_rate) / static_cast<double>(samples_per_2ms);
    const size_t num_onset_windows = total_samples / samples_per_2ms;
    if (num_onset_windows < 500) {
        return res;
    }

    std::vector<float> kick_energy(num_onset_windows, 0.0f);
    for (size_t w = 0; w < num_onset_windows; ++w) {
        const size_t start = w * samples_per_2ms;
        const size_t end = std::min(start + samples_per_2ms, total_samples);
        double sum_sq = 0.0;
        for (size_t i = start; i < end; ++i) {
            const float val = kick_samples[i];
            sum_sq += val * val;
        }
        const float count = static_cast<float>(end - start);
        kick_energy[w] = (count > 0.0f) ? static_cast<float>(std::sqrt(sum_sq / count)) : 0.0f;
    }

    std::vector<float> novelty(num_onset_windows, 0.0f);
    for (size_t w = 1; w < num_onset_windows; ++w) {
        const float diff = kick_energy[w] - kick_energy[w - 1];
        novelty[w] = (diff > 0.0f) ? diff : 0.0f;
    }

    // 2. Autocorrelation over 65 to 190 BPM with Harmonic Enhancement
    constexpr int kMinLag = 158; // ~190 BPM
    constexpr int kMaxLag = 465; // ~65 BPM
    const int max_search_lag = kMaxLag * 2 + 4;

    std::vector<double> raw_corr(max_search_lag + 1, 0.0);
    const size_t max_novelty_len = std::min<size_t>(novelty.size(), static_cast<size_t>(500 * 240)); // analyze up to 4 minutes

    for (int lag = kMinLag / 2; lag <= max_search_lag; ++lag) {
        if (static_cast<size_t>(lag) >= max_novelty_len) break;
        double sum = 0.0;
        const size_t count = max_novelty_len - lag;
        for (size_t i = 0; i < count; ++i) {
            sum += static_cast<double>(novelty[i]) * static_cast<double>(novelty[i + lag]);
        }
        raw_corr[lag] = (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
    }

    std::vector<double> autocorr(kMaxLag + 2, 0.0);
    int best_lag = 234; // 128 BPM
    double best_score = -1.0;

    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        // Octave harmonic summation reinforces true 4/4 fundamental beat
        double score = raw_corr[lag];
        if (lag * 2 <= max_search_lag) score += 0.4 * raw_corr[lag * 2];
        if (lag / 2 >= kMinLag / 2)   score += 0.25 * raw_corr[lag / 2];

        const double bpm = (60.0 * onset_rate) / static_cast<double>(lag);
        // Széles, kíméletes DJ prior (sigma 65) tie-breakerként, megakadályozva a 90-175 BPM dalok 128-ra kényszerítését
        const double prior = std::exp(-std::pow(bpm - 128.0, 2.0) / (2.0 * 65.0 * 65.0));
        score *= (0.80 + 0.20 * prior);
        autocorr[lag] = score;

        if (score > best_score) {
            best_score = score;
            best_lag = lag;
        }
    }

    // 3. Sub-sample Parabolic Peak Interpolation for 0.05 BPM accuracy
    double fine_lag = static_cast<double>(best_lag);
    if (best_lag > kMinLag && best_lag < kMaxLag) {
        const double y0 = autocorr[best_lag - 1];
        const double y1 = autocorr[best_lag];
        const double y2 = autocorr[best_lag + 1];
        const double denom = 2.0 * (y0 - 2.0 * y1 + y2);
        if (std::abs(denom) > 1e-6) {
            const double delta = (y0 - y2) / denom;
            if (std::abs(delta) < 1.0)
                fine_lag += delta;
        }
    }

    const double detected_bpm = (60.0 * onset_rate) / fine_lag;
    double final_bpm = detected_bpm;
    const double nearest_int = std::round(detected_bpm);
    if (std::abs(detected_bpm - nearest_int) <= 0.42) {
        final_bpm = nearest_int;
    } else {
        const double nearest_half = std::round(detected_bpm * 2.0) / 2.0;
        if (std::abs(detected_bpm - nearest_half) <= 0.12) {
            final_bpm = nearest_half;
        } else {
            final_bpm = std::round(detected_bpm * 10.0) / 10.0;
        }
    }
    res.bpm = final_bpm;

    // 4. Find highest energy drop / kick section for Downbeat (Kick 1) Anchor Alignment
    const int period = std::max(1, static_cast<int>(std::round(fine_lag)));
    
    // Find window with highest kick density across the track
    size_t best_window_start = 0;
    float max_density = -1.0f;
    const size_t search_span = std::min<size_t>(novelty.size(), static_cast<size_t>(500 * 90));
    const size_t chunk_size = 500 * 16; // 16 second window
    for (size_t s = 0; s + chunk_size < search_span; s += (500 * 4)) {
        float chunk_energy = 0.0f;
        for (size_t i = s; i < s + chunk_size; ++i) {
            chunk_energy += novelty[i];
        }
        if (chunk_energy > max_density) {
            max_density = chunk_energy;
            best_window_start = s;
        }
    }

    double best_offset_score = -1.0;
    int best_offset = 0;
    const size_t align_end = std::min(best_window_start + chunk_size, novelty.size() - period);

    for (int o = 0; o < period; ++o) {
        double alignment = 0.0;
        int beats = 0;
        for (size_t idx = best_window_start + o; idx + period < align_end; idx += period) {
            alignment += novelty[idx];
            beats++;
        }
        if (beats > 0)
            alignment /= beats;

        if (alignment > best_offset_score) {
            best_offset_score = alignment;
            best_offset = (best_window_start + o) % period;
        }
    }

    // Convert offset windows to milliseconds (each window is 2ms at 500 Hz)
    res.grid_offset_ms = static_cast<int64_t>(best_offset * 2);
    res.success = (res.bpm > 30.0);
    return res;
}
