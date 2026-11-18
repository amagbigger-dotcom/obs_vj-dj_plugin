#pragma once

#include <cmath>
#include <algorithm>
#include <array>

struct LfoConfig {
    int waveform = 0;       // 0: Szinusz, 1: Háromszög, 2: Fűrész Fel, 3: Fűrész Le, 4: Négyszög
    int rate_idx = 6;       // 0..10, Default: 6 -> 4 ütem (1 taktus 4/4-ben)
    float depth = 1.0f;     // 0.0 .. 1.0 (0% .. 100%)
    float offset = 0.0f;    // -1.0 .. +1.0
    float phase_deg = 0.0f; // 0.0 .. 360.0 fok
};

inline float getLfoRateBeats(int rate_idx) {
    static constexpr std::array<float, 11> kRates = {
        0.0625f, // 0: 1/16 ütem
        0.125f,  // 1: 1/8 ütem
        0.25f,   // 2: 1/4 ütem
        0.5f,    // 3: 1/2 ütem
        1.0f,    // 4: 1 ütem
        2.0f,    // 5: 2 ütem
        4.0f,    // 6: 4 ütem (1 taktus)
        8.0f,    // 7: 8 ütem (2 taktus)
        16.0f,   // 8: 16 ütem (4 taktus)
        32.0f,   // 9: 32 ütem (8 taktus)
        64.0f    // 10: 64 ütem (16 taktus / frakció)
    };
    if (rate_idx < 0 || rate_idx >= static_cast<int>(kRates.size())) return 4.0f;
    return kRates[rate_idx];
}

inline const char* getLfoRateName(int rate_idx) {
    static constexpr const char* kNames[] = {
        "1/16 Ütem", "1/8 Ütem", "1/4 Ütem", "1/2 Ütem",
        "1 Ütem", "2 Ütem", "4 Ütem (1 Taktus)", "8 Ütem (2 Taktus)",
        "16 Ütem (4 Taktus)", "32 Ütem (8 Taktus)", "64 Ütem (16 Taktus)"
    };
    if (rate_idx < 0 || rate_idx >= 11) return "4 Ütem (1 Taktus)";
    return kNames[rate_idx];
}

inline const char* getLfoWaveformName(int wf_idx) {
    static constexpr const char* kWaveforms[] = {
        "∿ Szinusz (Sine)",
        "▲ Háromszög (Triangle)",
        "◤ Fűrészfog Fel / Jobbra (Saw Up)",
        "◥ Fűrészfog Le / Balra (Saw Down)",
        "■ Négyszög / Kocka (Square)"
    };
    if (wf_idx < 0 || wf_idx >= 5) return "∿ Szinusz (Sine)";
    return kWaveforms[wf_idx];
}

inline float evaluateLfo(const LfoConfig &cfg, double time_sec, double bpm) {
    const float rate_beats = getLfoRateBeats(cfg.rate_idx);
    const double effective_bpm = (bpm > 20.0) ? bpm : 120.0;
    const double beats_per_sec = effective_bpm / 60.0;
    const double total_beats = time_sec * beats_per_sec;
    double phi = std::fmod((total_beats / static_cast<double>(rate_beats)) + (static_cast<double>(cfg.phase_deg) / 360.0), 1.0);
    if (phi < 0.0) phi += 1.0;

    float raw = 0.0f;
    const float fphi = static_cast<float>(phi);
    switch (cfg.waveform) {
    case 0: // Szinusz
        raw = std::sin(2.0f * 3.14159265f * fphi);
        break;
    case 1: // Háromszög
        raw = 2.0f * std::abs(2.0f * (fphi - std::floor(fphi + 0.5f))) - 1.0f;
        break;
    case 2: // Fűrészfog Fel/Jobbra
        raw = 2.0f * (fphi - std::floor(fphi)) - 1.0f;
        break;
    case 3: // Fűrészfog Le/Balra
        raw = 1.0f - 2.0f * (fphi - std::floor(fphi));
        break;
    case 4: // Négyszög/Kocka
        raw = (fphi < 0.5f) ? 1.0f : -1.0f;
        break;
    default:
        raw = std::sin(2.0f * 3.14159265f * fphi);
        break;
    }

    float val = raw * cfg.depth + cfg.offset;
    return std::clamp(val, -1.0f, 1.0f);
}
