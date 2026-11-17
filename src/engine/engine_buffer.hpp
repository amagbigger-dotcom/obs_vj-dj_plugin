#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

class EngineBuffer {
public:
    static int speedPercentFromBpm(double target_bpm, double base_bpm) noexcept;
    static double effectiveBpmFromSpeed(int speed_percent, double base_bpm) noexcept;
    static bool shouldDebounceSeek(int64_t request_time_ms, int64_t last_seek_time_ms, int min_interval_ms = 40) noexcept;
};
