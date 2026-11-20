#include "controllers/midi_handler.hpp"
#include "controllers/midi_dispatcher.hpp"
#include "gui/settings_dialog.hpp"
#include "engine/deck_engine.hpp"

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
std::atomic<bool> g_feedback_thread_running{false};
std::thread g_feedback_thread;
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
    if (name.find("network") != std::string::npos || name.find("rtpmidi") != std::string::npos)
        return false;

    return name.find("kaoss") != std::string::npos ||
           name.find("traktor") != std::string::npos ||
           name.find("korg") != std::string::npos ||
           name.find("pioneer") != std::string::npos ||
           name.find("numark") != std::string::npos ||
           name.find("hercules") != std::string::npos ||
           name.find("reloop") != std::string::npos ||
           name.find("denon") != std::string::npos ||
           name.find("dj") != std::string::npos;
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

    // Data-driven Controller Dispatcher
    MidiDispatcher::instance().dispatch(status, data1, data2);
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

void feedback_sender_loop()
{
    int refresh_counter = 0;
    while (g_feedback_thread_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        if (!g_connected.load(std::memory_order_relaxed) || !g_midi_out) {
            continue;
        }
        refresh_counter++;
        const bool force_refresh = (refresh_counter >= 25);
        if (force_refresh) refresh_counter = 0;
        MidiDispatcher::instance().sendFeedback(force_refresh);
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

    const auto &cfg = SettingsDialog::config();
    const QString saved_in_dev = cfg.midi_input_device;

    // 1. Try to open the explicitly configured MIDI input device first
    if (!saved_in_dev.isEmpty()) {
        for (UINT device_id = 0; device_id < device_count; ++device_id) {
            MIDIINCAPSA capabilities = {};
            if (midiInGetDevCapsA(device_id, &capabilities, sizeof(capabilities)) == MMSYSERR_NOERROR) {
                if (saved_in_dev == QString::fromLocal8Bit(capabilities.szPname)) {
                    target_device_id = device_id;
                    found_target = true;
                    std::strncpy(g_device_name, capabilities.szPname, sizeof(g_device_name) - 1);
                    g_device_name[sizeof(g_device_name) - 1] = '\0';
                    break;
                }
            }
        }
    }

    // 2. If not found or empty, search for known DJ controllers (Kaoss, Traktor, Pioneer, etc.)
    if (!found_target) {
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
                blog(LOG_INFO, "[obs-vj-deck] MIDI connected: %s (id: %u)", g_device_name, target_device_id);
            } else {
                midiInClose(g_midi_in);
                g_midi_in = nullptr;
                g_device_name[0] = '\0';
            }
        }
    }

    // Initialize active profile based on saved preset or detected hardware
    const QString preset = cfg.active_preset.toLower();
    std::string dev_lower = lowercase(g_device_name);
    if (preset.contains("kaoss") || dev_lower.find("kaoss") != std::string::npos) {
        MidiDispatcher::instance().setProfile(MidiControllerProfile::createKorgKaossDJ());
        blog(LOG_INFO, "[obs-vj-deck] Loaded Korg Kaoss DJ mapping profile");
    } else if (preset.contains("400")) {
        MidiDispatcher::instance().setProfile(MidiControllerProfile::createPioneerDDJ400());
        blog(LOG_INFO, "[obs-vj-deck] Loaded Pioneer DDJ-400 mapping profile");
    } else if (preset.contains("flx4")) {
        MidiDispatcher::instance().setProfile(MidiControllerProfile::createPioneerDDJFLX4());
        blog(LOG_INFO, "[obs-vj-deck] Loaded Pioneer DDJ-FLX4 mapping profile");
    } else if (preset.contains("mixtrack")) {
        MidiDispatcher::instance().setProfile(MidiControllerProfile::createNumarkMixtrackPro());
        blog(LOG_INFO, "[obs-vj-deck] Loaded Numark Mixtrack mapping profile");
    } else if (preset.contains("inpulse")) {
        MidiDispatcher::instance().setProfile(MidiControllerProfile::createHerculesInpulse300());
        blog(LOG_INFO, "[obs-vj-deck] Loaded Hercules Inpulse 300 mapping profile");
    } else {
        MidiDispatcher::instance().setProfile(MidiControllerProfile::createTraktorS2MK3());
        blog(LOG_INFO, "[obs-vj-deck] Loaded Traktor S2 MK3 mapping profile");
    }

    // Auto-connect to appropriate MIDI OUT device (saved preference -> match input device -> known DJ target)
    const QString saved_out_dev = cfg.midi_output_device;
    const UINT out_count = midiOutGetNumDevs();
    bool found_out = false;

    if (!saved_out_dev.isEmpty()) {
        for (UINT i = 0; i < out_count; ++i) {
            MIDIOUTCAPSA caps = {};
            if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                if (saved_out_dev == QString::fromLocal8Bit(caps.szPname)) {
                    vdj_midi_open_output_device(static_cast<int>(i));
                    found_out = true;
                    break;
                }
            }
        }
    }

    if (!found_out && g_device_name[0] != '\0') {
        const std::string in_lower = lowercase(g_device_name);
        for (UINT i = 0; i < out_count; ++i) {
            MIDIOUTCAPSA caps = {};
            if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                const std::string out_lower = lowercase(caps.szPname);
                if ((out_lower.find("kaoss") != std::string::npos && in_lower.find("kaoss") != std::string::npos) ||
                    out_lower == in_lower) {
                    vdj_midi_open_output_device(static_cast<int>(i));
                    found_out = true;
                    break;
                }
            }
        }
    }

    if (!found_out) {
        for (UINT i = 0; i < out_count; ++i) {
            MIDIOUTCAPSA caps = {};
            if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                if (is_target_device(caps.szPname)) {
                    vdj_midi_open_output_device(static_cast<int>(i));
                    found_out = true;
                    break;
                }
            }
        }
    }

    g_clock_thread_running.store(true, std::memory_order_relaxed);
    g_clock_thread = std::thread(clock_sender_loop);

    g_feedback_thread_running.store(true, std::memory_order_relaxed);
    g_feedback_thread = std::thread(feedback_sender_loop);
}

