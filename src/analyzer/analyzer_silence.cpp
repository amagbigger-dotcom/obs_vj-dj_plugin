#include "analyzer/analyzer_silence.hpp"

#include <cmath>
#include <algorithm>

AnalyzerSilence::SilenceResult AnalyzerSilence::analyze(const std::vector<float> &mono_samples,
                                                       uint32_t sample_rate,
                                                       float threshold_db)
{
    SilenceResult result;
    if (mono_samples.empty() || sample_rate == 0) {
        return result;
    }

    const float threshold_linear = std::pow(10.0f, threshold_db / 20.0f);
    const size_t total_samples = mono_samples.size();
    const size_t block_size = 256; // ~5.8 ms 44.1 kHz-en

    size_t first_idx = total_samples;
    size_t last_idx = 0;

    // 1. Elejétől pásztázás az első hallható leütésig / hangig
    for (size_t i = 0; i < total_samples; i += block_size) {
        const size_t cur_block = std::min(block_size, total_samples - i);
        float peak = 0.0f;
        for (size_t j = 0; j < cur_block; ++j) {
            const float a = std::abs(mono_samples[i + j]);
            if (a > peak) peak = a;
        }

        if (peak >= threshold_linear) {
            // Megtaláltuk a blokkot, finomhangoljuk a pontos minta-indexre
            for (size_t j = 0; j < cur_block; ++j) {
                if (std::abs(mono_samples[i + j]) >= threshold_linear) {
                    first_idx = i + j;
                    break;
                }
            }
            break;
        }
    }

    if (first_idx >= total_samples) {
        // A teljes sáv csend (nem érte el a küszöböt)
        result.has_sound = false;
        result.first_sound_ms = 0;
        result.last_sound_ms = 0;
        return result;
    }

    // 2. Végétől pásztázás visszafelé az utolsó hangig
    size_t end_scan = (total_samples / block_size) * block_size;
    if (end_scan >= total_samples) {
        end_scan = total_samples - block_size;
    }

    for (int64_t i = static_cast<int64_t>(end_scan); i >= 0; i -= block_size) {
        const size_t cur_block = std::min(block_size, total_samples - static_cast<size_t>(i));
        float peak = 0.0f;
        for (size_t j = 0; j < cur_block; ++j) {
            const float a = std::abs(mono_samples[static_cast<size_t>(i) + j]);
            if (a > peak) peak = a;
        }

        if (peak >= threshold_linear) {
            for (int64_t j = static_cast<int64_t>(cur_block) - 1; j >= 0; --j) {
                if (std::abs(mono_samples[static_cast<size_t>(i) + static_cast<size_t>(j)]) >= threshold_linear) {
                    last_idx = static_cast<size_t>(i) + static_cast<size_t>(j);
                    break;
                }
            }
            break;
        }
    }

    result.has_sound = true;
    result.first_sound_ms = static_cast<int64_t>((static_cast<uint64_t>(first_idx) * 1000ULL) / sample_rate);
    result.last_sound_ms = static_cast<int64_t>((static_cast<uint64_t>(last_idx) * 1000ULL) / sample_rate);

    return result;
}
