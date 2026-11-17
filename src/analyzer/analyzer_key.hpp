#pragma once

#include <QString>
#include <vector>
#include <cstdint>
#include <cstddef>

class AnalyzerKey {
public:
    struct KeyResult {
        QString key_name;      // pl. "Am", "C", "F#m"
        QString camelot_code;  // pl. "8A", "8B", "11B"
        int key_index = 0;     // 0..11: Major (C..B), 12..23: Minor (Cm..Bm)
        float confidence = 0.0f;
        bool success = false;
    };

    // Elemzi a monó PCM minták tonális kromavektorát (Pitch Class Profile)
    // és meghatározza a zenei hangnemet és a Camelot kódot.
    static KeyResult analyze(const std::vector<float> &mono_samples,
                             uint32_t sample_rate);
};
