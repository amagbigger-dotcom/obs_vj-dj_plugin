#include "midi-handler.hpp"
#include "deck-engine.hpp"

#include <obs-module.h>

#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <cstring>
#include <string>

#pragma comment(lib, "winmm.lib")

namespace {

HMIDIIN g_midi_in = nullptr;
std::atomic<bool> g_connected{false};
char g_device_name[MAXPNAMELEN] = {0};

void CALLBACK midi_in_proc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void)hMidiIn;
    (void)dwInstance;
    (void)dwParam2;

    if (wMsg != MIM_DATA)
        return;

    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    const unsigned char status = static_cast<unsigned char>(dwParam1 & 0xFF);
    const unsigned char data1  = static_cast<unsigned char>((dwParam1 >> 8) & 0xFF);
    const unsigned char data2  = static_cast<unsigned char>((dwParam1 >> 16) & 0xFF);

    const unsigned char msg_type = status & 0xF0;
    const unsigned char channel  = status & 0x0F; // 0 = Ch 1 (Deck A), 1 = Ch 2 (Deck B)

    // 1. Control Change (Potmeterek, Federek, Crossfader)
    if (msg_type == 0xB0) {
        // Master Crossfader: CC 8 (vagy CC 10)
        if (data1 == 8 || data1 == 10) {
            const double xfade = static_cast<double>(data2) / 127.0;
            decks->setCrossfader(xfade);
            return;
        }

        DeckPlayer *deck = nullptr;
        if (channel == 0) {
            deck = &decks->deckA();
        } else if (channel == 1) {
            deck = &decks->deckB();
        }

        if (!deck)
            return;

        switch (data1) {
        case 16: // HI Knob -> Red Tint (0.0f - 2.0f)
            deck->setRed(static_cast<float>(data2) / 63.5f);
            break;
        case 17: // MID Knob -> Green Tint (0.0f - 2.0f)
            deck->setGreen(static_cast<float>(data2) / 63.5f);
            break;
        case 18: // LOW Knob -> Blue Tint (0.0f - 2.0f)
            deck->setBlue(static_cast<float>(data2) / 63.5f);
            break;
        case 19: // FILTER Knob -> Saturation (0.0f - 2.0f)
            deck->setSaturation(static_cast<float>(data2) / 63.5f);
            break;
        case 20: // Volume Fader (0.0f - 1.0f)
            deck->setVolume(static_cast<float>(data2) / 127.0f);
            break;
        case 21: // Tempo / Pitch Fader (50% - 150%)
            {
                const int speed = 50 + static_cast<int>((data2 * 100.0f) / 127.0f);
                deck->setSpeed(speed);
            }
            break;
        default:
            break;
        }
        return;
    }

    // 2. Note On (Gombok: Play, Cue, Sync)
    if (msg_type == 0x90 && data2 > 0) {
        DeckPlayer *deck = nullptr;
        DeckPlayer *other_deck = nullptr;

        if (channel == 0) {
            deck = &decks->deckA();
            other_deck = &decks->deckB();
        } else if (channel == 1) {
            deck = &decks->deckB();
            other_deck = &decks->deckA();
        }

        if (!deck)
            return;

        switch (data1) {
        case 35: // PLAY / PAUSE
            deck->togglePlayback();
            break;
        case 36: // CUE
            deck->cue();
            break;
        case 37: // SYNC
            if (other_deck) {
                deck->setSpeed(other_deck->speed());
            }
            break;
        default:
            break;
        }
        return;
    }
}

} // namespace

void vdj_midi_init()
{
    if (g_midi_in)
        return;

    const UINT num_devs = midiInGetNumDevs();
    if (num_devs == 0) {
        blog(LOG_INFO, "[obs-vj-mixer] No MIDI input devices found on system.");
        return;
    }

    blog(LOG_INFO, "[obs-vj-mixer] Enumerating %u MIDI input device(s)...", num_devs);

    UINT target_dev_id = 0;
    bool found_traktor = false;

    for (UINT i = 0; i < num_devs; ++i) {
        MIDIINCAPSA caps = {0};
        if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            blog(LOG_INFO, "[obs-vj-mixer] MIDI Dev [%u]: %s", i, caps.szPname);
            std::string name(caps.szPname);
            if (name.find("Traktor") != std::string::npos ||
                name.find("Kontrol") != std::string::npos ||
                name.find("S2") != std::string::npos) {
                target_dev_id = i;
                found_traktor = true;
                std::strncpy(g_device_name, caps.szPname, sizeof(g_device_name) - 1);
            }
        }
    }

    if (!found_traktor) {
        MIDIINCAPSA caps = {0};
        if (midiInGetDevCapsA(0, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            std::strncpy(g_device_name, caps.szPname, sizeof(g_device_name) - 1);
        }
        target_dev_id = 0;
        blog(LOG_WARNING, "[obs-vj-mixer] Traktor S2 not identified by name. Opening default MIDI Dev [0]: %s", g_device_name);
    } else {
        blog(LOG_INFO, "[obs-vj-mixer] Traktor S2 detected on Dev [%u]: %s", target_dev_id, g_device_name);
    }

    MMRESULT res = midiInOpen(&g_midi_in, target_dev_id, reinterpret_cast<DWORD_PTR>(midi_in_proc), 0, CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        blog(LOG_ERROR, "[obs-vj-mixer] Failed to open MIDI device [%u] (err=%u)", target_dev_id, res);
        g_midi_in = nullptr;
        g_connected.store(false);
        return;
    }

    res = midiInStart(g_midi_in);
    if (res != MMSYSERR_NOERROR) {
        blog(LOG_ERROR, "[obs-vj-mixer] Failed to start MIDI input on [%u] (err=%u)", target_dev_id, res);
        midiInClose(g_midi_in);
        g_midi_in = nullptr;
        g_connected.store(false);
        return;
    }

    g_connected.store(true);
    blog(LOG_INFO, "[obs-vj-mixer] MIDI listener successfully connected to '%s'", g_device_name);
}

void vdj_midi_shutdown()
{
    if (g_midi_in) {
        midiInStop(g_midi_in);
        midiInReset(g_midi_in);
        midiInClose(g_midi_in);
        g_midi_in = nullptr;
    }
    g_connected.store(false);
    std::memset(g_device_name, 0, sizeof(g_device_name));
    blog(LOG_INFO, "[obs-vj-mixer] MIDI listener stopped.");
}

bool vdj_midi_is_connected()
{
    return g_connected.load();
}

const char *vdj_midi_get_device_name()
{
    return g_device_name;
}
