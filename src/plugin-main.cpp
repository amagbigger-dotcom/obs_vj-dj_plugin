#include "deck-engine.hpp"
#include "mixer-dock.hpp"
#include "midi-handler.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-vj-mixer", "en-US")

extern void vdj_register_sources();

namespace {
MixerWindow *g_window = nullptr;

void open_mixer_window(void *private_data)
{
    (void)private_data;
    if (!g_window) {
        auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
        g_window = new MixerWindow(vdj_decks(), main_win);
    }
    g_window->show();
    g_window->raise();
    g_window->activateWindow();
}
} // namespace

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs-vj-mixer] Loading Video DJ plugin...");

    auto *decks = new DeckPair();
    vdj_set_decks(decks);

    vdj_register_sources();

    // Windows MIDI In csatlakozas inditasa
    vdj_midi_init();

    obs_frontend_add_tools_menu_item("Video DJ Mixer", open_mixer_window, nullptr);

    blog(LOG_INFO, "[obs-vj-mixer] Plugin loaded successfully.");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs-vj-mixer] Unloading Video DJ plugin...");

    // MIDI kapcsolat leallitasa
    vdj_midi_shutdown();

    if (g_window) {
        delete g_window;
        g_window = nullptr;
    }

    DeckPair *decks = vdj_decks();
    if (decks) {
        delete decks;
        vdj_set_decks(nullptr);
    }

    blog(LOG_INFO, "[obs-vj-mixer] Plugin unloaded.");
}
