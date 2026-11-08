#include <obs-module.h>
#include <obs-frontend-api.h>
#include <qt-display.hpp>
#include <QWidget>

// Ezek a deklarációk kötelezőek az OBS számára, hogy felismerje a plugint
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs_vj-dj_plugin", "en-US")

// Előre deklaráljuk a deck engine / mixer inicializálást, ha külön fájlokban vannak
extern void init_deck_engine();
extern void free_deck_engine();
extern void init_mixer_dock();
extern void free_mixer_dock();
extern void register_mixer_source();

// FONTOS: Az extern "C" megakadályozza a C++ név-manglálást, 
// így az OBS megtalálja a belépési pontot a DLL-ben!
extern "C" {

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs_vj-dj_plugin] Modul betöltése indul...");

    // Források regisztrálása
    register_mixer_source();

    // Deck motor és keverő indítása
    init_deck_engine();
    init_mixer_dock();

    blog(LOG_INFO, "[obs_vj-dj_plugin] Sikeresen betöltve!");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs_vj-dj_plugin] Modul leállítása...");
    free_deck_engine();
    free_mixer_dock();
}

} // extern "C"
