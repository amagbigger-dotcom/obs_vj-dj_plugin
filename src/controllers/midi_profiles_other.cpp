#include "controllers/midi_mapping.hpp"

MidiControllerProfile MidiControllerProfile::createNumarkMixtrackPro()
{
    MidiControllerProfile p("Mixtrack Pro", "Numark");
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::BIN_OFFSET, "Crossfader"});
    p.addBinding({MidiMsgType::CC, 1, 26, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::BIN_OFFSET, "Browse Rotary"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 26, MidiAction::LOAD_SELECTED, 0, MidiJogMode::BIN_OFFSET, "Browse Push"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 27, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::BIN_OFFSET, "Load Deck A"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 28, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::BIN_OFFSET, "Load Deck B"});

    // Deck A
    p.addBinding({MidiMsgType::CC, 1, 10, MidiAction::VOLUME, 1, MidiJogMode::BIN_OFFSET, "Volume"});
    p.addBinding({MidiMsgType::CC, 1, 12, MidiAction::EQ_HI, 1, MidiJogMode::BIN_OFFSET, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 13, MidiAction::EQ_MID, 1, MidiJogMode::BIN_OFFSET, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 14, MidiAction::EQ_LOW, 1, MidiJogMode::BIN_OFFSET, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 15, MidiAction::FILTER, 1, MidiJogMode::BIN_OFFSET, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 0, MidiAction::PLAY_PAUSE, 1, MidiJogMode::BIN_OFFSET, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 1, MidiAction::CUE, 1, MidiJogMode::BIN_OFFSET, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 2, MidiAction::SYNC, 1, MidiJogMode::BIN_OFFSET, "Sync"});

    return p;
}

MidiControllerProfile MidiControllerProfile::createHerculesInpulse300()
{
    MidiControllerProfile p("DJControl Inpulse 300", "Hercules");
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader"});
    p.addBinding({MidiMsgType::CC, 1, 33, MidiAction::BROWSE_SCROLL, 0, MidiJogMode::SPREAD_64, "Browse Rotary"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 33, MidiAction::LOAD_SELECTED, 0, MidiJogMode::SPREAD_64, "Browse Push"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 34, MidiAction::LOAD_SELECTED_A, 1, MidiJogMode::SPREAD_64, "Load Deck A"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 35, MidiAction::LOAD_SELECTED_B, 2, MidiJogMode::SPREAD_64, "Load Deck B"});

    // Deck A
    p.addBinding({MidiMsgType::CC, 1, 1, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 1, 2, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 3, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 4, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 5, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 1, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 2, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 3, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});

    return p;
}
