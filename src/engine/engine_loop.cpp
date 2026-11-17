#include "engine/engine_loop.hpp"

double EngineLoop::beatMultiplier(int beat_index) noexcept
{
    static const double BEAT_MULTIPLIERS[] = {
        0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0
    };
    return BEAT_MULTIPLIERS[std::clamp(beat_index, 0, 11)];
}

int64_t EngineLoop::calculateLoopLengthMs(double beat_duration_ms, int beat_index) noexcept
{
    const double mult = beatMultiplier(beat_index);
    return static_cast<int64_t>(std::round(mult * beat_duration_ms));
}

int64_t EngineLoop::quantizeLoopStart(int64_t current_time_ms, int64_t grid_offset_ms, double beat_duration_ms) noexcept
{
    if (beat_duration_ms <= 20.0)
        return current_time_ms;

    const double delta = static_cast<double>(current_time_ms - grid_offset_ms);
    const double nearest_beat_idx = std::round(delta / beat_duration_ms);
    const int64_t quantized = grid_offset_ms + static_cast<int64_t>(std::round(nearest_beat_idx * beat_duration_ms));
    return (quantized >= 0) ? quantized : current_time_ms;
}

bool EngineLoop::shouldLoopJump(int64_t current_time_ms, int64_t loop_start_ms, int64_t loop_end_ms, int64_t last_jump_ms) noexcept
{
    if (loop_end_ms <= loop_start_ms)
        return false;

    // 40ms debounce to prevent re-triggering repeatedly while the player is seeking
    if (current_time_ms >= loop_end_ms && (current_time_ms - last_jump_ms > 40)) {
        return true;
    }
    return false;
}
