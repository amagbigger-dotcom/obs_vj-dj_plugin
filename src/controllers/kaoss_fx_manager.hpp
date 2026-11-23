#pragma once

#include <cstdint>
#include <mutex>
#include <atomic>
#include <chrono>

class KaossFxManager {
public:
    static KaossFxManager &instance();

    // Touchpad inputs
    void onTouch(bool pressed, uint8_t velocity);
    void onTouchX(uint8_t x_val);
    void onTouchY(uint8_t y_val);

    // Rotary Encoder input
    void onEncoderRotate(uint8_t data2);

    // HOLD button
    void onHoldButton(bool pressed);
    void toggleHold();
    bool isHoldActive() const;

    // Periodic watchdog timer & parameter smoothing (e.g. called from 25 Hz / 40 ms loop)
    void tick(float delta_sec);

    // Status queries
    int activeDeck() const; // 1 = Deck A, 2 = Deck B
    int activeSlot() const; // 0..3
    float currentParamValue() const;

    // Master reset / kill
    void killActiveEffect();

private:
    KaossFxManager();

    void evaluateTouchZone();
    bool isCurrentSlotBipolar() const;

    mutable std::mutex mutex_;

    // Touch state & race condition guard
    bool touch_down_ = false;
    bool touch_pending_ = false;
    bool touch_triggered_lock_ = false;
    uint8_t last_x_ = 0;
    uint8_t last_y_ = 0;
    std::chrono::steady_clock::time_point touch_down_time_{};

    // Active effect & deck selection
    int active_deck_ = 1; // Default Deck A
    int active_slot_ = 0; // Default Slot 0 (Filter)
    bool is_effect_engaged_ = false;

    // Parameter & Encoder state
    float current_val_ = 0.5f; // [0.0, 1.0], 0.5 is neutral for bipolar filter
    float idle_timer_ms_ = 0.0f;

    // HOLD state
    std::atomic<bool> is_hold_active_{false};
};
