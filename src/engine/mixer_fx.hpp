#pragma once

#include <cstddef>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

enum class MixerFxType : int {
    FILTER = 0,         // Sima Filter (Piros / Red)
    DELAY = 1,          // Filter + Delay (Kék / Blue, 3/4 Beat Dub Echo)
    FILTER_DELAY = 1,   // Alias
    REVERB = 2,         // Filter + Reverb (Zöld / Green, 3D Plate/Hall)
    FILTER_REVERB = 2,  // Alias
    FLANGER = 3,        // Filter + Flanger (Sárga / Yellow, 8-Beat Jet Sweep)
    FILTER_FLANGER = 3, // Alias
    BEATMASHER = 4,     // Filter + Beatmasher 2 (Lila / Purple, 1/2..1/32 Rhythmic Stutter)
    TURNTABLE = 5,      // Filter + Turntable FX (Narancs / Orange, Vinyl Brake & Spinback)
    RAMP_DELAY = 6,     // Filter + Ramp Delay (Ciánkék / Cyan, UFO Siren Riser)
    MULHOLLAND = 7,     // Filter + Mulholland Drive (Vérvörös / Crimson, Overdrive & Feedback Growl)
    TRANSPOSE = 8       // Filter + Transpose Stretch (Rózsaszín / Pink, Pitch Riser / Dropper)
};

struct MixerFxState {
    bool initialized = false;

    // Filter állapot (Bipoláris DJ TPT integrátor és 2-fokozatú C1 simító)
    float tpt_s_lp = 0.0f;       // TPT Low-Pass belső integrátor állapot
    float tpt_s_hp = 0.0f;       // TPT High-Pass belső integrátor állapot
    float cur_flt = 0.0f;        // 1. simítási fokozat (simított poti érték)
    float cur_flt_s2 = 0.0f;     // 2. simítási fokozat (C1 folytonos, zéró sebesség-ugrás)
    float prev_dry_gain = 1.0f;  // Mintánkénti lineáris dry gain simítás
    float prev_wet_gain = 0.0f;  // Mintánkénti lineáris wet gain simítás

    // Bemeneti mélyvágó szűrők (Low-Cut az effekt bemeneteken a lábdob bedörgés ellen)
    float delay_send_hp = 0.0f;
    float reverb_send_hp = 0.0f;
    float flanger_fb_hp = 0.0f;

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

    // Beatmasher 2 állapot (96 000 minta = 2.0s körkörös gyűrűpuffer)
    std::vector<float> beatmash_buf;
    size_t beatmash_write_pos = 0;
    size_t beatmash_freeze_anchor = 0;
    float beatmash_playback_pos = 0.0f;
    bool beatmash_active = false;

    // Turntable FX állapot (96 000 minta = 2.0s körkörös gyűrűpuffer)
    std::vector<float> turntable_buf;
    size_t turntable_write_pos = 0;
    float turntable_read_pos = 0.0f;
    float turntable_speed = 1.0f;
    float turntable_decay = 1.0f;
    bool turntable_braking = false;
    bool turntable_spinning = false;
    size_t turntable_spin_anchor = 0;

    // Ramp Delay állapot
    float ramp_phase = 0.0f;
    float ramp_send_hp = 0.0f;
    float ramp_fb_hp = 0.0f;

    // Mulholland Drive állapot (rezonáns sáváteresztő feedback állapot)
    float mulholland_bp_p0 = 0.0f;
    float mulholland_bp_p1 = 0.0f;

    // Transpose Stretch állapot (4096 minta granular grain buffer)
    std::vector<float> grain_buf;
    size_t grain_write_pos = 0;
    float grain_phase_a = 0.0f;
    float grain_phase_b = 0.0f;

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

    static const char *name(MixerFxType type) noexcept;
    static const char *shortName(MixerFxType type) noexcept;
    static const char *colorHex(MixerFxType type) noexcept;
};
