#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

class AnalyzerGain {
public:
    struct GainResult {
        float replay_gain_db = 0.0f;  // Ajánlott korrekció dB-ben a -14 LUFS célhoz
        float peak_amplitude = 1.0f;   // Maximális csúcsérték (0.0 .. 1.0+)
        float integrated_lufs = -14.0f; // Mért integrált hangerő
        bool success = false;
    };

    // EBU R128 / BS.1770 K-szűrt integrált hangerő és ReplayGain elemzés
    static GainResult analyze(const std::vector<float> &mono_samples,
                              uint32_t sample_rate);
};
