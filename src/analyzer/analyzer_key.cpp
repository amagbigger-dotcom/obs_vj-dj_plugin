#include "analyzer/analyzer_key.hpp"

#include <cmath>
#include <array>
#include <vector>
#include <algorithm>

namespace {

static constexpr float PI_F = 3.14159265358979323846f;

// 12 alaphang neve
static const char *kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Camelot kódok leképzése a 12 dúr és 12 moll hangnemhez
static const char *kCamelotMajor[12] = {
    "8B",  // C
    "3B",  // C#
    "10B", // D
    "5B",  // D#
    "12B", // E
    "7B",  // F
    "2B",  // F#
    "9B",  // G
    "4B",  // G#
    "11B", // A
    "6B",  // A#
    "1B"   // B
};

static const char *kCamelotMinor[12] = {
    "5A",  // Cm
    "12A", // C#m
    "7A",  // Dm
    "2A",  // D#m
    "9A",  // Em
    "4A",  // Fm
    "11A", // F#m
    "6A",  // Gm
    "1A",  // G#m
    "8A",  // Am
    "3A",  // A#m
    "10A"  // Bm
};

// Krumhansl-Schmuckler tonális profilok (Major és Minor)
static const std::array<float, 12> kProfileMajor = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};

static const std::array<float, 12> kProfileMinor = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

// Goertzel algoritmus DFT energiához adott célfrekvencián
inline float goertzelEnergy(const float *samples, size_t n, float target_freq, float sr)
{
    const float omega = 2.0f * PI_F * target_freq / sr;
    const float coeff = 2.0f * std::cos(omega);
    float s_prev = 0.0f;
    float s_prev2 = 0.0f;

    for (size_t i = 0; i < n; ++i) {
        const float s = samples[i] + coeff * s_prev - s_prev2;
        s_prev2 = s_prev;
        s_prev = s;
    }

    return std::max(0.0f, s_prev * s_prev + s_prev2 * s_prev2 - coeff * s_prev * s_prev2);
}

// Pearson korreláció két 12 elemű profil között
inline float pearsonCorrelation(const std::array<float, 12> &x, const std::array<float, 12> &y)
{
    float mean_x = 0.0f;
    float mean_y = 0.0f;
    for (int i = 0; i < 12; ++i) {
        mean_x += x[i];
        mean_y += y[i];
    }
    mean_x /= 12.0f;
    mean_y /= 12.0f;

    float num = 0.0f;
    float denom_x = 0.0f;
    float denom_y = 0.0f;
    for (int i = 0; i < 12; ++i) {
        const float dx = x[i] - mean_x;
        const float dy = y[i] - mean_y;
        num += dx * dy;
        denom_x += dx * dx;
        denom_y += dy * dy;
    }

    const float denom = std::sqrt(denom_x * denom_y);
    if (denom <= 1e-9f) return 0.0f;
    return num / denom;
}

} // namespace

AnalyzerKey::KeyResult AnalyzerKey::analyze(const std::vector<float> &mono_samples,
                                           uint32_t sample_rate)
{
    KeyResult result;
    if (mono_samples.size() < 2048 || sample_rate < 8000) {
        return result;
    }

    // 1. Frekvenciatáblázat generálása (12 hang * 4 oktáv: Octave 2..5, 65 Hz - 1000 Hz)
    // A domináns harmóniák a zenei basszus és középtartományban találhatók a legtisztábban
    std::array<std::array<float, 4>, 12> note_frequencies;
    for (int p = 0; p < 12; ++p) {
        for (int oct = 2; oct <= 5; ++oct) {
            // A4 = 440 Hz (p = 9, oct = 4)
            const float semi_offset = static_cast<float>(p - 9) + static_cast<float>(oct - 4) * 12.0f;
            note_frequencies[p][oct - 2] = 440.0f * std::pow(2.0f, semi_offset / 12.0f);
        }
    }

    // 2. Kroma (Pitch Class Profile) vektor akkumulálása
    std::array<float, 12> chroma = {0.0f};

    const size_t win_size = 2048;
    const size_t hop_size = 4096; // ~92 ms ugrás 44.1 kHz-en
    const size_t total_samples = mono_samples.size();

    // Hanning ablak előszámítása
    std::vector<float> window(win_size);
    for (size_t i = 0; i < win_size; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * PI_F * static_cast<float>(i) / static_cast<float>(win_size - 1)));
    }

    std::vector<float> frame(win_size);

    for (size_t pos = 0; pos + win_size <= total_samples; pos += hop_size) {
        float frame_energy = 0.0f;
        for (size_t i = 0; i < win_size; ++i) {
            frame[i] = mono_samples[pos + i] * window[i];
            frame_energy += frame[i] * frame[i];
        }

        // Csendes részek átugrása
        if (frame_energy < 0.001f) continue;

        for (int p = 0; p < 12; ++p) {
            float p_energy = 0.0f;
            for (int oct_idx = 0; oct_idx < 4; ++oct_idx) {
                const float freq = note_frequencies[p][oct_idx];
                p_energy += goertzelEnergy(frame.data(), win_size, freq, static_cast<float>(sample_rate));
            }
            // Logaritmikus tömörítés
            chroma[p] += std::log1pf(p_energy);
        }
    }

    // Normalizálás L2 normával
    float sum_sq = 0.0f;
    for (float v : chroma) sum_sq += v * v;
    if (sum_sq > 1e-6f) {
        const float inv_norm = 1.0f / std::sqrt(sum_sq);
        for (float &v : chroma) v *= inv_norm;
    } else {
        return result;
    }

    // 3. Korreláció vizsgálata mind a 24 lehetséges hangnemre
    float best_corr = -1.0f;
    int best_key_idx = 0;
    bool is_minor = false;

    // Dúr hangnemek vizsgálata (12 eltolás)
    for (int root = 0; root < 12; ++root) {
        std::array<float, 12> rotated_profile;
        for (int i = 0; i < 12; ++i) {
            rotated_profile[(i + root) % 12] = kProfileMajor[i];
        }
        const float corr = pearsonCorrelation(chroma, rotated_profile);
        if (corr > best_corr) {
            best_corr = corr;
            best_key_idx = root;
            is_minor = false;
        }
    }

    // Moll hangnemek vizsgálata (12 eltolás)
    for (int root = 0; root < 12; ++root) {
        std::array<float, 12> rotated_profile;
        for (int i = 0; i < 12; ++i) {
            rotated_profile[(i + root) % 12] = kProfileMinor[i];
        }
        const float corr = pearsonCorrelation(chroma, rotated_profile);
        if (corr > best_corr) {
            best_corr = corr;
            best_key_idx = root;
            is_minor = true;
        }
    }

    result.success = (best_corr > 0.1f);
    result.confidence = best_corr;

    if (is_minor) {
        result.key_index = 12 + best_key_idx;
        result.key_name = QString::fromUtf8(kNoteNames[best_key_idx]) + QStringLiteral("m");
        result.camelot_code = QString::fromUtf8(kCamelotMinor[best_key_idx]);
    } else {
        result.key_index = best_key_idx;
        result.key_name = QString::fromUtf8(kNoteNames[best_key_idx]);
        result.camelot_code = QString::fromUtf8(kCamelotMajor[best_key_idx]);
    }

    return result;
}
