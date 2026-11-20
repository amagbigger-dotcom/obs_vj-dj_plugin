#include "controllers/midi_mapping.hpp"

MidiControllerProfile MidiControllerProfile::createKorgKaossDJ()
{
    MidiControllerProfile p("Kaoss DJ", "Korg");

    // =========================================================================
    // Korg KAOSS DJ Native Mapping
    // Ch 7: Global / Master / Touchpad
    // Ch 8: Deck A
    // Ch 9: Deck B
    // =========================================================================

    // --- Global Controls (Ch 7) ---
    p.addBinding({MidiMsgType::CC, 7, 23, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader"});
    p.addBinding({MidiMsgType::CC, 7, 12, MidiAction::VJ_PARAM_X, 0, MidiJogMode::ABSOLUTE, "Kaoss Touchpad X -> VJ P0"});
    p.addBinding({MidiMsgType::CC, 7, 13, MidiAction::VJ_PARAM_Y, 0, MidiJogMode::ABSOLUTE, "Kaoss Touchpad Y -> VJ P1"});
    p.addBinding({MidiMsgType::CC, 7, 29, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::BIN_OFFSET, "Browse Rotary Encoder"});
    p.addBinding({MidiMsgType::NOTE_ON, 7, 32, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Browse Push (Load)"});

    // --- Deck A (Ch 8) ---
    p.addBinding({MidiMsgType::NOTE_ON, 8, 27, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 30, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 29, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 25, MidiAction::HEADPHONE_CUE, 1, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 45, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A"});

    p.addBinding({MidiMsgType::CC, 8, 24, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 8, 25, MidiAction::TEMPO_SPEED, 1, MidiJogMode::SPREAD_64, "Pitch / Tempo Fader"});
    p.addBinding({MidiMsgType::CC, 8, 26, MidiAction::GAIN, 1, MidiJogMode::SPREAD_64, "Gain"});
    p.addBinding({MidiMsgType::CC, 8, 27, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 8, 28, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 8, 29, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 8, 22, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 8, 30, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter (Alt CC 30)"});

    p.addBinding({MidiMsgType::CC, 8, 16, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel / Strip"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 16, MidiAction::JOG_TOUCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel Touch"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 21, MidiAction::PITCH_BEND_DOWN, 1, MidiJogMode::SPREAD_64, "Pitch Bend Down (<<)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 22, MidiAction::PITCH_BEND_UP, 1, MidiJogMode::SPREAD_64, "Pitch Bend Up (>>)"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 28, MidiAction::LOOP_HALVE, 1, MidiJogMode::SPREAD_64, "Loop 1/2X / In"});
    p.addBinding({MidiMsgType::NOTE_ON, 8, 31, MidiAction::LOOP_TOGGLE, 1, MidiJogMode::SPREAD_64, "Loop Toggle / Auto Loop"});

    // --- Deck B (Ch 9) ---
    p.addBinding({MidiMsgType::NOTE_ON, 9, 27, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 30, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 29, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 25, MidiAction::HEADPHONE_CUE, 2, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 46, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B"});

    p.addBinding({MidiMsgType::CC, 9, 24, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 9, 25, MidiAction::TEMPO_SPEED, 2, MidiJogMode::SPREAD_64, "Pitch / Tempo Fader"});
    p.addBinding({MidiMsgType::CC, 9, 26, MidiAction::GAIN, 2, MidiJogMode::SPREAD_64, "Gain"});
    p.addBinding({MidiMsgType::CC, 9, 27, MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 9, 28, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 9, 29, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 9, 22, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 9, 30, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter (Alt CC 30)"});

    p.addBinding({MidiMsgType::CC, 9, 16, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel / Strip"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 16, MidiAction::JOG_TOUCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel Touch"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 21, MidiAction::PITCH_BEND_DOWN, 2, MidiJogMode::SPREAD_64, "Pitch Bend Down (<<)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 22, MidiAction::PITCH_BEND_UP, 2, MidiJogMode::SPREAD_64, "Pitch Bend Up (>>)"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 28, MidiAction::LOOP_HALVE, 2, MidiJogMode::SPREAD_64, "Loop 1/2X / In"});
    p.addBinding({MidiMsgType::NOTE_ON, 9, 31, MidiAction::LOOP_TOGGLE, 2, MidiJogMode::SPREAD_64, "Loop Toggle / Auto Loop"});

    // =========================================================================
    // Fallback Mappings for Channel 1 (Deck A) & Channel 2 (Deck B)
    // =========================================================================
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader (Ch 1 CC 8)"});
    p.addBinding({MidiMsgType::CC, 1, 23, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader (Ch 1 CC 23)"});
    p.addBinding({MidiMsgType::CC, 1, 12, MidiAction::VJ_PARAM_X, 0, MidiJogMode::ABSOLUTE, "Touchpad X (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 13, MidiAction::VJ_PARAM_Y, 0, MidiJogMode::ABSOLUTE, "Touchpad Y (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 29, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::BIN_OFFSET, "Browse Rotary (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 32, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Browse Push (Ch 1)"});

    // Deck A (Ch 1)
    p.addBinding({MidiMsgType::NOTE_ON, 1, 27, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 30, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 29, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 25, MidiAction::HEADPHONE_CUE, 1, MidiJogMode::SPREAD_64, "Headphone CUE (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 45, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 24, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 25, MidiAction::TEMPO_SPEED, 1, MidiJogMode::SPREAD_64, "Pitch / Tempo (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 26, MidiAction::GAIN, 1, MidiJogMode::SPREAD_64, "Gain (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 27, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 28, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 29, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 22, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter (Ch 1)"});
    p.addBinding({MidiMsgType::CC, 1, 16, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 16, MidiAction::JOG_TOUCH, 1, MidiJogMode::SPREAD_64, "Touch Wheel Touch (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 21, MidiAction::PITCH_BEND_DOWN, 1, MidiJogMode::SPREAD_64, "Pitch Bend Down (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 22, MidiAction::PITCH_BEND_UP, 1, MidiJogMode::SPREAD_64, "Pitch Bend Up (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 28, MidiAction::LOOP_HALVE, 1, MidiJogMode::SPREAD_64, "Loop 1/2X (Ch 1)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 31, MidiAction::LOOP_TOGGLE, 1, MidiJogMode::SPREAD_64, "Loop Toggle (Ch 1)"});

    // Deck B (Ch 2)
    p.addBinding({MidiMsgType::NOTE_ON, 2, 27, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 30, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 29, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 25, MidiAction::HEADPHONE_CUE, 2, MidiJogMode::SPREAD_64, "Headphone CUE (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 46, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 24, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Volume (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 25, MidiAction::TEMPO_SPEED, 2, MidiJogMode::SPREAD_64, "Pitch / Tempo (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 26, MidiAction::GAIN, 2, MidiJogMode::SPREAD_64, "Gain (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 27, MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 28, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 29, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 22, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter (Ch 2)"});
    p.addBinding({MidiMsgType::CC, 2, 16, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 16, MidiAction::JOG_TOUCH, 2, MidiJogMode::SPREAD_64, "Touch Wheel Touch (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 21, MidiAction::PITCH_BEND_DOWN, 2, MidiJogMode::SPREAD_64, "Pitch Bend Down (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 22, MidiAction::PITCH_BEND_UP, 2, MidiJogMode::SPREAD_64, "Pitch Bend Up (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 28, MidiAction::LOOP_HALVE, 2, MidiJogMode::SPREAD_64, "Loop 1/2X (Ch 2)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 31, MidiAction::LOOP_TOGGLE, 2, MidiJogMode::SPREAD_64, "Loop Toggle (Ch 2)"});

    return p;
}
