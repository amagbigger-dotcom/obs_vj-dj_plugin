#pragma once

#include "controllers/midi_types.hpp"
#include <cstdint>

class MidiJogWheel {
public:
    explicit MidiJogWheel(MidiJogMode mode = MidiJogMode::SPREAD_64, double sensitivity = 1.0);

    void setMode(MidiJogMode mode) { mode_ = mode; }
    void setSensitivity(double s) { sensitivity_ = s; }
    void setTouched(bool touched) { touched_ = touched; }
    bool isTouched() const { return touched_; }

    // Decodes incoming MIDI data byte into relative movement delta (-inf .. +inf)
    double processDelta(uint8_t value);

private:
    MidiJogMode mode_;
    double sensitivity_;
    bool touched_ = false;
    uint8_t last_absolute_val_ = 64;
    bool has_prev_absolute_ = false;
};
