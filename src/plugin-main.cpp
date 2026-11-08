#include <obs-module.h>
#include <obs-frontend-api.h>
#include "mixer-dock.hpp"
#include "deck-engine.hpp"
#include "midi-handler.hpp"

#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-vj-mixer", "en-US")

static QPointer<MixerWindow> s_mixer_window = nullptr;
DeckPair *g_decks = nullptr;

static void open_mixer_window(void *data)
{
    UNUSED_PARAMETER(data);
    QMainWindow *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    
    if (!s_mixer_window) {
        if (!g_decks) {
            g_decks = new DeckPair();
        }
        s_mixer_window = new MixerWindow(g_decks, main_window);
    }
    
    s_mixer_window->show();
    s_mixer_window->raise();
    s_mixer_window->activateWindow();
}

extern "C" {
    __declspec(dllexport) bool obs_module_load(void)
    {
        blog(LOG_INFO, "[obs-vj-mixer] Loading Traktor S2 VJ Mixer plugin...");
        
        if (!g_decks) {
            g_decks = new DeckPair();
        }

        vdj_register_sources();
        vdj_midi_init();

        obs_frontend_add_tools_menu_item("Traktor S2 VJ Mixer", open_mixer_window, nullptr);
        
        blog(LOG_INFO, "[obs-vj-mixer] Plugin loaded successfully.");
        return true;
    }

    __declspec(dllexport) void obs_module_unload(void)
    {
        blog(LOG_INFO, "[obs-vj-mixer] Unloading plugin...");
        vdj_midi_shutdown();

        if (s_mixer_window) {
            s_mixer_window->close();
            delete s_mixer_window;
            s_mixer_window = nullptr;
        }

        delete g_decks;
        g_decks = nullptr;
        
        blog(LOG_INFO, "[obs-vj-mixer] Plugin unloaded.");
    }
}
