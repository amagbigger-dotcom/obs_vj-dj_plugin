#include "midi-handler.hpp"

#include "deck-engine.hpp"

#include <obs-module.h>

#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <cctype>
#include <cstring>
#include <string>

#pragma comment(lib, "winmm.lib")

namespace {

HMIDIIN g_midi_in = nullptr;
std::atomic<bool> g_connected{false};
char g_device_name[MAXPNAMELEN] = {};

std::string lowercase(std::string value)
{
    for (char &character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

bool is_target_device(const char *device_name)
{
    if (!device_name)
        return false;

    const std::string name = lowercase(device_name);

    // A Traktor S2 nevét keressük, hogy ne nyisson meg véletlenül
    // egy másik, nem ehhez a kiosztáshoz tartozó MIDI eszközt.
    return name.find("traktor") != std::string::npos &&
           name.find("s2") != std::string::npos;
}

void handle_control_change(DeckPair *decks,
                           unsigned char channel,
                           unsigned char controller,
                           unsigned char value)
{
    if (!decks)
        return;

    // A terv szerint a crossfader Ch 1 / CC 8.
    if (channel == 0 && controller == 8) {
        decks->setCrossfader(static_cast<double>(value) / 127.0);
        return;
    }

    DeckPlayer *deck = nullptr;

    if (channel == 0) {
        deck = &decks->deckA();
    } else if (channel == 1) {
        deck = &decks->deckB();
    } else {
        return;
    }

    switch (controller) {
    case 16:
        deck->setEqHi(static_cast<float>(value) / 63.5f);
        break;
    case 17:
        deck->setEqMid(static_cast<float>(value) / 63.5f);
        break;
    case 18:
        deck->setEqLow(static_cast<float>(value) / 63.5f);
        break;
    case 19:
        deck->setFilterVal((static_cast<float>(value) - 63.5f) / 63.5f);
        break;
    case 20:
        deck->setVolume(static_cast<float>(value) / 127.0f);
        break;
    case 21: {
        const int speed =
            50 + static_cast<int>((static_cast<unsigned int>(value) * 100U) / 127U);
        deck->setSpeed(speed);
        break;
    }
    default:
        break;
    }
}

void handle_note(DeckPair *decks,
                 unsigned char channel,
                 unsigned char note)
{
    if (!decks)
        return;

    DeckPlayer *deck = nullptr;
    DeckPlayer *other_deck = nullptr;

    if (channel == 0) {
        deck = &decks->deckA();
        other_deck = &decks->deckB();
    } else if (channel == 1) {
        deck = &decks->deckB();
        other_deck = &decks->deckA();
    } else {
        return;
    }

    switch (note) {
    case 35:
        deck->togglePlayback();
        break;
    case 36:
        deck->cue();
        break;
    case 37:
        if (channel == 0)
            decks->syncTempoBtoA();
        else
            decks->syncTempoAtoB();
        break;
    case 38:
        deck->setHeadphoneCue(!deck->isHeadphoneCue());
        break;
    default:
        break;
    }
}

void CALLBACK midi_in_proc(HMIDIIN,
                           UINT message,
                           DWORD_PTR,
                           DWORD_PTR parameter,
                           DWORD_PTR)
{
    if (message != MIM_DATA)
        return;

    const DWORD packed_message = static_cast<DWORD>(parameter);
    const auto status =
        static_cast<unsigned char>(packed_message & 0xFFU);
    const auto data1 =
        static_cast<unsigned char>((packed_message >> 8U) & 0xFFU);
    const auto data2 =
        static_cast<unsigned char>((packed_message >> 16U) & 0xFFU);

    const unsigned char message_type = status & 0xF0U;
    const unsigned char channel = status & 0x0FU;

    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    if (message_type == 0xB0U) {
        handle_control_change(decks, channel, data1, data2);
        return;
    }

    // A Note On 0 velocity MIDI szerint a Note Off megfelelője.
    // Gombműveletet csak tényleges lenyomáskor indítunk.
    if (message_type == 0x90U && data2 > 0)
        handle_note(decks, channel, data1);
}

} // namespace

void vdj_midi_init()
{
    if (g_midi_in)
        return;

    g_connected.store(false, std::memory_order_relaxed);
    g_device_name[0] = '\0';

    const UINT device_count = midiInGetNumDevs();
    UINT target_device_id = MIDI_MAPPER;
    bool found_target = false;

    for (UINT device_id = 0; device_id < device_count; ++device_id) {
        MIDIINCAPSA capabilities = {};

        if (midiInGetDevCapsA(
                device_id, &capabilities, sizeof(capabilities)) != MMSYSERR_NOERROR) {
            continue;
        }

        if (!is_target_device(capabilities.szPname))
            continue;

        target_device_id = device_id;
        found_target = true;

        std::strncpy(
            g_device_name,
            capabilities.szPname,
            sizeof(g_device_name) - 1);
        g_device_name[sizeof(g_device_name) - 1] = '\0';

        break;
    }

    if (!found_target) {
        blog(LOG_INFO,
             "[obs-vj-mixer] Traktor Kontrol S2 MIDI input device not found.");
        return;
    }

    MMRESULT result = midiInOpen(
        &g_midi_in,
        target_device_id,
        reinterpret_cast<DWORD_PTR>(midi_in_proc),
        0,
        CALLBACK_FUNCTION);

    if (result != MMSYSERR_NOERROR) {
        g_midi_in = nullptr;
        g_device_name[0] = '\0';
        blog(LOG_WARNING,
             "[obs-vj-mixer] Could not open Traktor S2 MIDI input (error %u).",
             static_cast<unsigned int>(result));
        return;
    }

    result = midiInStart(g_midi_in);

    if (result != MMSYSERR_NOERROR) {
        midiInClose(g_midi_in);
        g_midi_in = nullptr;
        g_device_name[0] = '\0';
        blog(LOG_WARNING,
             "[obs-vj-mixer] Could not start Traktor S2 MIDI input (error %u).",
             static_cast<unsigned int>(result));
        return;
    }

    g_connected.store(true, std::memory_order_relaxed);
    blog(LOG_INFO,
         "[obs-vj-mixer] MIDI connected: %s",
         g_device_name);
}

void vdj_midi_shutdown()
{
    g_connected.store(false, std::memory_order_relaxed);

    if (g_midi_in) {
        midiInStop(g_midi_in);
        midiInReset(g_midi_in);
        midiInClose(g_midi_in);
        g_midi_in = nullptr;
    }

    g_device_name[0] = '\0';
}

bool vdj_midi_is_connected()
{
    return g_connected.load(std::memory_order_relaxed);
}

const char *vdj_midi_get_device_name()
{
    return g_device_name;
}
