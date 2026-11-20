#pragma once

#include "controllers/midi_mapping.hpp"
#include "controllers/midi_jog.hpp"
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

private:
    MidiDispatcher();

    mutable std::mutex mutex_;
    MidiControllerProfile profile_;
    MidiJogWheel jog_a_{MidiJogMode::SPREAD_64};
    MidiJogWheel jog_b_{MidiJogMode::SPREAD_64};
    MidiJogWheel jog_browse_{MidiJogMode::BIN_OFFSET};

    bool is_learning_ = false;
    MidiAction learn_action_ = MidiAction::NONE;
    int learn_deck_ = 0;
    LearnCallback learn_cb_;
    MonitorCallback monitor_cb_;
};
