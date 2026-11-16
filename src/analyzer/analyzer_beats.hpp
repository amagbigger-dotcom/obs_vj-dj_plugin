#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

class AnalyzerBeats {
public:
    struct BeatResult {
        double bpm = 128.0;
        int64_t grid_offset_ms = 0;
        bool success = false;
    };

    static BeatResult analyze(const std::vector<float> &kick_samples,
                              size_t total_samples,
                              uint32_t sample_rate);
};
