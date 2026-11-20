#include "controllers/midi_mapping.hpp"

MidiControllerProfile MidiControllerProfile::createPioneerDDJ400()
{
    MidiControllerProfile p("DDJ-400", "Pioneer DJ");

    // Global
    p.addBinding({MidiMsgType::CC, 1, 31, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader"});
    p.addBinding({MidiMsgType::CC, 1, 64, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary Encoder"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 65, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Browse Push"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 70, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 71, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B"});

    // Deck A (Ch 1)
    p.addBinding({MidiMsgType::CC, 1, 19, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Channel Fader"});
    p.addBinding({MidiMsgType::CC, 1, 7,  MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 11, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 15, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 23, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 1, 0,  MidiAction::TEMPO_SPEED, 1, MidiJogMode::SPREAD_64, "Tempo Slider"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 11, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 12, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 88, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 84, MidiAction::HEADPHONE_CUE, 1, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::CC, 1, 33, MidiAction::JOG_SCRATCH, 1, MidiJogMode::SPREAD_64, "Jog Wheel"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 54, MidiAction::JOG_TOUCH, 1, MidiJogMode::SPREAD_64, "Jog Wheel Touch"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 16, MidiAction::LOOP_TOGGLE, 1, MidiJogMode::SPREAD_64, "Auto Loop (4 Beat)"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 17, MidiAction::LOOP_HALVE, 1, MidiJogMode::SPREAD_64, "Loop 1/2X"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 18, MidiAction::LOOP_DOUBLE, 1, MidiJogMode::SPREAD_64, "Loop 2X"});

    // Deck B (Ch 2)
    p.addBinding({MidiMsgType::CC, 2, 19, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Channel Fader"});
    p.addBinding({MidiMsgType::CC, 2, 7,  MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 2, 11, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 2, 15, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 2, 23, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 2, 0,  MidiAction::TEMPO_SPEED, 2, MidiJogMode::SPREAD_64, "Tempo Slider"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 11, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 12, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 88, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 84, MidiAction::HEADPHONE_CUE, 2, MidiJogMode::SPREAD_64, "Headphone CUE"});
    p.addBinding({MidiMsgType::CC, 2, 33, MidiAction::JOG_SCRATCH, 2, MidiJogMode::SPREAD_64, "Jog Wheel"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 54, MidiAction::JOG_TOUCH, 2, MidiJogMode::SPREAD_64, "Jog Wheel Touch"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 16, MidiAction::LOOP_TOGGLE, 2, MidiJogMode::SPREAD_64, "Auto Loop (4 Beat)"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 17, MidiAction::LOOP_HALVE, 2, MidiJogMode::SPREAD_64, "Loop 1/2X"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 18, MidiAction::LOOP_DOUBLE, 2, MidiJogMode::SPREAD_64, "Loop 2X"});

    return p;
}

MidiControllerProfile MidiControllerProfile::createPioneerDDJFLX4()
{
    // DDJ-FLX4 shares the core MIDI map of DDJ-400 with Smart Fader additions
    MidiControllerProfile p = createPioneerDDJ400();
    p.setName("DDJ-FLX4");
    return p;
}
