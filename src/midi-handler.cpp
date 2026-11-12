#include "midi-handler.hpp"
#include "deck-engine.hpp"

#include <obs-module.h>

#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#pragma comment(lib, "winmm.lib")

namespace {

HMIDIIN g_midi_in = nullptr;
HMIDIOUT g_midi_out = nullptr;
std::atomic<bool> g_connected{false};
std::atomic<bool> g_clock_send_enabled{false};
std::atomic<bool> g_clock_thread_running{false};
std::thread g_clock_thread;
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
    return (name.find("traktor") != std::string::npos && name.find("s2") != std::string::npos) ||
           name.find("s3") != std::string::npos ||
           name.find("dj") != std::string::npos ||
           name.find("midi") != std::string::npos;
}

void handle_control_change(DeckPlayer *deck,
                           unsigned char channel,
                           unsigned char controller,
                           unsigned char value)
{
    UNUSED_PARAMETER(channel);
    if (!deck)
        return;

    switch (controller) {
    case 20: // Volume Fader
        deck->setVolume(static_cast<float>(value) / 127.0f);
        break;
    case 21: { // Tempo / Pitch
        const int speed =
            50 + static_cast<int>((static_cast<unsigned int>(value) * 100U) / 127U);
        deck->applySpeedSettings(speed);
        break;
    }
    default:
        break;
    }
}

void handle_note(DeckPlayer *deck,
                 unsigned char channel,
                 unsigned char note)
{
    UNUSED_PARAMETER(channel);
    if (!deck)
        return;

    switch (note) {
    case 35: // Play / Pause toggle
        deck->togglePlayback();
        break;
    case 36: // Cue
        deck->cue();
        break;
    case 37: // Sync
        deck->setSync(!deck->isSync());
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
    const auto status = static_cast<unsigned char>(packed_message & 0xFFU);
    const auto data1 = static_cast<unsigned char>((packed_message >> 8U) & 0xFFU);
    const auto data2 = static_cast<unsigned char>((packed_message >> 16U) & 0xFFU);

    DeckPlayer *deck = vj_active_deck();

    // MIDI System Real-Time Messages
    if (status == 0xF8U) { // Clock tick
        static auto last_tick_tp = std::chrono::steady_clock::now();
        static std::vector<int64_t> tick_intervals_us;
        const auto now_tp = std::chrono::steady_clock::now();
        const auto delta_us = std::chrono::duration_cast<std::chrono::microseconds>(now_tp - last_tick_tp).count();
        last_tick_tp = now_tp;

        if (delta_us > 1000 && delta_us < 100000) {
            tick_intervals_us.push_back(delta_us);
            if (tick_intervals_us.size() > 24)
                tick_intervals_us.erase(tick_intervals_us.begin());

            if (tick_intervals_us.size() >= 12 && deck && deck->isSync()) {
                int64_t sum_us = 0;
                for (int64_t interval : tick_intervals_us)
                    sum_us += interval;
                const double avg_tick_us = static_cast<double>(sum_us) / static_cast<double>(tick_intervals_us.size());
                if (avg_tick_us > 100.0) {
                    const double incoming_bpm = 60000000.0 / (avg_tick_us * 24.0);
                    if (incoming_bpm >= 40.0 && incoming_bpm <= 300.0) {
                        deck->syncToBpm(incoming_bpm);
                    }
                }
            }
        }
        return;
    }

    if (status == 0xFAU) { // MIDI Start
        if (deck && deck->isSync()) deck->setPlaying(true);
        return;
    }

    if (status == 0xFCU) { // MIDI Stop
        if (deck && deck->isSync()) deck->setPlaying(false);
        return;
    }

    const unsigned char message_type = status & 0xF0U;
    const unsigned char channel = status & 0x0FU;

    if (message_type == 0xB0U) {
        handle_control_change(deck, channel, data1, data2);
        return;
    }

    // Note On with vel > 0
    if (message_type == 0x90U && data2 > 0)
        handle_note(deck, channel, data1);
}

void clock_sender_loop()
{
    while (g_clock_thread_running.load(std::memory_order_relaxed)) {
        if (!g_clock_send_enabled.load(std::memory_order_relaxed) || !g_midi_out) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        DeckPlayer *deck = vj_active_deck();
        const double bpm = deck ? deck->effectiveBpm() : 128.0;
        const double eff_bpm = (bpm >= 30.0) ? bpm : 128.0;
        const double tick_interval_us = 60000000.0 / (eff_bpm * 24.0);

        const auto start_tp = std::chrono::steady_clock::now();
        midiOutShortMsg(g_midi_out, 0xF8);

        const auto target_tp = start_tp + std::chrono::microseconds(static_cast<int64_t>(tick_interval_us));
        while (std::chrono::steady_clock::now() < target_tp && g_clock_thread_running.load(std::memory_order_relaxed)) {
            const auto remaining_us = std::chrono::duration_cast<std::chrono::microseconds>(target_tp - std::chrono::steady_clock::now()).count();
            if (remaining_us > 2000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                std::this_thread::yield();
            }
        }
    }
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

        if (is_target_device(capabilities.szPname)) {
            target_device_id = device_id;
            found_target = true;
            std::strncpy(
                g_device_name,
                capabilities.szPname,
                sizeof(g_device_name) - 1);
            g_device_name[sizeof(g_device_name) - 1] = '\0';
            break;
        }
    }

    if (!found_target && device_count > 0) {
        MIDIINCAPSA capabilities = {};
        if (midiInGetDevCapsA(0, &capabilities, sizeof(capabilities)) == MMSYSERR_NOERROR) {
            target_device_id = 0;
            found_target = true;
            std::strncpy(
                g_device_name,
                capabilities.szPname,
                sizeof(g_device_name) - 1);
            g_device_name[sizeof(g_device_name) - 1] = '\0';
        }
    }

    if (found_target) {
        MMRESULT result = midiInOpen(
            &g_midi_in,
            target_device_id,
            reinterpret_cast<DWORD_PTR>(midi_in_proc),
            0,
            CALLBACK_FUNCTION);

        if (result == MMSYSERR_NOERROR) {
            result = midiInStart(g_midi_in);
            if (result == MMSYSERR_NOERROR) {
                g_connected.store(true, std::memory_order_relaxed);
                blog(LOG_INFO, "[obs-vj-deck] MIDI connected: %s", g_device_name);
            } else {
                midiInClose(g_midi_in);
                g_midi_in = nullptr;
                g_device_name[0] = '\0';
            }
        }
    }

    // Auto-connect to first available MIDI OUT device
    const UINT out_count = midiOutGetNumDevs();
    for (UINT i = 0; i < out_count; ++i) {
        MIDIOUTCAPSA caps = {};
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            if (is_target_device(caps.szPname) || (i == 0 && !g_midi_out)) {
                vdj_midi_open_output_device(static_cast<int>(i));
            }
        }
    }

    g_clock_thread_running.store(true, std::memory_order_relaxed);
    g_clock_thread = std::thread(clock_sender_loop);
}

