#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

class EngineLoop {
public:
    static constexpr int LOOP_BEAT_COUNT = 17;
    static constexpr int DEFAULT_LOOP_INDEX = 6; // 4 beats
    static constexpr int MAX_LOOP_INDEX = 16;

    static double beatMultiplier(int beat_index) noexcept;
    static int64_t calculateLoopLengthMs(double beat_duration_ms, int beat_index) noexcept;
    static int64_t quantizeLoopStart(int64_t current_time_ms, int64_t grid_offset_ms, double beat_duration_ms) noexcept;
    static bool shouldLoopJump(int64_t current_time_ms, int64_t loop_start_ms, int64_t loop_end_ms, int64_t last_jump_ms) noexcept;
};
