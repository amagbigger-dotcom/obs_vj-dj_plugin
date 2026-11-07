#include "deck-engine.hpp"
#include "mixer-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-vj-mixer", "en-US")

void vdj_register_sources();

namespace {
DeckPair *g_decks = nullptr;
MixerWindow *g_window = nullptr;
}

static void on_tools_menu_clicked(void *)
{
    if (!g_window)
        return;

    g_window->show();
    g_window->raise();
    g_window->activateWindow();
}

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs-vj-mixer] Loading Video DJ Mixer");

    vdj_register_sources();

    g_decks = new DeckPair();
    vdj_set_decks(g_decks);

    auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!main_window) {
        blog(LOG_ERROR, "[obs-vj-mixer] OBS main window is unavailable");
        vdj_set_decks(nullptr);
        delete g_decks;
        g_decks = nullptr;
        return false;
    }

    g_window = new MixerWindow(g_decks, main_window);

    // Külön ablak az Eszközök menüből (nem dokkolódik az OBS felületébe)
    obs_frontend_add_tools_menu_item("Video DJ Mixer", on_tools_menu_clicked, nullptr);

    return true;
}

void obs_module_unload(void)
{
    if (g_window) {
        g_window->close();
        delete g_window;
        g_window = nullptr;
    }

    vdj_set_decks(nullptr);

    delete g_decks;
    g_decks = nullptr;

    blog(LOG_INFO, "[obs-vj-mixer] Video DJ Mixer unloaded");
}
