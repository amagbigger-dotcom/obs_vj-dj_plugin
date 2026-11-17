#include "engine/engine_buffer.hpp"

int EngineBuffer::speedPercentFromBpm(double target_bpm, double base_bpm) noexcept
{
    if (base_bpm <= 1.0)
        return 100;
    const double ratio = target_bpm / base_bpm;
    const int spd = static_cast<int>(std::round(ratio * 100.0));
    return std::clamp(spd, 50, 150);
}

double EngineBuffer::effectiveBpmFromSpeed(int speed_percent, double base_bpm) noexcept
{
    const double base = (base_bpm > 1.0) ? base_bpm : 128.0;
    return base * (static_cast<double>(speed_percent) / 100.0);
}

bool EngineBuffer::shouldDebounceSeek(int64_t request_time_ms, int64_t last_seek_time_ms, int min_interval_ms) noexcept
{
    return (std::abs(request_time_ms - last_seek_time_ms) < min_interval_ms);
}
