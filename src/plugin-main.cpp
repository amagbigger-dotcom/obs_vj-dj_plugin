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

// Ez a konstruktor/függvény automatikusan lefut a DLL betöltésekor, és hozzáadja a bin\64bit mappát a kereséshez
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        // Hozzáadjuk az OBS bin\64bit mappáját a DLL keresési útvonalhoz, hogy a Qt6 DLL-ek megleljenek
        char path[MAX_PATH];
        if (GetModuleFileNameA(hModule, path, MAX_PATH)) {
            char *last_slash = strrchr(path, '\\');
            if (last_slash) {
                *last_slash = '\0'; // obs-plugins\64bit
                char *second_slash = strrchr(path, '\\');
                if (second_slash) {
                    *second_slash = '\0'; // obs-plugins
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
