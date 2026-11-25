#pragma once

#include "controllers/midi_mapping.hpp"
#include "controllers/midi_jog.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>

class MidiDispatcher {
public:
    static MidiDispatcher &instance();

    void setProfile(const MidiControllerProfile &p);
    MidiControllerProfile activeProfile() const;

    void startLearn(MidiAction action, int deck);
    void cancelLearn();
    bool isLearning() const { return is_learning_; }

    using LearnCallback = std::function<void(MidiMsgType type, uint8_t ch, uint8_t control, MidiAction action, int deck)>;
    void setLearnCallback(LearnCallback cb) { learn_cb_ = cb; }

    using MonitorCallback = std::function<void(uint8_t status, uint8_t data1, uint8_t data2)>;
    void setMonitorCallback(MonitorCallback cb) { monitor_cb_ = cb; }

    void dispatch(uint8_t status, uint8_t data1, uint8_t data2);
    void sendFeedback(bool force_refresh = false);

    bool isShiftPressed(int deck = 0) const;

private:
    MidiDispatcher();

    mutable std::mutex mutex_;
    MidiControllerProfile profile_;
    MidiJogWheel jog_a_{MidiJogMode::SPREAD_64};
    MidiJogWheel jog_b_{MidiJogMode::SPREAD_64};
    MidiJogWheel jog_browse_{MidiJogMode::BIN_OFFSET};

    std::atomic<bool> shift_a_{false};
    std::atomic<bool> shift_b_{false};
    std::atomic<bool> shift_global_{false};

    int last_strip_a_ = -1;
    int last_strip_b_ = -1;
    std::chrono::steady_clock::time_point last_strip_time_a_{};
    std::chrono::steady_clock::time_point last_strip_time_b_{};

    bool is_learning_ = false;
    MidiAction learn_action_ = MidiAction::NONE;
    int learn_deck_ = 0;
    LearnCallback learn_cb_;
    MonitorCallback monitor_cb_;

    std::unordered_map<uint32_t, uint8_t> last_feedback_sent_;
    uint32_t feedback_blink_counter_{0};
};
