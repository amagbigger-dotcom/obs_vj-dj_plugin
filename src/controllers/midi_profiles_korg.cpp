#include "controllers/midi_mapping.hpp"

MidiControllerProfile MidiControllerProfile::createKorgKaossDJ()
{
    MidiControllerProfile p("Kaoss DJ", "Korg");

    // =========================================================================
    // Korg KAOSS DJ Native Mapping
    // Ch 7: Global / Master / Touchpad / Browser
    // Ch 8: Deck A
    // Ch 9: Deck B
    // =========================================================================

    // --- Global Controls (Ch 7) ---
    p.addBinding({MidiMsgType::CC, 7, 23, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader"});
    p.addBinding({MidiMsgType::CC, 7, 12, MidiAction::VJ_PARAM_X, 0, MidiJogMode::ABSOLUTE, "Kaoss Touchpad X -> VJ P0"});
    p.addBinding({MidiMsgType::CC, 7, 13, MidiAction::VJ_PARAM_Y, 0, MidiJogMode::ABSOLUTE, "Kaoss Touchpad Y -> VJ P1"});
    p.addBinding({MidiMsgType::CC, 7, 30, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary Encoder"});
    p.addBinding({MidiMsgType::CC, 7, 31, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Program/Value Encoder"});
    p.addBinding({MidiMsgType::CC, 7, 29, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary (Alt CC 29)"});
    p.addBinding({MidiMsgType::NOTE_ON, 7, 14, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Browse Push (Load)"});
    p.addBinding({MidiMsgType::NOTE_ON, 7, 32, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Touchpad Mode / Load"});
    p.addBinding({MidiMsgType::NOTE_ON, 7, 11, MidiAction::SYNC, 0, MidiJogMode::SPREAD_64, "Tap BPM"});

    // --- Deck A (Ch 8) ---
    p.addBinding({MidiMsgType::NOTE_ON, 8, 27, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 30, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 29, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 25, MidiAction::HEADPHONE_CUE, 1, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 14, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 35, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A (Hold)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 45, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A (Alt)"});

    p.addBinding({MidiMsgType::CC, 8, 24, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 8, 25, MidiAction::TEMPO_SPEED, 1, MidiJogMode::SPREAD_64, "Pitch / Tempo Fader"});
    p.addBinding({MidiMsgType::CC, 8, 26, MidiAction::GAIN, 1, MidiJogMode::SPREAD_64, "Gain"});
    p.addBinding({MidiMsgType::CC, 8, 27, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 8, 28, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 8, 29, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 8, 22, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 8, 30, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter (Alt CC 30)"});

    // Deck A Touch Platter (Jog Wheel)
    p.addBinding({MidiMsgType::NOTE_ON, 8, 31, MidiAction::JOG_TOUCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel Touch"});
    p.addBinding({MidiMsgType::CC, 8, 14, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel Movement"});
    p.addBinding({MidiMsgType::CC, 8, 15, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel Movement (Shift)"});
    p.addBinding({MidiMsgType::CC, 8, 16, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel / Strip Scratch"});

    // Deck A Shift
    p.addBinding({MidiMsgType::NOTE_ON, 8, 26, MidiAction::SHIFT, 1, MidiJogMode::SPREAD_64, "Shift Deck A"});

    // Deck A Touch Strip - Normal Mode (Jog feletti resz: Nudge apro lokdos, Shift-tel Grid pozicionalas)
    p.addBinding({MidiMsgType::NOTE_ON, 8, 21, MidiAction::PITCH_BEND_DOWN, 1, MidiJogMode::SPREAD_64, "Nudge Down / Shift: Grid Left (<<)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 22, MidiAction::GRID_SET_ANCHOR, 1, MidiJogMode::SPREAD_64, "Set Grid Anchor (Kick 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 23, MidiAction::PITCH_BEND_UP, 1, MidiJogMode::SPREAD_64, "Nudge Up / Shift: Grid Right (>>)"});
    p.addBinding({MidiMsgType::CC, 8, 33, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Strip Position"});

    // Deck A Touch Strip - Loop Mode
    p.addBinding({MidiMsgType::NOTE_ON, 8, 15, MidiAction::LOOP_HALVE, 1, MidiJogMode::SPREAD_64, "Loop 1/2X (Left)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 16, MidiAction::LOOP_TOGGLE, 1, MidiJogMode::SPREAD_64, "Loop On/Off / Catch (Center)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 17, MidiAction::LOOP_DOUBLE, 1, MidiJogMode::SPREAD_64, "Loop 2X (Right)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 28, MidiAction::LOOP_HALVE, 1, MidiJogMode::SPREAD_64, "Loop 1/2X (Alt Note 28)"});

    // Deck A Touch Strip - Hot Cue Mode
    p.addBinding({MidiMsgType::NOTE_ON, 8, 18, MidiAction::HOTCUE_1, 1, MidiJogMode::SPREAD_64, "Hot Cue 1"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 19, MidiAction::HOTCUE_2, 1, MidiJogMode::SPREAD_64, "Hot Cue 2"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 20, MidiAction::HOTCUE_3, 1, MidiJogMode::SPREAD_64, "Hot Cue 3"});

    // Deck A FX Button
    p.addBinding({MidiMsgType::NOTE_ON, 8, 24, MidiAction::CROSSFADE_EFFECT_NEXT, 1, MidiJogMode::SPREAD_64, "FX Button -> Crossfade FX Next"});

    // --- Deck B (Ch 9) ---
    p.addBinding({MidiMsgType::NOTE_ON, 9, 27, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 30, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 29, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 25, MidiAction::HEADPHONE_CUE, 2, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 14, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 35, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B (Hold)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 46, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B (Alt)"});

    p.addBinding({MidiMsgType::CC, 9, 24, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 9, 25, MidiAction::TEMPO_SPEED, 2, MidiJogMode::SPREAD_64, "Pitch / Tempo Fader"});
    p.addBinding({MidiMsgType::CC, 9, 26, MidiAction::GAIN, 2, MidiJogMode::SPREAD_64, "Gain"});
    p.addBinding({MidiMsgType::CC, 9, 27, MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 9, 28, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 9, 29, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 9, 22, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 9, 30, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter (Alt CC 30)"});

    // Deck B Touch Platter (Jog Wheel)
    p.addBinding({MidiMsgType::NOTE_ON, 9, 31, MidiAction::JOG_TOUCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel Touch"});
    p.addBinding({MidiMsgType::CC, 9, 14, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel Movement"});
    p.addBinding({MidiMsgType::CC, 9, 15, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel Movement (Shift)"});
    p.addBinding({MidiMsgType::CC, 9, 16, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel / Strip Scratch"});

    // Deck B Shift
    p.addBinding({MidiMsgType::NOTE_ON, 9, 26, MidiAction::SHIFT, 2, MidiJogMode::SPREAD_64, "Shift Deck B"});

    // Deck B Touch Strip - Normal Mode (Jog feletti resz: Nudge apro lokdos, Shift-tel Grid pozicionalas)
    p.addBinding({MidiMsgType::NOTE_ON, 9, 21, MidiAction::PITCH_BEND_DOWN, 2, MidiJogMode::SPREAD_64, "Nudge Down / Shift: Grid Left (<<)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 22, MidiAction::GRID_SET_ANCHOR, 2, MidiJogMode::SPREAD_64, "Set Grid Anchor (Kick 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 23, MidiAction::PITCH_BEND_UP, 2, MidiJogMode::SPREAD_64, "Nudge Up / Shift: Grid Right (>>)"});
    p.addBinding({MidiMsgType::CC, 9, 33, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Strip Position"});

    // Deck B Touch Strip - Loop Mode
    p.addBinding({MidiMsgType::NOTE_ON, 9, 15, MidiAction::LOOP_HALVE, 2, MidiJogMode::SPREAD_64, "Loop 1/2X (Left)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 16, MidiAction::LOOP_TOGGLE, 2, MidiJogMode::SPREAD_64, "Loop On/Off / Catch (Center)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 17, MidiAction::LOOP_DOUBLE, 2, MidiJogMode::SPREAD_64, "Loop 2X (Right)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 28, MidiAction::LOOP_HALVE, 2, MidiJogMode::SPREAD_64, "Loop 1/2X (Alt Note 28)"});

    // Deck B Touch Strip - Hot Cue Mode
    p.addBinding({MidiMsgType::NOTE_ON, 9, 18, MidiAction::HOTCUE_1, 2, MidiJogMode::SPREAD_64, "Hot Cue 1"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 19, MidiAction::HOTCUE_2, 2, MidiJogMode::SPREAD_64, "Hot Cue 2"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 20, MidiAction::HOTCUE_3, 2, MidiJogMode::SPREAD_64, "Hot Cue 3"});

    // Deck B FX Button
    p.addBinding({MidiMsgType::NOTE_ON, 9, 24, MidiAction::CROSSFADE_EFFECT_NEXT, 2, MidiJogMode::SPREAD_64, "FX Button -> Crossfade FX Next"});

    // =========================================================================
    // Fallback Mappings for Channel 1 (Deck A) & Channel 2 (Deck B)
    // =========================================================================
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader (Ch 1 CC 8)"});
    p.addBinding({MidiMsgType::CC, 1, 23, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader (Ch 1 CC 23)"});
    p.addBinding({MidiMsgType::CC, 1, 12, MidiAction::VJ_PARAM_X, 0, MidiJogMode::ABSOLUTE, "Touchpad X (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 13, MidiAction::VJ_PARAM_Y, 0, MidiJogMode::ABSOLUTE, "Touchpad Y (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 30, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary (Ch 1 CC 30)"});
    p.addBinding({MidiMsgType::CC, 1, 31, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Value (Ch 1 CC 31)"});
    p.addBinding({MidiMsgType::CC, 1, 29, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary (Ch 1 CC 29)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 14, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Browse Push (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 32, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Touchpad Push (Ch 1)"});

    // Deck A (Ch 1)
    p.addBinding({MidiMsgType::NOTE_ON, 1, 27, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 30, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 29, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 25, MidiAction::HEADPHONE_CUE, 1, MidiJogMode::SPREAD_64, "Headphone CUE (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 14, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A (Ch 1 Note 14)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 35, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A (Ch 1 Hold 35)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 45, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A (Ch 1 Note 45)"});
    p.addBinding({MidiMsgType::CC, 1, 24, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 25, MidiAction::TEMPO_SPEED, 1, MidiJogMode::SPREAD_64, "Pitch / Tempo (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 26, MidiAction::GAIN, 1, MidiJogMode::SPREAD_64, "Gain (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 27, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 28, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 29, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 22, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 14, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel (Ch 1 CC 14)"});
    p.addBinding({MidiMsgType::CC, 1, 15, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel (Shift Ch 1 CC 15)"});
    p.addBinding({MidiMsgType::CC, 1, 16, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel (Ch 1 CC 16)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 26, MidiAction::SHIFT, 1, MidiJogMode::SPREAD_64, "Shift Deck A (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 31, MidiAction::JOG_TOUCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel Touch (Ch 1 Note 31)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 21, MidiAction::PITCH_BEND_DOWN, 1, MidiJogMode::SPREAD_64, "Nudge Down / Shift: Grid Left (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 22, MidiAction::GRID_SET_ANCHOR, 1, MidiJogMode::SPREAD_64, "Set Grid Anchor (Kick 1 Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 23, MidiAction::PITCH_BEND_UP, 1, MidiJogMode::SPREAD_64, "Nudge Up / Shift: Grid Right (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 15, MidiAction::LOOP_HALVE, 1, MidiJogMode::SPREAD_64, "Loop 1/2X (Ch 1 Note 15)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 16, MidiAction::LOOP_TOGGLE, 1, MidiJogMode::SPREAD_64, "Loop Toggle (Ch 1 Note 16)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 17, MidiAction::LOOP_DOUBLE, 1, MidiJogMode::SPREAD_64, "Loop 2X (Ch 1 Note 17)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 28, MidiAction::LOOP_HALVE, 1, MidiJogMode::SPREAD_64, "Loop 1/2X (Ch 1 Note 28)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 18, MidiAction::HOTCUE_1, 1, MidiJogMode::SPREAD_64, "Hot Cue 1 (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 19, MidiAction::HOTCUE_2, 1, MidiJogMode::SPREAD_64, "Hot Cue 2 (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 20, MidiAction::HOTCUE_3, 1, MidiJogMode::SPREAD_64, "Hot Cue 3 (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 24, MidiAction::CROSSFADE_EFFECT_NEXT, 1, MidiJogMode::SPREAD_64, "FX (Ch 1)"});

    // Deck B (Ch 2)
    p.addBinding({MidiMsgType::NOTE_ON, 2, 27, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 30, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 29, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 25, MidiAction::HEADPHONE_CUE, 2, MidiJogMode::SPREAD_64, "Headphone CUE (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 14, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B (Ch 2 Note 14)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 35, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B (Ch 2 Hold 35)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 46, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B (Ch 2 Note 46)"});
    p.addBinding({MidiMsgType::CC, 2, 24, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Volume (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 25, MidiAction::TEMPO_SPEED, 2, MidiJogMode::SPREAD_64, "Pitch / Tempo (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 26, MidiAction::GAIN, 2, MidiJogMode::SPREAD_64, "Gain (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 27, MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 28, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 29, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 22, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 14, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel (Ch 2 CC 14)"});
    p.addBinding({MidiMsgType::CC, 2, 15, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel (Shift Ch 2 CC 15)"});
    p.addBinding({MidiMsgType::CC, 2, 16, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel (Ch 2 CC 16)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 26, MidiAction::SHIFT, 2, MidiJogMode::SPREAD_64, "Shift Deck B (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 31, MidiAction::JOG_TOUCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel Touch (Ch 2 Note 31)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 21, MidiAction::PITCH_BEND_DOWN, 2, MidiJogMode::SPREAD_64, "Nudge Down / Shift: Grid Left (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 22, MidiAction::GRID_SET_ANCHOR, 2, MidiJogMode::SPREAD_64, "Set Grid Anchor (Kick 1 Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 23, MidiAction::PITCH_BEND_UP, 2, MidiJogMode::SPREAD_64, "Nudge Up / Shift: Grid Right (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 15, MidiAction::LOOP_HALVE, 2, MidiJogMode::SPREAD_64, "Loop 1/2X (Ch 2 Note 15)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 16, MidiAction::LOOP_TOGGLE, 2, MidiJogMode::SPREAD_64, "Loop Toggle (Ch 2 Note 16)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 17, MidiAction::LOOP_DOUBLE, 2, MidiJogMode::SPREAD_64, "Loop 2X (Ch 2 Note 17)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 28, MidiAction::LOOP_HALVE, 2, MidiJogMode::SPREAD_64, "Loop 1/2X (Ch 2 Note 28)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 18, MidiAction::HOTCUE_1, 2, MidiJogMode::SPREAD_64, "Hot Cue 1 (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 19, MidiAction::HOTCUE_2, 2, MidiJogMode::SPREAD_64, "Hot Cue 2 (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 20, MidiAction::HOTCUE_3, 2, MidiJogMode::SPREAD_64, "Hot Cue 3 (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 24, MidiAction::CROSSFADE_EFFECT_NEXT, 2, MidiJogMode::SPREAD_64, "FX (Ch 2)"});

    return p;
}
