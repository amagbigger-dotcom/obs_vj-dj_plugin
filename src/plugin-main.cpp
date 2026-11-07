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
MixerDock *g_dock = nullptr;
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

    g_dock = new MixerDock(g_decks, main_window);
    g_dock->setObjectName(QStringLiteral("obs-vj-mixer-dock"));
    obs_frontend_add_custom_qdock("obs-vj-mixer-dock", g_dock);

    return true;
}

void obs_module_unload(void)
{
    if (g_dock) {
        obs_frontend_remove_dock("obs-vj-mixer-dock");
        delete g_dock;
        g_dock = nullptr;
    }

    vdj_set_decks(nullptr);

    delete g_decks;
    g_decks = nullptr;

    blog(LOG_INFO, "[obs-vj-mixer] Video DJ Mixer unloaded");
}