void vdj_midi_shutdown()
{
    g_connected.store(false, std::memory_order_relaxed);
    g_clock_send_enabled.store(false, std::memory_order_relaxed);
    g_clock_thread_running.store(false, std::memory_order_relaxed);
    g_feedback_thread_running.store(false, std::memory_order_relaxed);

    if (g_clock_thread.joinable())
        g_clock_thread.join();

    if (g_feedback_thread.joinable())
        g_feedback_thread.join();

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

std::vector<std::string> vdj_midi_get_input_devices()
{
    std::vector<std::string> devices;
    const UINT count = midiInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        MIDIINCAPSA caps = {};
        if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            devices.push_back(caps.szPname);
        }
    }
    return devices;
}

bool vdj_midi_open_input_device(int device_id)
{
    if (g_midi_in) {
        midiInStop(g_midi_in);
        midiInClose(g_midi_in);
        g_midi_in = nullptr;
    }

    g_connected.store(false, std::memory_order_relaxed);
    g_device_name[0] = '\0';

    if (device_id < 0 || device_id >= static_cast<int>(midiInGetNumDevs()))
        return false;

    MIDIINCAPSA caps = {};
    if (midiInGetDevCapsA(static_cast<UINT>(device_id), &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        std::strncpy(g_device_name, caps.szPname, sizeof(g_device_name) - 1);
        g_device_name[sizeof(g_device_name) - 1] = '\0';
    }

    MMRESULT result = midiInOpen(
        &g_midi_in,
        static_cast<UINT>(device_id),
        reinterpret_cast<DWORD_PTR>(midi_in_proc),
        0,
        CALLBACK_FUNCTION);

    if (result == MMSYSERR_NOERROR) {
        result = midiInStart(g_midi_in);
        if (result == MMSYSERR_NOERROR) {
            g_connected.store(true, std::memory_order_relaxed);
            blog(LOG_INFO, "[obs-vj-deck] MIDI switched to input device: %s", g_device_name);
            return true;
        } else {
            midiInClose(g_midi_in);
            g_midi_in = nullptr;
            g_device_name[0] = '\0';
        }
    }
    return false;
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

void vdj_midi_set_profile(const MidiControllerProfile &p)
{
    MidiDispatcher::instance().setProfile(p);
}

MidiControllerProfile vdj_midi_get_profile()
{
    return MidiDispatcher::instance().activeProfile();
}

void vdj_midi_start_learn(MidiAction action, int deck)
{
    MidiDispatcher::instance().startLearn(action, deck);
}

void vdj_midi_cancel_learn()
{
    MidiDispatcher::instance().cancelLearn();
}

bool vdj_midi_is_learning()
{
    return MidiDispatcher::instance().isLearning();
}

void vdj_midi_send_short_msg(uint8_t status, uint8_t data1, uint8_t data2)
{
    if (!g_midi_out)
        return;
    const DWORD packed = static_cast<DWORD>(status) |
                         (static_cast<DWORD>(data1) << 8U) |
                         (static_cast<DWORD>(data2) << 16U);
    midiOutShortMsg(g_midi_out, packed);
}

