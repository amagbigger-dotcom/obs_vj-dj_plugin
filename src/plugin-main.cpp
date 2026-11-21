#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QWidget>
#include "engine/deck_engine.hpp"
#include "gui/deck_window.hpp"
#include "gui/mixer_window.hpp"
#include "gui/tracklist_dock.hpp"
#include "gui/vj_fx_dialog.hpp"
#include "gui/lfo_dialog.hpp"
#include "gui/settings_dialog.hpp"
#include "controllers/midi_handler.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs_vj-dj_plugin", "en-US")

static DeckPlayer *g_deck_a = nullptr;
static DeckPlayer *g_deck_b = nullptr;
static DeckPair *g_deck_pair = nullptr;
static DeckWindow *g_deck_window_a = nullptr;
static DeckWindow *g_deck_window_b = nullptr;

extern "C" {
void vdj_register_deck_source(void);
void vdj_register_sources(void);
}

static void open_tracklist_menu(void *data)
{
    UNUSED_PARAMETER(data);
    vdj_open_tracklist_window();
}

static void open_vj_fx_menu(void *data)
{
    UNUSED_PARAMETER(data);
    vdj_open_vj_fx_dialog();
}

static void open_lfo_menu(void *data)
{
    UNUSED_PARAMETER(data);
    vdj_open_lfo_dialog();
}

static void open_settings_menu(void *data)
{
    UNUSED_PARAMETER(data);
    vdj_open_settings_dialog();
}

// =============================================================================
// Deck A Ablak megnyitása (Open Deck A Window - Left 1/3)
// =============================================================================
static void open_deck_window_a(void *data)
{
    UNUSED_PARAMETER(data);
    if (!g_deck_a)
        return;

    if (!g_deck_window_a) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_deck_window_a = new DeckWindow(g_deck_a, main_window);
    }
    g_deck_window_a->show();
    g_deck_window_a->raise();
    g_deck_window_a->activateWindow();
}

// =============================================================================
// Deck B Ablak megnyitása (Open Deck B Window - Right 1/3)
// =============================================================================
static void open_deck_window_b(void *data)
{
    UNUSED_PARAMETER(data);
    if (!g_deck_b)
        return;

    if (!g_deck_window_b) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_deck_window_b = new DeckWindow(g_deck_b, main_window);
    }
    g_deck_window_b->show();
    g_deck_window_b->raise();
    g_deck_window_b->activateWindow();
}

// =============================================================================
// Mixer Ablak megnyitása (Open Mixer Window - Center 1/3)
// =============================================================================
static void open_mixer_window(void *data)
{
    UNUSED_PARAMETER(data);
    vdj_open_mixer_window();
}

// =============================================================================
// Mind az 5 ablak egykattintásos elrendezése (Open 5-Window Complete Layout)
// =============================================================================
static void open_all_layout(void *data)
{
    UNUSED_PARAMETER(data);
    open_deck_window_a(nullptr);
    open_mixer_window(nullptr);
    open_deck_window_b(nullptr);
    vdj_open_tracklist_window();
    vdj_open_vj_fx_dialog();
}

void init_deck_engine()
{
    SettingsDialog::loadSettings();

    g_deck_a = new DeckPlayer("vj_deck_source_a", "OBS VJ Deck A");
    g_deck_a->setSide(DeckPlayer::DeckSide::A);
    g_deck_b = new DeckPlayer("vj_deck_source_b", "OBS VJ Deck B");
    g_deck_b->setSide(DeckPlayer::DeckSide::B);
    vdj_set_deck_a(g_deck_a);
    vdj_set_deck_b(g_deck_b);
    vdj_set_active_deck(g_deck_a);

    g_deck_pair = new DeckPair();
    g_deck_pair->setVideoScaleMode(SettingsDialog::config().default_scale_mode);
    vdj_set_decks(g_deck_pair);

    vdj_midi_init();
}

void free_deck_engine()
{
    if (SettingsDialog::config().auto_save_on_exit) {
        SettingsDialog::saveSettings();
    }

    vdj_midi_shutdown();
    vdj_set_decks(nullptr);
    delete g_deck_pair;
    g_deck_pair = nullptr;

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
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Deck A").toUtf8().constData(), open_deck_window_a, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Mixer").toUtf8().constData(), open_mixer_window, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Deck B").toUtf8().constData(), open_deck_window_b, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Teljes Elrendezés (Mind az 5 ablak)").toUtf8().constData(), open_all_layout, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Tracklist").toUtf8().constData(), open_tracklist_menu, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Visualizer & FX").toUtf8().constData(), open_vj_fx_menu, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ LFO Oszcillátor").toUtf8().constData(), open_lfo_menu, nullptr);
    obs_frontend_add_tools_menu_item(QStringLiteral("OBS VJ Beállítások (Settings)").toUtf8().constData(), open_settings_menu, nullptr);
}

void free_deck_ui()
{
    vdj_free_settings_dialog();
    vdj_free_mixer_window();
    vdj_free_tracklist_window();
    vdj_free_vj_fx_dialog();
    vdj_free_lfo_dialog();
    if (g_deck_window_a) {
        g_deck_window_a->close();
        delete g_deck_window_a;
        g_deck_window_a = nullptr;
    }
    if (g_deck_window_b) {
        g_deck_window_b->close();
        delete g_deck_window_b;
        g_deck_window_b = nullptr;
    }
}

extern "C" {

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs_vj_mixer] Modul betöltése indul...");
    vdj_register_deck_source();
    vdj_register_sources();
    init_deck_engine();
    init_deck_ui();
    blog(LOG_INFO, "[obs_vj_mixer] Sikeresen betöltve!");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs_vj_mixer] Modul leállítása...");
    free_deck_ui();
    free_deck_engine();
}

} // extern "C"
