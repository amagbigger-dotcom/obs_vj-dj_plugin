#pragma once

#include <cstdint>
#include <string>

enum class MidiAction : uint16_t {
    NONE = 0,
    // Transport & Playback
    PLAY_PAUSE,
    CUE,
    SYNC,
    HEADPHONE_CUE,
    PITCH_BEND_UP,
    PITCH_BEND_DOWN,
    TEMPO_SPEED,
    KEY_LOCK,

    // Mixer & EQ
    VOLUME,
    EQ_HI,
    EQ_MID,
    EQ_LOW,
    FILTER,
    GAIN,
    CROSSFADER,

    // Jog Wheels
    JOG_TOUCH,
    JOG_SCRATCH,
    JOG_PITCHBEND,

    // Loops & Hotcues
    LOOP_TOGGLE,
    LOOP_HALVE,
    LOOP_DOUBLE,
    LOOP_SIZE_CHANGE,
    LOOP_AUTO_4,
    LOOP_IN,
    LOOP_OUT,
    LOOP_EXIT,
    HOTCUE_1,
    HOTCUE_2,
    HOTCUE_3,
    HOTCUE_4,

    // Library & Navigation
    BROWSE_SCROLL,
    BROWSE_NEXT,
    BROWSE_PREV,
    LOAD_SELECTED_A,
    LOAD_SELECTED_B,
    LOAD_SELECTED,

    // Modifiers & Beatgrid
    SHIFT,
    TOUCH_STRIP,
    GRID_NUDGE_LEFT,
    GRID_NUDGE_RIGHT,
    GRID_SET_ANCHOR,

    // VJ & Video Controls
    VJ_PARAM_X,      // Touchpad X / Ribbon -> Shader p0
    VJ_PARAM_Y,      // Touchpad Y -> Shader p1
    VJ_EFFECT_NEXT,
    VJ_EFFECT_PREV,
    CROSSFADE_EFFECT_NEXT
};

enum class MidiMsgType : uint8_t {
    CC = 0,
    NOTE_ON = 1,
    NOTE_OFF = 2,
    PITCH_BEND = 3
};

enum class MidiJogMode : uint8_t {
    SPREAD_64 = 0,    // Two's complement / center 64: 65=+1, 63=-1 (Pioneer, Traktor)
    SIGN_MAGNITUDE = 1, // Bit 6 gives sign
    BIN_OFFSET = 2,    // 1=+1, 127=-1 (Numark, Denon)
    ABSOLUTE = 3       // 0..127 standard continuous
};

const char *midi_action_to_name(MidiAction action);
MidiAction midi_name_to_action(const std::string &name);
