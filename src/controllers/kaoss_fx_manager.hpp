#pragma once

#include <cstdint>
#include <mutex>
#include <atomic>
#include <chrono>

class KaossFxManager {
public:
    static KaossFxManager &instance();

    // Touchpad inputs (CC 12, CC 13, Note 32)
    void onTouch(bool pressed, uint8_t velocity);
    void onTouchX(uint8_t x_val);
    void onTouchY(uint8_t y_val);

    // Rotary Encoder input (CC 31)
    void onEncoderRotate(uint8_t data2);

    // HOLD button
    void onHoldButton(bool pressed);
    void toggleHold();
    void setHold(bool active);
    bool isHoldActive() const;

    // Periodic watchdog timer & parameter smoothing (called at 25 Hz / 40 ms)
    void tick(float delta_sec);

    // Status queries
    int activeDeck() const; // 1 = Deck A, 2 = Deck B
    int activeSlot() const; // 0..3
    float currentParamValue() const;

    // Master reset / kill
    void killActiveEffect();

private:
    KaossFxManager();

    void applyTouchCoordinates();

    mutable std::mutex mutex_;

    // Touch state
    bool touch_down_ = false;
    uint8_t last_x_ = 0;
    uint8_t last_y_ = 0;
    bool has_x_ = false;
    bool has_y_ = false;

    // Active effect & deck selection
    int active_deck_ = 1; // 1 = Deck A, 2 = Deck B
    int active_slot_ = 0; // 0..3
    bool is_effect_engaged_ = false;

    // Parameter & Encoder state
    // current_val_ is in [0.0, 1.0], where 0.50 is neutral (0.0 filter).
    // < 0.50 is Low-Pass Filter (negative / bass / dark tone).
    // > 0.50 is High-Pass Filter (positive / treble / bright tone).
    float current_val_ = 0.50f;
    float idle_timer_ms_ = 0.0f;

    // HOLD state
    std::atomic<bool> is_hold_active_{false};
};
