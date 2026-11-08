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
    const unsigned char channel  = status & 0x0F;

    if (msg_type == 0xB0) {
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
        case 16:
            deck->setRed(static_cast<float>(data2) / 63.5f);
            break;
        case 17:
            deck->setGreen(static_cast<float>(data2) / 63.5f);
            break;
        case 18:
            deck->setBlue(static_cast<float>(data2) / 63.5f);
            break;
        case 19:
            deck->setSaturation(static_cast<float>(data2) / 63.5f);
            break;
        case 20:
            deck->setVolume(static_cast<float>(data2) / 127.0f);
            break;
        case 21:
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
        case 35:
            deck->togglePlayback();
            break;
        case 36:
            deck->cue();
            break;
        case 37:
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

    UINT target_dev_id = 0;
    bool found_traktor = false;

    for (UINT i = 0; i < num_devs; ++i) {
        MIDIINCAPSA caps = {0};
        if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
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
    }

    MMRESULT res = midiInOpen(&g_midi_in, target_dev_id, reinterpret_cast<DWORD_PTR>(midi_in_proc), 0, CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        g_midi_in = nullptr;
        g_connected.store(false);
        return;
    }

    res = midiInStart(g_midi_in);
    if (res != MMSYSERR_NOERROR) {
        midiInClose(g_midi_in);
        g_midi_in = nullptr;
        g_connected.store(false);
        return;
    }

    g_connected.store(true);
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
}

bool vdj_midi_is_connected()
{
    return g_connected.load();
}

const char *vdj_midi_get_device_name()
{
    return g_device_name;
}
