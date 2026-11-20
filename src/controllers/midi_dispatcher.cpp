#include "controllers/midi_dispatcher.hpp"
#include "engine/deck_engine.hpp"
#include "gui/tracklist_dock.hpp"
#include <algorithm>

MidiDispatcher &MidiDispatcher::instance()
{
    static MidiDispatcher s_inst;
    return s_inst;
}

MidiDispatcher::MidiDispatcher()
{
    // Default to Traktor S2 MK3
    profile_ = MidiControllerProfile::createTraktorS2MK3();
}

void MidiDispatcher::setProfile(const MidiControllerProfile &p)
{
    std::lock_guard<std::mutex> lock(mutex_);
    profile_ = p;
}

MidiControllerProfile MidiDispatcher::activeProfile() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return profile_;
}

void MidiDispatcher::startLearn(MidiAction action, int deck)
{
    std::lock_guard<std::mutex> lock(mutex_);
    is_learning_ = true;
    learn_action_ = action;
    learn_deck_ = deck;
}

void MidiDispatcher::cancelLearn()
{
    std::lock_guard<std::mutex> lock(mutex_);
    is_learning_ = false;
    learn_action_ = MidiAction::NONE;
}

void MidiDispatcher::dispatch(uint8_t status, uint8_t data1, uint8_t data2)
{
    const uint8_t msg_type_nibble = status & 0xF0U;
    const uint8_t channel = (status & 0x0FU) + 1; // 1-based channel

    MidiMsgType msg_type = MidiMsgType::CC;
    if (msg_type_nibble == 0xB0U) {
        msg_type = MidiMsgType::CC;
    } else if (msg_type_nibble == 0x90U) {
        msg_type = (data2 > 0) ? MidiMsgType::NOTE_ON : MidiMsgType::NOTE_OFF;
    } else if (msg_type_nibble == 0x80U) {
        msg_type = MidiMsgType::NOTE_OFF;
    } else if (msg_type_nibble == 0xE0U) {
        msg_type = MidiMsgType::PITCH_BEND;
    } else {
        return; // Ignore clock/sysex
    }

    std::unique_lock<std::mutex> lock(mutex_);

    if (monitor_cb_) {
        monitor_cb_(status, data1, data2);
    }

    // 1. MIDI Learn Mode
    if (is_learning_) {
        if (msg_type == MidiMsgType::CC || msg_type == MidiMsgType::NOTE_ON) {
            MidiControlBinding b;
            b.msg_type = msg_type;
            b.channel = channel;
            b.control = data1;
            b.action = learn_action_;
            b.deck = learn_deck_;
            b.comment = QStringLiteral("Learned Control");
            profile_.addBinding(b);

            const auto cb = learn_cb_;
            const auto action = learn_action_;
            const auto deck = learn_deck_;
            is_learning_ = false;
            learn_action_ = MidiAction::NONE;
            lock.unlock();

            if (cb) {
                cb(msg_type, channel, data1, action, deck);
            }
            return;
        }
    }

    // 2. Normal Dispatch
    const MidiControlBinding *b = profile_.findBinding(msg_type, channel, data1);
    if (!b) return;

    // Pick target DeckPlayer
    DeckPlayer *target_deck = nullptr;
    if (b->deck == 1) {
        target_deck = vj_deck_a();
    } else if (b->deck == 2) {
        target_deck = vj_deck_b();
    } else {
        target_deck = vj_active_deck();
    }

    const float normalized_val = static_cast<float>(data2) / 127.0f;
    const float bipolar_val = (static_cast<float>(data2) / 63.5f) - 1.0f;

    switch (b->action) {
    case MidiAction::CROSSFADER:
        if (auto *decks = vdj_decks()) {
            decks->setCrossfader(static_cast<double>(normalized_val));
        }
        break;

    case MidiAction::VOLUME:
        if (target_deck) target_deck->setVolume(normalized_val);
        break;

    case MidiAction::GAIN:
        if (target_deck) target_deck->setGain(static_cast<float>(data2) / 63.5f);
        break;

    case MidiAction::EQ_HI:
        if (target_deck) target_deck->setEqHi(static_cast<float>(data2) / 63.5f);
        break;

    case MidiAction::EQ_MID:
        if (target_deck) target_deck->setEqMid(static_cast<float>(data2) / 63.5f);
        break;

    case MidiAction::EQ_LOW:
        if (target_deck) target_deck->setEqLow(static_cast<float>(data2) / 63.5f);
        break;

    case MidiAction::FILTER:
        if (target_deck) target_deck->setFilterVal(bipolar_val);
        break;

    case MidiAction::TEMPO_SPEED:
        if (target_deck) {
            const int speed = 50 + static_cast<int>((static_cast<unsigned int>(data2) * 100U) / 127U);
            target_deck->applySpeedSettings(speed);
        }
        break;

    case MidiAction::PITCH_BEND_UP:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->nudgePlayback(120);
        }
        break;

    case MidiAction::PITCH_BEND_DOWN:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->nudgePlayback(-120);
        }
        break;

    case MidiAction::PLAY_PAUSE:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->togglePlayback();
        }
        break;

    case MidiAction::CUE:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->cue();
        }
        break;

    case MidiAction::SYNC:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->setSync(!target_deck->isSync());
        }
        break;

    case MidiAction::HEADPHONE_CUE:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->setHeadphoneCue(!target_deck->isHeadphoneCue());
        }
        break;

    case MidiAction::LOOP_TOGGLE:
    case MidiAction::LOOP_AUTO_4:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->setLooping(!target_deck->isLooping());
        }
        break;

    case MidiAction::LOOP_HALVE:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            const int cur = target_deck->loopBeatIndex();
            if (cur > 0) {
                target_deck->setLoopBeatIndex(cur - 1);
                if (target_deck->isLooping()) target_deck->setLooping(true);
            }
        }
        break;

    case MidiAction::LOOP_DOUBLE:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            const int cur = target_deck->loopBeatIndex();
            if (cur < 11) {
                target_deck->setLoopBeatIndex(cur + 1);
                if (target_deck->isLooping()) target_deck->setLooping(true);
            }
        }
        break;

    case MidiAction::LOOP_SIZE_CHANGE: {
        auto &jog = (b->deck == 2) ? jog_b_ : jog_a_;
        jog.setMode(b->jog_mode);
        const double delta = jog.processDelta(data2);
        if (target_deck && std::abs(delta) > 0.001) {
            const int cur = target_deck->loopBeatIndex();
            const int new_idx = std::clamp(cur + (delta > 0 ? 1 : -1), 0, 11);
            if (new_idx != cur) {
                target_deck->setLoopBeatIndex(new_idx);
                if (target_deck->isLooping()) target_deck->setLooping(true);
            }
        }
        break;
    }

    case MidiAction::LOOP_IN:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->setLooping(false);
            target_deck->setLooping(true);
        }
        break;

    case MidiAction::LOOP_OUT:
    case MidiAction::LOOP_EXIT:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->setLooping(false);
        }
        break;

    case MidiAction::HOTCUE_1:
        if (target_deck) target_deck->triggerHotCue(0);
        break;
    case MidiAction::HOTCUE_2:
        if (target_deck) target_deck->triggerHotCue(1);
        break;
    case MidiAction::HOTCUE_3:
        if (target_deck) target_deck->triggerHotCue(2);
        break;
    case MidiAction::HOTCUE_4:
        if (target_deck) target_deck->triggerHotCue(3);
        break;

    case MidiAction::BROWSE_SCROLL: {
        jog_browse_.setMode(b->jog_mode);
        const double delta = jog_browse_.processDelta(data2);
        if (std::abs(delta) > 0.001) {
            vdj_tracklist_move_selection(delta > 0 ? 1 : -1);
        }
        break;
    }

    case MidiAction::BROWSE_NEXT:
        if (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0)) {
            vdj_tracklist_move_selection(1);
        }
        break;

    case MidiAction::BROWSE_PREV:
        if (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0)) {
            vdj_tracklist_move_selection(-1);
        }
        break;

    case MidiAction::LOAD_SELECTED_A:
        if (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0)) {
            vdj_tracklist_load_selected(0);
        }
        break;

    case MidiAction::LOAD_SELECTED_B:
        if (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0)) {
            vdj_tracklist_load_selected(1);
        }
        break;

    case MidiAction::LOAD_SELECTED:
        if (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0)) {
            int target_ch = (b->deck == 2) ? 1 : ((b->deck == 1) ? 0 : -1);
            if (target_ch < 0) {
                DeckPlayer *da = vj_deck_a();
                DeckPlayer *db = vj_deck_b();
                if (da && db) {
                    if (da->isPlaying() && !db->isPlaying()) target_ch = 1;
                    else if (!da->isPlaying() && db->isPlaying()) target_ch = 0;
                    else target_ch = (vj_active_deck() == db) ? 1 : 0;
                } else {
                    target_ch = 0;
                }
            }
            vdj_tracklist_load_selected(target_ch);
        }
        break;

    case MidiAction::JOG_TOUCH:
        if (b->deck == 2) jog_b_.setTouched(data2 > 0);
        else jog_a_.setTouched(data2 > 0);
        break;

    case MidiAction::JOG_SCRATCH:
    case MidiAction::JOG_PITCHBEND: {
        auto &jog = (b->deck == 2) ? jog_b_ : jog_a_;
        jog.setMode(b->jog_mode);
        const double delta = jog.processDelta(data2);
        if (target_deck && std::abs(delta) > 0.001) {
            if (jog.isTouched()) {
                target_deck->nudgePlayback(static_cast<int64_t>(delta * 120.0));
            } else {
                target_deck->nudgePlayback(static_cast<int64_t>(delta * 25.0));
            }
        }
        break;
    }

    case MidiAction::VJ_PARAM_X: // E.g. Korg Kaoss Touchpad X -> Shader P0
        if (auto *decks = vdj_decks()) {
            decks->setCrossfadeFxParamValue(decks->crossfadeFxMode(), 0, normalized_val);
        }
        break;

    case MidiAction::VJ_PARAM_Y: // E.g. Korg Kaoss Touchpad Y -> Shader P1
        if (auto *decks = vdj_decks()) {
            decks->setCrossfadeFxParamValue(decks->crossfadeFxMode(), 1, normalized_val);
        }
        break;

    default:
        break;
    }
}
