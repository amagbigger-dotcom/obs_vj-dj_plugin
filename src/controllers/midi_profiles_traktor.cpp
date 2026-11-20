#include "controllers/midi_mapping.hpp"

MidiControllerProfile MidiControllerProfile::createTraktorS2MK3()
{
    MidiControllerProfile p("Traktor Kontrol S2 MK3", "Native Instruments");

    // Global
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Master Crossfader"});
    p.addBinding({MidiMsgType::CC, 1, 52, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary Encoder"});
    p.addBinding({MidiMsgType::CC, 1, 27, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary (Alt CC 27)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 52, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Browse Push"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 33, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 34, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B"});

    // Deck A (Ch 1)
    p.addBinding({MidiMsgType::CC, 1, 16, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 17, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 18, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 19, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 1, 20, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 1, 21, MidiAction::TEMPO_SPEED, 1, MidiJogMode::SPREAD_64, "Tempo Fader"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 35, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 36, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 37, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 38, MidiAction::HEADPHONE_CUE, 1, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::CC, 1, 25, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Jog Wheel"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 42, MidiAction::JOG_TOUCH, 1, MidiJogMode::SPREAD_64, "Jog Wheel Touch"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 28, MidiAction::LOOP_TOGGLE, 1, MidiJogMode::SPREAD_64, "Loop Toggle"});
    p.addBinding({MidiMsgType::CC, 1, 26, MidiAction::LOOP_SIZE_CHANGE, 1, MidiJogMode::SPREAD_64, "Loop Size Encoder"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 21, MidiAction::PITCH_BEND_DOWN, 1, MidiJogMode::SPREAD_64, "Pitch Bend Down (<<)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 22, MidiAction::PITCH_BEND_UP, 1, MidiJogMode::SPREAD_64, "Pitch Bend Up (>>)"});

    // Deck B (Ch 2)
    p.addBinding({MidiMsgType::CC, 2, 16, MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 2, 17, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 2, 18, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 2, 19, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 2, 20, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 2, 21, MidiAction::TEMPO_SPEED, 2, MidiJogMode::SPREAD_64, "Tempo Fader"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 35, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 36, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 37, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 38, MidiAction::HEADPHONE_CUE, 2, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::CC, 2, 25, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Jog Wheel"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 42, MidiAction::JOG_TOUCH, 2, MidiJogMode::SPREAD_64, "Jog Wheel Touch"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 28, MidiAction::LOOP_TOGGLE, 2, MidiJogMode::SPREAD_64, "Loop Toggle"});
    p.addBinding({MidiMsgType::CC, 2, 26, MidiAction::LOOP_SIZE_CHANGE, 2, MidiJogMode::SPREAD_64, "Loop Size Encoder"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 21, MidiAction::PITCH_BEND_DOWN, 2, MidiJogMode::SPREAD_64, "Pitch Bend Down (<<)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 22, MidiAction::PITCH_BEND_UP, 2, MidiJogMode::SPREAD_64, "Pitch Bend Up (>>)"});

    return p;
}
