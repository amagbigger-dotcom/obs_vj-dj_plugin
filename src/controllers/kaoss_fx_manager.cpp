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

void KaossFxManager::onTouch(bool pressed, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(mutex_);
    touch_down_ = (pressed && velocity > 0);
    if (touch_down_) {
        applyTouchCoordinates();
    }
}

void KaossFxManager::onTouchX(uint8_t x_val)
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_x_ = x_val;
    has_x_ = true;
    applyTouchCoordinates();
}

void KaossFxManager::onTouchY(uint8_t y_val)
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_y_ = y_val;
    has_y_ = true;
    applyTouchCoordinates();
}

void KaossFxManager::applyTouchCoordinates()
{
    // 2 Columns:
    // Left column: X < 64 -> Deck A (target = 1)
    // Right column: X >= 64 -> Deck B (target = 2)
    const int target_deck = (last_x_ >= 64) ? 2 : 1;

    // 4 Rows vertically (fentről lefelé 4 slot):
    // Row 0 (Fent): Y in [96..127] -> Slot 0
    // Row 1: Y in [64..95] -> Slot 1
    // Row 2: Y in [32..63] -> Slot 2
    // Row 3 (Lent): Y in [0..31] -> Slot 3
    const int target_slot = 3 - std::clamp(static_cast<int>(last_y_) / 32, 0, 3);

    // If already targeting this deck and slot, avoid reselecting slot and resetting parameter while dragging
    if (target_deck == active_deck_ && target_slot == active_slot_ && is_effect_engaged_) {
        return;
    }

    active_deck_ = target_deck;
    active_slot_ = target_slot;
    is_effect_engaged_ = true;

    DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
    if (dp) {
        dp->selectSlot(active_slot_);
        // Read current deck filter value and sync encoder position:
        // dp->filterVal() is in [-1.0, +1.0]. Map to [0.0, 1.0] where 0.50 is center.
        current_val_ = std::clamp((dp->filterVal() + 1.0f) * 0.50f, 0.0f, 1.0f);
    }
    idle_timer_ms_ = 0.0f;
}

void KaossFxManager::onSamplerPad(int pad_index)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Pad 0 (Note 74): Top-Left -> Deck A, Slot 0
    // Pad 1 (Note 75): Top-Right -> Deck B, Slot 0
    // Pad 2 (Note 76): Bottom-Left -> Deck A, Slot 2
    // Pad 3 (Note 77): Bottom-Right -> Deck B, Slot 2
    const int target_deck = (pad_index % 2 == 1) ? 2 : 1;
    const int target_slot = (pad_index < 2) ? 0 : 2;

    if (target_deck == active_deck_ && target_slot == active_slot_ && is_effect_engaged_) {
        return;
    }

    active_deck_ = target_deck;
    active_slot_ = target_slot;
    is_effect_engaged_ = true;

    DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
    if (dp) {
        dp->selectSlot(active_slot_);
        current_val_ = std::clamp((dp->filterVal() + 1.0f) * 0.50f, 0.0f, 1.0f);
    }
    idle_timer_ms_ = 0.0f;
}

void KaossFxManager::killActiveEffect()
{
    DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
    if (dp) {
        dp->setFilterVal(0.0f);
    }
    current_val_ = 0.50f;
    is_effect_engaged_ = false;
    is_hold_active_.store(false, std::memory_order_relaxed);
    idle_timer_ms_ = 0.0f;
}

void KaossFxManager::onEncoderRotate(uint8_t data2)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Relative delta decoding:
    // CW (right): 1, 2, 3...
    // CCW (left): 127 (-1), 126 (-2), 125 (-3)...
    const int delta = (data2 < 64) ? static_cast<int>(data2) : (static_cast<int>(data2) - 128);
    if (delta == 0) return;

    // Movement resets watchdog timer
    idle_timer_ms_ = 0.0f;
    is_effect_engaged_ = true;

    // Dynamic acceleration
    const float abs_delta = static_cast<float>(std::abs(delta));
    const float accel = (abs_delta > 1.0f) ? (1.0f + 0.35f * (abs_delta - 1.0f)) : 1.0f;
    const float step = static_cast<float>(delta) * 0.025f * accel;

    // Universal Bipolar Control for all 4 slots [-1.0 .. +1.0]:
    // Left (0.50 -> 0.00): Low-pass filter / dark tone / negative value [-1.0, 0.0]
    // Right (0.50 -> 1.00): High-pass filter / bright tone / positive value [0.0, +1.0]
    current_val_ = std::clamp(current_val_ + step, 0.0f, 1.0f);

    // Magnetic snap-to-center in [0.48, 0.52] -> snaps cleanly to 0.50 (neutral bypass)
    if (current_val_ >= 0.48f && current_val_ <= 0.52f) {
        current_val_ = 0.50f;
    }

    DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
    if (dp) {
        const float bipolar_val = (current_val_ - 0.50f) * 2.0f;
        dp->setFilterVal(bipolar_val);
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

void KaossFxManager::setHold(bool active)
{
    std::lock_guard<std::mutex> lock(mutex_);
    is_hold_active_.store(active, std::memory_order_relaxed);
    if (!active) {
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

    // If HOLD is active, watchdog is suspended: keep parameter strictly locked!
    if (is_hold_active_.load(std::memory_order_relaxed)) {
        return;
    }

    // Accumulate idle time
    idle_timer_ms_ += delta_sec * 1000.0f;

    // 500 ms idle window gives comfortable musical pause before auto-resetting
    if (idle_timer_ms_ >= 500.0f) {
        const float neutral = 0.50f;

        if (std::abs(current_val_ - neutral) > 0.005f) {
            // Smooth lerp decay towards neutral (~6.0 decay coefficient)
            const float factor = 1.0f - std::exp(-6.0f * delta_sec);
            current_val_ += (neutral - current_val_) * factor;

            DeckPlayer *dp = (active_deck_ == 2) ? vj_deck_b() : vj_deck_a();
            if (dp) {
                dp->setFilterVal((current_val_ - 0.50f) * 2.0f);
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
