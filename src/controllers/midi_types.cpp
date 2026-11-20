#include "controllers/midi_types.hpp"
#include <unordered_map>

static const std::unordered_map<MidiAction, const char*> g_action_names = {
    {MidiAction::PLAY_PAUSE, "PLAY_PAUSE"},
    {MidiAction::CUE, "CUE"},
    {MidiAction::SYNC, "SYNC"},
    {MidiAction::HEADPHONE_CUE, "HEADPHONE_CUE"},
    {MidiAction::PITCH_BEND_UP, "PITCH_BEND_UP"},
    {MidiAction::PITCH_BEND_DOWN, "PITCH_BEND_DOWN"},
    {MidiAction::TEMPO_SPEED, "TEMPO_SPEED"},
    {MidiAction::KEY_LOCK, "KEY_LOCK"},
    {MidiAction::VOLUME, "VOLUME"},
    {MidiAction::EQ_HI, "EQ_HI"},
    {MidiAction::EQ_MID, "EQ_MID"},
    {MidiAction::EQ_LOW, "EQ_LOW"},
    {MidiAction::FILTER, "FILTER"},
    {MidiAction::GAIN, "GAIN"},
    {MidiAction::CROSSFADER, "CROSSFADER"},
    {MidiAction::JOG_TOUCH, "JOG_TOUCH"},
    {MidiAction::JOG_SCRATCH, "JOG_SCRATCH"},
    {MidiAction::JOG_PITCHBEND, "JOG_PITCHBEND"},
    {MidiAction::LOOP_TOGGLE, "LOOP_TOGGLE"},
    {MidiAction::LOOP_HALVE, "LOOP_HALVE"},
    {MidiAction::LOOP_DOUBLE, "LOOP_DOUBLE"},
    {MidiAction::LOOP_SIZE_CHANGE, "LOOP_SIZE_CHANGE"},
    {MidiAction::LOOP_AUTO_4, "LOOP_AUTO_4"},
    {MidiAction::LOOP_IN, "LOOP_IN"},
    {MidiAction::LOOP_OUT, "LOOP_OUT"},
    {MidiAction::LOOP_EXIT, "LOOP_EXIT"},
    {MidiAction::HOTCUE_1, "HOTCUE_1"},
    {MidiAction::HOTCUE_2, "HOTCUE_2"},
    {MidiAction::HOTCUE_3, "HOTCUE_3"},
    {MidiAction::HOTCUE_4, "HOTCUE_4"},
    {MidiAction::BROWSE_SCROLL, "BROWSE_SCROLL"},
    {MidiAction::BROWSE_NEXT, "BROWSE_NEXT"},
    {MidiAction::BROWSE_PREV, "BROWSE_PREV"},
    {MidiAction::LOAD_SELECTED_A, "LOAD_SELECTED_A"},
    {MidiAction::LOAD_SELECTED_B, "LOAD_SELECTED_B"},
    {MidiAction::LOAD_SELECTED, "LOAD_SELECTED"},
    {MidiAction::SHIFT, "SHIFT"},
    {MidiAction::TOUCH_STRIP, "TOUCH_STRIP"},
    {MidiAction::GRID_NUDGE_LEFT, "GRID_NUDGE_LEFT"},
    {MidiAction::GRID_NUDGE_RIGHT, "GRID_NUDGE_RIGHT"},
    {MidiAction::GRID_SET_ANCHOR, "GRID_SET_ANCHOR"},
    {MidiAction::VJ_PARAM_X, "VJ_PARAM_X"},
    {MidiAction::VJ_PARAM_Y, "VJ_PARAM_Y"},
    {MidiAction::VJ_EFFECT_NEXT, "VJ_EFFECT_NEXT"},
    {MidiAction::VJ_EFFECT_PREV, "VJ_EFFECT_PREV"},
    {MidiAction::CROSSFADE_EFFECT_NEXT, "CROSSFADE_EFFECT_NEXT"}
};

const char *midi_action_to_name(MidiAction action)
{
    auto it = g_action_names.find(action);
    return (it != g_action_names.end()) ? it->second : "NONE";
}

MidiAction midi_name_to_action(const std::string &name)
{
    for (const auto &pair : g_action_names) {
        if (pair.second == name) {
            return pair.first;
        }
    }
    return MidiAction::NONE;
}
