#include "controllers/midi_dispatcher.hpp"
#include "controllers/midi_handler.hpp"
#include "engine/deck_engine.hpp"
#include "gui/tracklist_dock.hpp"
#include <algorithm>
#include <unordered_map>

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
    last_feedback_sent_.clear();
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

bool MidiDispatcher::isShiftPressed(int deck) const
{
    if (deck == 1) return shift_a_.load(std::memory_order_relaxed) || shift_global_.load(std::memory_order_relaxed);
    if (deck == 2) return shift_b_.load(std::memory_order_relaxed) || shift_global_.load(std::memory_order_relaxed);
    return shift_a_.load(std::memory_order_relaxed) || shift_b_.load(std::memory_order_relaxed) || shift_global_.load(std::memory_order_relaxed);
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
            if (isShiftPressed(b->deck)) {
                // Shift + >> : moves beatgrid forward (+5 ms) to align grid to kick drum
                target_deck->nudgeGrid(5);
            } else {
                // Normal >> : gentle phase nudge (+15 ms)
                target_deck->nudgePlayback(15);
            }
        }
        break;

    case MidiAction::PITCH_BEND_DOWN:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            if (isShiftPressed(b->deck)) {
                // Shift + << : moves beatgrid backward (-5 ms) to align grid to kick drum
                target_deck->nudgeGrid(-5);
            } else {
                // Normal << : gentle phase nudge (-15 ms)
                target_deck->nudgePlayback(-15);
            }
        }
        break;

    case MidiAction::GRID_NUDGE_RIGHT:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->nudgeGrid(5);
        }
        break;

    case MidiAction::GRID_NUDGE_LEFT:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->nudgeGrid(-5);
        }
        break;

    case MidiAction::GRID_SET_ANCHOR:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            target_deck->setGridAnchorAtCurrent(true);
        }
        break;

    case MidiAction::TOUCH_STRIP: {
        if (!target_deck) break;
        int &last_val = (b->deck == 2) ? last_strip_b_ : last_strip_a_;
        auto &last_time = (b->deck == 2) ? last_strip_time_b_ : last_strip_time_a_;
        auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
        last_time = now;

        if (last_val < 0 || elapsed_ms > 300) {
            // New touch session / touch down: anchor position without jump
            last_val = static_cast<int>(data2);
            break;
        }

        const int diff = static_cast<int>(data2) - last_val;
        last_val = static_cast<int>(data2);

        // If finger lifted and tapped far away (> 35), avoid large sudden jumps
        if (std::abs(diff) > 35) break;

        if (isShiftPressed(b->deck)) {
            // SHIFT held: smoothly drag/slide the beatgrid across the waveform (2 ms per unit)
            target_deck->nudgeGrid(static_cast<int64_t>(diff * 2));
        } else {
            // SHIFT not held: smooth audio scrub / position nudge (15 ms per unit)
            target_deck->nudgePlayback(static_cast<int64_t>(diff * 15));
        }
        break;
    }

    case MidiAction::SHIFT: {
        const bool active = (msg_type == MidiMsgType::NOTE_ON && data2 > 0) ||
                            (msg_type == MidiMsgType::CC && data2 > 63);
        if (b->deck == 1) {
            shift_a_.store(active, std::memory_order_relaxed);
        } else if (b->deck == 2) {
            shift_b_.store(active, std::memory_order_relaxed);
        } else {
            shift_global_.store(active, std::memory_order_relaxed);
        }
        break;
    }

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
                const int new_idx = cur - 1;
                target_deck->setLoopBeatIndex(new_idx);
                if (auto *dp = vdj_decks()) dp->setLoopBeatIndex(b->deck == 2 ? 1 : 0, new_idx);
                if (target_deck->isLooping()) target_deck->setLooping(true);
            }
        }
        break;

    case MidiAction::LOOP_DOUBLE:
        if (target_deck && (msg_type == MidiMsgType::NOTE_ON || (msg_type == MidiMsgType::CC && data2 > 0))) {
            const int cur = target_deck->loopBeatIndex();
            if (cur < 11) {
                const int new_idx = cur + 1;
                target_deck->setLoopBeatIndex(new_idx);
                if (auto *dp = vdj_decks()) dp->setLoopBeatIndex(b->deck == 2 ? 1 : 0, new_idx);
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
                if (auto *dp = vdj_decks()) dp->setLoopBeatIndex(b->deck == 2 ? 1 : 0, new_idx);
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
        double delta = jog_browse_.processDelta(data2);
        // Robust auto-detection for relative encoders (BIN_OFFSET 1/127 and SPREAD_64 65/63)
        if (data2 == 127 || data2 == 126) delta = -1.0;
        else if (data2 == 1 || data2 == 2) delta = 1.0;
        else if (data2 == 65 || data2 == 66) delta = 1.0;
        else if (data2 == 63 || data2 == 62) delta = -1.0;

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
            const bool shifted = isShiftPressed(b->deck) || (b->control == 15);
            if (shifted) {
                if (jog.isTouched()) {
                    // Platter touch + Shift: Ultra-fine audio positioning (2.0 ms/tick, sample accurate)
                    target_deck->nudgePlayback(static_cast<int64_t>(delta * 2.0));
                } else {
                    // Outer rim turn + Shift (or CC 15 wheelTurnShift): Slide beatgrid by 2 ms/tick!
                    target_deck->nudgeGrid(static_cast<int64_t>(delta * 2.0));
                }
            } else if (jog.isTouched()) {
                // Normal platter touch: half speed (50 ms/tick instead of 120 ms)
                target_deck->nudgePlayback(static_cast<int64_t>(delta * 50.0));
            } else {
                // Normal outer rim: gentle phase nudge (12 ms/tick)
                target_deck->nudgePlayback(static_cast<int64_t>(delta * 12.0));
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

void MidiDispatcher::sendFeedback(bool force_refresh)
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (!da && !db) return;

    std::lock_guard<std::mutex> lock(mutex_);

    feedback_blink_counter_++;
    const bool blink_state = (feedback_blink_counter_ % 20) < 10; // ~400ms on, 400ms off

    auto sendIfChanged = [this, force_refresh](uint8_t status, uint8_t note, uint8_t val) {
        const uint32_t key = (static_cast<uint32_t>(status) << 8) | note;
        auto it = last_feedback_sent_.find(key);
        if (force_refresh || it == last_feedback_sent_.end() || it->second != val) {
            last_feedback_sent_[key] = val;
            vdj_midi_send_short_msg(status, note, val);
        }
    };

    const bool play_a = da && da->isPlaying();
    const bool cue_a = da && (da->cuePointMs() >= 0);
    const bool sync_a = da && da->isSync();
    const bool pfl_a = da && da->isHeadphoneCue();
    const bool loop_a = da && da->isLooping();
    const bool load_a = da && (!da->filePath().isEmpty() || da->durationMs() > 0);

    const bool play_b = db && db->isPlaying();
    const bool cue_b = db && (db->cuePointMs() >= 0);
    const bool sync_b = db && db->isSync();
    const bool pfl_b = db && db->isHeadphoneCue();
    const bool loop_b = db && db->isLooping();
    const bool load_b = db && (!db->filePath().isEmpty() || db->durationMs() > 0);

    const auto &bindings = profile_.bindings();

    for (const auto &b : bindings) {
        if (b.msg_type != MidiMsgType::NOTE_ON) continue;
        const uint8_t status = 0x90U | ((b.channel - 1) & 0x0FU);
        const uint8_t note = b.control;
        uint8_t val = 0;

        switch (b.action) {
        case MidiAction::PLAY_PAUSE:
            if (b.deck == 2) {
                val = play_b ? 127 : (cue_b && blink_state ? 127 : 0);
            } else {
                val = play_a ? 127 : (cue_a && blink_state ? 127 : 0);
            }
            break;
        case MidiAction::CUE:
            if (b.deck == 2) {
                val = play_b ? (blink_state ? 127 : 0) : (cue_b ? 127 : 0);
            } else {
                val = play_a ? (blink_state ? 127 : 0) : (cue_a ? 127 : 0);
            }
            break;
        case MidiAction::SYNC:
            val = (b.deck == 2 ? sync_b : sync_a) ? 127 : 0;
            break;
        case MidiAction::HEADPHONE_CUE:
            val = (b.deck == 2 ? pfl_b : pfl_a) ? 127 : 0;
            break;
        case MidiAction::LOOP_TOGGLE:
        case MidiAction::LOOP_HALVE:
        case MidiAction::LOOP_DOUBLE:
            val = (b.deck == 2 ? loop_b : loop_a) ? 127 : 0;
            break;
        case MidiAction::LOAD_SELECTED_A:
            val = load_a ? 127 : 0;
            break;
        case MidiAction::LOAD_SELECTED_B:
            val = load_b ? 127 : 0;
            break;
        case MidiAction::HOTCUE_1:
            val = ((b.deck == 2 ? db : da) && (b.deck == 2 ? db : da)->hotCueMs(0) >= 0) ? 127 : 0;
            break;
        case MidiAction::HOTCUE_2:
            val = ((b.deck == 2 ? db : da) && (b.deck == 2 ? db : da)->hotCueMs(1) >= 0) ? 127 : 0;
            break;
        case MidiAction::HOTCUE_3:
            val = ((b.deck == 2 ? db : da) && (b.deck == 2 ? db : da)->hotCueMs(2) >= 0) ? 127 : 0;
            break;
        case MidiAction::HOTCUE_4:
            val = ((b.deck == 2 ? db : da) && (b.deck == 2 ? db : da)->hotCueMs(3) >= 0) ? 127 : 0;
            break;
        case MidiAction::SHIFT:
            val = (b.deck == 2 ? shift_b_.load() : shift_a_.load()) ? 127 : 0;
            break;
        default:
            continue;
        }

        sendIfChanged(status, note, val);
    }

    // Specific Korg Kaoss DJ direct fallback sends to guarantee feedback on all hardware modes:
    // Deck A: Ch 8 (0x97) & Ch 1 (0x90)
    for (uint8_t ch_st : {static_cast<uint8_t>(0x97), static_cast<uint8_t>(0x90)}) {
        sendIfChanged(ch_st, 27, play_a ? 127 : (cue_a && blink_state ? 127 : 0)); // Play
        sendIfChanged(ch_st, 30, play_a ? (blink_state ? 127 : 0) : (cue_a ? 127 : 0)); // Cue
        sendIfChanged(ch_st, 29, sync_a ? 127 : 0); // Sync
        sendIfChanged(ch_st, 25, pfl_a ? 127 : 0);  // Headphone Cue
        sendIfChanged(ch_st, 26, shift_a_.load() ? 127 : 0); // Shift A
        sendIfChanged(ch_st, 16, loop_a ? 127 : 0); // Loop Active
        sendIfChanged(ch_st, 15, loop_a ? 127 : 0); // Loop 1/2X
        sendIfChanged(ch_st, 17, loop_a ? 127 : 0); // Loop 2X
        sendIfChanged(ch_st, 14, load_a ? 127 : 0); // Load A
    }

    // Deck B: Ch 9 (0x98) & Ch 2 (0x91)
    for (uint8_t ch_st : {static_cast<uint8_t>(0x98), static_cast<uint8_t>(0x91)}) {
        sendIfChanged(ch_st, 27, play_b ? 127 : (cue_b && blink_state ? 127 : 0)); // Play
        sendIfChanged(ch_st, 30, play_b ? (blink_state ? 127 : 0) : (cue_b ? 127 : 0)); // Cue
        sendIfChanged(ch_st, 29, sync_b ? 127 : 0); // Sync
        sendIfChanged(ch_st, 25, pfl_b ? 127 : 0);  // Headphone Cue
        sendIfChanged(ch_st, 26, shift_b_.load() ? 127 : 0); // Shift B
        sendIfChanged(ch_st, 16, loop_b ? 127 : 0); // Loop Active
        sendIfChanged(ch_st, 15, loop_b ? 127 : 0); // Loop 1/2X
        sendIfChanged(ch_st, 17, loop_b ? 127 : 0); // Loop 2X
        sendIfChanged(ch_st, 14, load_b ? 127 : 0); // Load B
    }
}
