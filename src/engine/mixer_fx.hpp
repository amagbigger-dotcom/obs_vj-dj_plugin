#pragma once

#include <cstddef>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

enum class MixerFxType : int {
    FILTER = 0,         // Sima Filter (Piros / Red LED)
    FILTER_DELAY = 1,   // Filter + Delay (Kék / Blue LED)
    FILTER_REVERB = 2,  // Filter + Reverb (Zöld / Green LED)
    FILTER_FLANGER = 3  // Filter + Flanger (Sárga / Yellow LED)
};

struct MixerFxState {
    bool initialized = false;

    // Filter állapot (DJ Low-Pass és High-Pass IIR)
    float dj_lp = 0.0f;
    float dj_hp = 0.0f;
    float cur_flt = 0.0f;

    // Delay állapot (96 000 minta = 2.0s @ 48kHz körkörös gyűrűpuffer)
    std::vector<float> delay_buf;
    size_t delay_write_pos = 0;
    float delay_damp_lp = 0.0f;
    float delay_damp_hp = 0.0f;
    float cur_delay_samples = 16875.0f; // ~351.5 ms @ 48kHz (128 BPM 3/4 beat)

    // Reverb állapot (Freeverb-stílusú 4 párhuzamos comb + 2 soros allpass)
    std::array<std::vector<float>, 4> comb_buf;
    std::array<size_t, 4> comb_pos = {0, 0, 0, 0};
    std::array<float, 4> comb_filter = {0.0f, 0.0f, 0.0f, 0.0f};

    std::array<std::vector<float>, 2> allpass_buf;
    std::array<size_t, 2> allpass_pos = {0, 0};
    float reverb_lfo_phase = 0.0f; // Modulációs fázis a fésűs fémes csengés kiküszöbölésére

    // Flanger állapot (1024 minta késleltető buffer + LFO fázis)
    std::vector<float> flanger_buf;
    size_t flanger_write_pos = 0;
    float flanger_lfo_phase = 0.0f;

    void init(size_t sample_rate, size_t channel_idx);
    void reset();
};

class MixerFxProcessor {
public:
    static void process(float *channel_data,
                        size_t num_frames,
                        size_t sample_rate,
                        float target_flt,
                        MixerFxType type,
                        double bpm,
                        MixerFxState &st,
                        size_t channel_idx);
};
