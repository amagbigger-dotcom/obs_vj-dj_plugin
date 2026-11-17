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

// In-place Radix-2 Cooley-Tukey FFT for complex numbers (re, im)
static void fft2048(std::array<float, 2048> &re, std::array<float, 2048> &im)
{
    constexpr size_t N = 2048;
    size_t j = 0;
    for (size_t i = 0; i < N - 1; ++i) {
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
        size_t k = N / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    for (size_t len = 2; len <= N; len <<= 1) {
        const float angle = -2.0f * PI_F / static_cast<float>(len);
        const float wlen_re = std::cos(angle);
        const float wlen_im = std::sin(angle);
        for (size_t i = 0; i < N; i += len) {
            float w_re = 1.0f;
            float w_im = 0.0f;
            const size_t half = len >> 1;
            for (size_t k = 0; k < half; ++k) {
                const float u_re = re[i + k];
                const float u_im = im[i + k];
                const float v_re = re[i + k + half] * w_re - im[i + k + half] * w_im;
                const float v_im = re[i + k + half] * w_im + im[i + k + half] * w_re;
                re[i + k] = u_re + v_re;
                im[i + k] = u_im + v_im;
                re[i + k + half] = u_re - v_re;
                im[i + k + half] = u_im - v_im;
                const float next_w_re = w_re * wlen_re - w_im * wlen_im;
                w_im = w_re * wlen_im + w_im * wlen_re;
                w_re = next_w_re;
            }
        }
    }
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

    constexpr size_t win_size = 2048;
    const size_t hop_size = 4096; // ~92 ms ugrás 44.1 kHz-en
    const size_t total_samples = mono_samples.size();

    // Hann ablak előszámítása
    std::array<float, win_size> window;
    for (size_t i = 0; i < win_size; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * PI_F * static_cast<float>(i) / static_cast<float>(win_size - 1)));
    }

    std::array<float, 12> total_chroma = {0.0f};

    const float bin_hz = static_cast<float>(sample_rate) / static_cast<float>(win_size);
    // Frekvenciatartomány a harmóniákhoz: 130 Hz (C3) - 2100 Hz (C7)
    // Ez teljesen kizárja a lábdob és szub-basszus energiát (40-100 Hz), ami torzítaná a hangnemet
    const size_t min_bin = std::max<size_t>(1, static_cast<size_t>(130.0f / bin_hz));
    const size_t max_bin = std::min<size_t>(win_size / 2, static_cast<size_t>(2100.0f / bin_hz));

    std::array<float, win_size> re;
    std::array<float, win_size> im;

    for (size_t pos = 0; pos + win_size <= total_samples; pos += hop_size) {
        float frame_energy = 0.0f;
        for (size_t i = 0; i < win_size; ++i) {
            const float s = mono_samples[pos + i] * window[i];
            re[i] = s;
            im[i] = 0.0f;
            frame_energy += s * s;
        }

        // Csendes részek átugrása
        if (frame_energy < 0.001f) continue;

        fft2048(re, im);

        std::array<float, 12> frame_chroma = {0.0f};

        for (size_t k = min_bin; k <= max_bin; ++k) {
            const float freq = static_cast<float>(k) * bin_hz;
            const float mag = std::sqrt(re[k] * re[k] + im[k] * im[k]);

            // Folyamatos MIDI hangmagasság (A4 = 440 Hz -> MIDI 69)
            const float pitch = 12.0f * (std::log(freq / 440.0f) / 0.69314718056f) + 69.0f;
            const int note = static_cast<int>(std::round(pitch));
            const int p = ((note % 12) + 12) % 12;

            const float dist = std::abs(pitch - static_cast<float>(note));
            if (dist < 0.5f) {
                // Koszinuszos interpoláció a hangmagasság pontosságáért
                const float w = 0.5f * (1.0f + std::cos(PI_F * dist / 0.5f));
                // Spektrális fehérítés és logaritmikus kompresszió
                frame_chroma[p] += w * std::log1p(10.0f * mag);
            }
        }

        // Képkockánkénti normalizálás (per-frame whitening)
        // Így a halk énekes / szintis részek ugyanolyan súllyal számítanak, mint a hangos zúzások
        float f_sum_sq = 0.0f;
        for (float v : frame_chroma) f_sum_sq += v * v;
        if (f_sum_sq > 1e-6f) {
            const float inv_f = 1.0f / std::sqrt(f_sum_sq);
            for (int p = 0; p < 12; ++p) {
                total_chroma[p] += frame_chroma[p] * inv_f;
            }
        }
    }

    // Normalizálás L2 normával
    float sum_sq = 0.0f;
    for (float v : total_chroma) sum_sq += v * v;
    if (sum_sq > 1e-6f) {
        const float inv_norm = 1.0f / std::sqrt(sum_sq);
        for (float &v : total_chroma) v *= inv_norm;
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
        const float corr = pearsonCorrelation(total_chroma, rotated_profile);
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
        const float corr = pearsonCorrelation(total_chroma, rotated_profile);
        if (corr > best_corr) {
            best_corr = corr;
            best_key_idx = root;
            is_minor = true;
        }
    }

    result.success = (best_corr > 0.05f);
    result.confidence = best_corr;
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
