#include <obs-module.h>
#include <obs-frontend-api.h>
#include "mixer-dock.hpp"
#include "deck-engine.hpp"
#include "midi-handler.hpp"

#include <QMainWindow>
#include <QPointer>
#include <windows.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-vj-mixer", "en-US")

extern "C" void vdj_register_sources(void);

static QPointer<MixerWindow> s_mixer_window = nullptr;
DeckPair *g_decks = nullptr;

static void open_mixer_window(void *data)
{
    UNUSED_PARAMETER(data);
    QMainWindow *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    
    // Csak akkor hozzuk létre a deckeket és az ablakot, amikor a felhasználó megnyitja a keverőt!
    if (!s_mixer_window) {
        if (!g_decks) {
            g_decks = new DeckPair();
            vdj_set_decks(g_decks);
        }
        s_mixer_window = new MixerWindow(g_decks, main_window);
    }
    
    s_mixer_window->show();
    s_mixer_window->raise();
    s_mixer_window->activateWindow();
}

// DLL belépési pont a Qt6 / Windows keresési útvonalhoz
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNUSED_PARAMETER(lpReserved);
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        char path[MAX_PATH];
        if (GetModuleFileNameA(hModule, path, MAX_PATH)) {
            char *last_slash = strrchr(path, '\\');
            if (last_slash) {
                *last_slash = '\0';
                char *second_slash = strrchr(path, '\\');
                if (second_slash) {
                    *second_slash = '\0';
                    char bin_path[MAX_PATH];
                    snprintf(bin_path, sizeof(bin_path), "%s\\bin\\64bit", path);
                    SetDllDirectoryA(bin_path);
                }
            }
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        SetDllDirectoryA(NULL);
        break;
    }
    return TRUE;
}

extern "C" {
    __declspec(dllexport) bool obs_module_load(void)
    {
        blog(LOG_INFO, "[obs-vj-mixer] Loading Traktor S2 VJ Mixer plugin...");

        // ITT NEM PÉLDÁNYOSÍTUNK DeckPair-t, mert az OBS még nem kész az ffmpeg_source létrehozására!
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

        if (g_decks) {
            delete g_decks;
            g_decks = nullptr;
            vdj_set_decks(nullptr);
        }
        
        blog(LOG_INFO, "[obs-vj-mixer] Plugin unloaded.");
    }
}
