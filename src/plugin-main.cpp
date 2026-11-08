#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QWidget>
#include "deck-engine.hpp"
#include "midi-handler.hpp"
#include "mixer-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs_vj-dj_plugin", "en-US")

static DeckPair *g_main_decks = nullptr;
static MixerWindow *g_mixer_window = nullptr;

extern "C" {
void vdj_register_sources(void);
}

void init_deck_engine()
{
    g_main_decks = new DeckPair();
    vdj_set_decks(g_main_decks);
    vdj_midi_init();
}

void free_deck_engine()
{
    vdj_midi_shutdown();
    vdj_set_decks(nullptr);
    delete g_main_decks;
    g_main_decks = nullptr;
}

static void open_mixer_window(void *data)
{
    UNUSED_PARAMETER(data);
    if (!g_main_decks)
        return;
    
    if (!g_mixer_window) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_mixer_window = new MixerWindow(g_main_decks, main_window);
    }
    g_mixer_window->show();
    g_mixer_window->raise();
    g_mixer_window->activateWindow();
}

void init_mixer_dock()
{
    obs_frontend_add_tools_menu_item(QStringLiteral("Video DJ Mixer").toUtf8().constData(), open_mixer_window, nullptr);
}

void free_mixer_dock()
{
    if (g_mixer_window) {
        g_mixer_window->close();
        delete g_mixer_window;
        g_mixer_window = nullptr;
    }
}

extern "C" {

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs_vj-dj_plugin] Modul betöltése indul...");
    vdj_register_sources();
    init_deck_engine();
    init_mixer_dock();
    blog(LOG_INFO, "[obs_vj-dj_plugin] Sikeresen betöltve!");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs_vj-dj_plugin] Modul leállítása...");
    free_mixer_dock();
    free_deck_engine();
}

} // extern "C"
