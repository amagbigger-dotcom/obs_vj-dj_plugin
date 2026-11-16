#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

class AnalyzerWaveform {
public:
    struct WaveformResult {
        std::vector<float> overview_peaks;
        std::vector<float> overview_low;
        std::vector<float> overview_mid;
        std::vector<float> overview_high;
        std::vector<float> detailed_peaks;
        std::vector<float> detailed_low;
        std::vector<float> detailed_mid;
        std::vector<float> detailed_high;
        std::vector<float> kick_samples; // 160Hz lowpass for BPM onset
    };

    static WaveformResult generate(const std::vector<float> &mono_samples,
                                   uint32_t sample_rate);
};
