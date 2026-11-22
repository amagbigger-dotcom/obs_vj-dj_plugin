#pragma once

#include <cstddef>
#include <cmath>
#include <algorithm>
#include "engine/mixer_fx.hpp"

struct EngineDspState {
    float lp = 0.0f;
    float hp = 0.0f;
    float cur_low = 1.0f;
    float cur_mid = 1.0f;
    float cur_hi = 1.0f;

    MixerFxState fx;
};

struct LiveEnvelopes {
    float amp = 0.0f;
    float bass = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
};

class EngineDsp {
public:
    static void process(float *channel_data,
                        size_t num_frames,
                        size_t channel_idx,
                        size_t sample_rate,
                        float g_low,
                        float g_mid,
                        float g_hi,
                        float f_val,
                        EngineDspState &st,
                        LiveEnvelopes *out_envelopes = nullptr,
                        MixerFxType fx_type = MixerFxType::FILTER,
                        double bpm = 128.0);
};
