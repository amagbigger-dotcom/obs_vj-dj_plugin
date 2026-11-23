#include "controllers/kaoss_fx_manager.hpp"
#include "engine/deck_engine.hpp"
#include "engine/mixer_fx.hpp"
#include <algorithm>
#include <cmath>

KaossFxManager &KaossFxManager::instance()
{
    static KaossFxManager s_instance;
    return s_instance;
}

KaossFxManager::KaossFxManager()
{
}

bool KaossFxManager::isCurrentSlotBipolar() const
{
    DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
    if (!dp) return true;
    const int fx_type = dp->slotFxType(active_slot_);
    return (fx_type == static_cast<int>(MixerFxType::FILTER));
}

void KaossFxManager::onTouch(bool pressed, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (pressed && velocity > 0) {
        touch_down_ = true;
        touch_pending_ = true;
        touch_triggered_lock_ = false;
        touch_down_time_ = std::chrono::steady_clock::now();
    } else {
        touch_down_ = false;
        touch_pending_ = false;
        touch_triggered_lock_ = false;
    }
}

void KaossFxManager::onTouchX(uint8_t x_val)
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_x_ = x_val;
    if (touch_pending_ && !touch_triggered_lock_) {
        evaluateTouchZone();
    }
}

void KaossFxManager::onTouchY(uint8_t y_val)
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_y_ = y_val;
    if (touch_pending_ && !touch_triggered_lock_) {
        evaluateTouchZone();
    }
}

void KaossFxManager::evaluateTouchZone()
{
    // Race condition guard: evaluate strictly once per physical touch down
    touch_triggered_lock_ = true;
    touch_pending_ = false;

    // 2 Columns x 4 Rows:
    // Left column: X < 64 -> Deck A (target_deck = 1)
    // Right column: X >= 64 -> Deck B (target_deck = 2)
    const int target_deck = (last_x_ >= 64) ? 2 : 1;

    // Y is 0..127. Top row is Slot 0, Bottom row is Slot 3
    // Row 0 (Top): Y 96..127 -> Slot 0
    // Row 1: Y 64..95 -> Slot 1
    // Row 2: Y 32..63 -> Slot 2
    // Row 3 (Bottom): Y 0..31 -> Slot 3
    const int target_slot = 3 - std::clamp(static_cast<int>(last_y_) / 32, 0, 3);

    DeckPlayer *dp = (target_deck == 2) ? vj_deck_b() : vj_deck_a();
    if (!dp) return;

    // Kill switch / Toggle OFF: Re-tapping the currently active zone bypasses/clears effect
    if (is_effect_engaged_ && target_deck == active_deck_ && target_slot == active_slot_) {
        killActiveEffect();
        return;
    }

    // Activate new zone
    active_deck_ = target_deck;
    active_slot_ = target_slot;
    is_effect_engaged_ = true;

    // Select the slot on the hardware deck
    dp->selectSlot(target_slot);

    // Initialize current parameter value to neutral based on slot type
    const bool bipolar = isCurrentSlotBipolar();
    current_val_ = bipolar ? 0.50f : 0.0f;
    dp->setFilterVal(0.0f);

    idle_timer_ms_ = 0.0f;
}

void KaossFxManager::killActiveEffect()
{
    DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
    if (dp) {
        dp->setFilterVal(0.0f);
    }
    const bool bipolar = isCurrentSlotBipolar();
    current_val_ = bipolar ? 0.50f : 0.0f;
    is_effect_engaged_ = false;
    is_hold_active_.store(false, std::memory_order_relaxed);
    idle_timer_ms_ = 0.0f;
}

void KaossFxManager::onEncoderRotate(uint8_t data2)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Relative delta decoding:
    // CCW: 127 (-1), 126 (-2), 125 (-3)...
    // CW: 1 (+1), 2 (+2), 3 (+3)...
    const int delta = (data2 < 64) ? static_cast<int>(data2) : (static_cast<int>(data2) - 128);
    if (delta == 0) return;

    // Movement resets watchdog timer
    idle_timer_ms_ = 0.0f;
    is_effect_engaged_ = true;

    // Dynamic acceleration
    const float abs_delta = static_cast<float>(std::abs(delta));
    const float accel = (abs_delta > 1.0f) ? (1.0f + 0.35f * (abs_delta - 1.0f)) : 1.0f;
    const float step = static_cast<float>(delta) * 0.025f * accel;

    const bool bipolar = isCurrentSlotBipolar();
    DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
    if (!dp) return;

    if (bipolar) {
        // Bipolar Combo Filter [0.0 .. 1.0], center = 0.50
        current_val_ = std::clamp(current_val_ + step, 0.0f, 1.0f);

        // Magnetic snap-to-center in [0.48, 0.52]
        if (current_val_ >= 0.48f && current_val_ <= 0.52f) {
            current_val_ = 0.50f;
        }

        // Apply bipolar filter to deck (-1.0 to +1.0)
        const float bipolar_val = (current_val_ - 0.50f) * 2.0f;
        dp->setFilterVal(bipolar_val);
    } else {
        // Unipolar FX [0.0 .. 1.0], neutral = 0.0
        current_val_ = std::clamp(current_val_ + step, 0.0f, 1.0f);
        dp->setFilterVal(current_val_);
    }
}

void KaossFxManager::onHoldButton(bool pressed)
{
    if (pressed) {
        toggleHold();
    }
}

void KaossFxManager::toggleHold()
{
    std::lock_guard<std::mutex> lock(mutex_);
    const bool new_state = !is_hold_active_.load(std::memory_order_relaxed);
    is_hold_active_.store(new_state, std::memory_order_relaxed);

    // If HOLD turned OFF, reset parameter back to neutral
    if (!new_state) {
        killActiveEffect();
    }
}

bool KaossFxManager::isHoldActive() const
{
    return is_hold_active_.load(std::memory_order_relaxed);
}

void KaossFxManager::tick(float delta_sec)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // If HOLD is active, watchdog is suspended
    if (is_hold_active_.load(std::memory_order_relaxed)) {
        return;
    }

    // Accumulate idle time
    idle_timer_ms_ += delta_sec * 1000.0f;

    // If idle time exceeds 200 ms, auto-reset parameter to neutral with smooth exponential lerp
    if (idle_timer_ms_ >= 200.0f) {
        const bool bipolar = isCurrentSlotBipolar();
        const float neutral = bipolar ? 0.50f : 0.0f;

        if (std::abs(current_val_ - neutral) > 0.005f) {
            // Smooth lerp decay towards neutral (~8.0 decay coefficient)
            const float factor = 1.0f - std::exp(-8.0f * delta_sec);
            current_val_ += (neutral - current_val_) * factor;

            DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
            if (dp) {
                if (bipolar) {
                    dp->setFilterVal((current_val_ - 0.50f) * 2.0f);
                } else {
                    dp->setFilterVal(current_val_);
                }
            }
        } else if (current_val_ != neutral) {
            current_val_ = neutral;
            DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
            if (dp) {
                dp->setFilterVal(0.0f);
            }
            is_effect_engaged_ = false;
        }
    }
}

int KaossFxManager::activeDeck() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_deck_;
}

int KaossFxManager::activeSlot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_slot_;
}

float KaossFxManager::currentParamValue() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return current_val_;
}
