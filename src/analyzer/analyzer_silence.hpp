#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

class AnalyzerSilence {
public:
    struct SilenceResult {
        int64_t first_sound_ms = 0;
        int64_t last_sound_ms = 0;
        bool has_sound = false;
    };

    // Pásztázza a monó PCM mintákat a megadott küszöbérték (pl. -60 dBFS) alapján,
    // és meghatározza az első és utolsó hallható hang időkódját ms-ban.
    static SilenceResult analyze(const std::vector<float> &mono_samples,
                                 uint32_t sample_rate,
                                 float threshold_db = -60.0f);
};
