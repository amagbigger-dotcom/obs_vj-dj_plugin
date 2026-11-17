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
        std::vector<float> overview_tonus; // Lassú makro-struktúra zenei tónus és fokozódás [0.0 .. 1.0]
        std::vector<float> detailed_peaks;
        std::vector<float> detailed_low;
        std::vector<float> detailed_mid;
        std::vector<float> detailed_high;
        std::vector<float> kick_samples;  // 160Hz lowpass for Kick BPM onset
        std::vector<float> snare_samples; // 250Hz - 2500Hz bandpass for Snare/Clap onset (Broken beat support)
    };

    static WaveformResult generate(const std::vector<float> &mono_samples,
                                   uint32_t sample_rate);
};
