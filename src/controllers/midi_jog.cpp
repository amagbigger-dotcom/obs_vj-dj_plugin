#include "controllers/midi_jog.hpp"
#include <cmath>

MidiJogWheel::MidiJogWheel(MidiJogMode mode, double sensitivity)
    : mode_(mode), sensitivity_(sensitivity)
{
}

double MidiJogWheel::processDelta(uint8_t value)
{
    double delta = 0.0;

    switch (mode_) {
    case MidiJogMode::SPREAD_64: // 64 = Center; 65 = +1, 63 = -1 (Pioneer, Traktor)
        if (value >= 64) {
            delta = static_cast<double>(value - 64);
        } else {
            delta = static_cast<double>(static_cast<int>(value) - 64);
        }
        break;

    case MidiJogMode::SIGN_MAGNITUDE: // Bit 6 gives direction
        if (value & 0x40) {
            delta = static_cast<double>(value & 0x3F);
        } else {
            delta = -static_cast<double>(value & 0x3F);
        }
        break;

    case MidiJogMode::BIN_OFFSET: // 1..63 = +1..+63; 127..65 = -1..-63 (Numark, Denon)
        if (value < 64) {
            delta = static_cast<double>(value);
        } else {
            delta = static_cast<double>(static_cast<int>(value) - 128);
        }
        break;

    case MidiJogMode::ABSOLUTE:
        if (!has_prev_absolute_) {
            last_absolute_val_ = value;
            has_prev_absolute_ = true;
            return 0.0;
        }
        int diff = static_cast<int>(value) - static_cast<int>(last_absolute_val_);
        // Handle 0/127 boundary wrap
        if (diff > 64) diff -= 128;
        else if (diff < -64) diff += 128;
        delta = static_cast<double>(diff);
        last_absolute_val_ = value;
        break;
    }

    return delta * sensitivity_;
}