void vdj_midi_shutdown()
{
    g_connected.store(false, std::memory_order_relaxed);
    g_clock_send_enabled.store(false, std::memory_order_relaxed);
    g_clock_thread_running.store(false, std::memory_order_relaxed);

    if (g_clock_thread.joinable())
        g_clock_thread.join();

    if (g_midi_in) {
        midiInStop(g_midi_in);
        midiInReset(g_midi_in);
        midiInClose(g_midi_in);
        g_midi_in = nullptr;
    }

    if (g_midi_out) {
        midiOutReset(g_midi_out);
        midiOutClose(g_midi_out);
        g_midi_out = nullptr;
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

void vdj_midi_set_clock_send(bool enable)
{
    g_clock_send_enabled.store(enable, std::memory_order_relaxed);
}

bool vdj_midi_is_clock_send()
{
    return g_clock_send_enabled.load(std::memory_order_relaxed);
}

std::vector<std::string> vdj_midi_get_output_devices()
{
    std::vector<std::string> devices;
    const UINT count = midiOutGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        MIDIOUTCAPSA caps = {};
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            devices.push_back(caps.szPname);
        }
    }
    return devices;
}

bool vdj_midi_open_output_device(int device_id)
{
    if (g_midi_out) {
        midiOutReset(g_midi_out);
        midiOutClose(g_midi_out);
        g_midi_out = nullptr;
    }

    if (device_id < 0)
        return false;

    MMRESULT res = midiOutOpen(&g_midi_out, static_cast<UINT>(device_id), 0, 0, CALLBACK_NULL);
    return res == MMSYSERR_NOERROR;
}
