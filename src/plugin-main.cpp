#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QWidget>
#include "deck-engine.hpp"
#include "deck-window.hpp"
#include "tracklist-dock.hpp"
#include "midi-handler.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs_vj-dj_plugin", "en-US")

static DeckPlayer *g_deck_a = nullptr;
static DeckPlayer *g_deck_b = nullptr;
static DeckWindow *g_deck_window = nullptr;

extern "C" {
void vdj_register_deck_source(void);
}

static void open_tracklist_menu(void *data)
{
    UNUSED_PARAMETER(data);
    vdj_open_tracklist_window();
}

static void open_deck_window(void *data)
{
    UNUSED_PARAMETER(data);
    if (!g_deck_a)
        return;

    if (!g_deck_window) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_deck_window = new DeckWindow(g_deck_a, main_window);
    }
    g_deck_window->show();
    g_deck_window->raise();
    g_deck_window->activateWindow();
}

void init_deck_engine()
{
    g_deck_a = new DeckPlayer("vj_deck_source_a", "OBS VJ Deck A");
    g_deck_b = new DeckPlayer("vj_deck_source_b", "OBS VJ Deck B");
    vdj_set_deck_a(g_deck_a);
    vdj_set_deck_b(g_deck_b);
    vdj_set_active_deck(g_deck_a);
    vdj_midi_init();
}

void free_deck_engine()
{
    vdj_midi_shutdown();
    vdj_set_active_deck(nullptr);
    vdj_set_deck_a(nullptr);
    vdj_set_deck_b(nullptr);
    delete g_deck_a;
    delete g_deck_b;
    g_deck_a = nullptr;
    g_deck_b = nullptr;
}

void init_deck_ui()
{
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Deck").toUtf8().constData(), open_deck_window, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Tracklist").toUtf8().constData(), open_tracklist_menu, nullptr);
}

void free_deck_ui()
{
    if (g_deck_window) {
        g_deck_window->close();
        delete g_deck_window;
        g_deck_window = nullptr;
    }
}

extern "C" {

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs_vj_deck] Modul betöltése indul...");
    vdj_register_deck_source();
    init_deck_engine();
    init_deck_ui();
    blog(LOG_INFO, "[obs_vj_deck] Sikeresen betöltve!");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs_vj_deck] Modul leállítása...");
    free_deck_ui();
    free_deck_engine();
}

} // extern "C"
