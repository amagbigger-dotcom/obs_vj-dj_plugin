# OBS VJ/DJ MIXER PLUGIN - RENDSZERDIREKTÍVÁK & SZABÁLYZAT

Ez a dokumentum a projekt statikus architektúrális alapjait, szálbiztonsági szabályait és fejlesztési viselkedési direktíváit rögzíti.
Minden munkamenet elején a dinamikus állapotot a STATUS.md fájlból kell olvasni!

---

## 1. ALAPADATOK ÉS TECH STACK
* **Projekt neve / Repó:** `amagbigger-dotcom/obs_vj-dj_plugin`
* **Kimeneti bináris:** Kizárólag **1 darab `obs_dj-vj_plugin.dll`**, közvetlenül a `C:\Program Files\obs-studio\obs-plugins\64bit\` mappába másolva. Nincsenek külső `.effect` fájlok; az összes HLSL shader C++ karaktersorozatként be van ágyazva a binárisba.
* **Nyelv és Fordító:** C++17, CMake 3.25+, MSVC v143 (Visual Studio 2022 x64).
* **Keretrendszerek:** Qt 6.6+ (Core, Gui, Widgets), libobs és obs-frontend-api (OBS Studio 30+/33+).
* **Média & Rendszer API-k:** Windows Media Foundation (WMF), Windows Multimedia (WinMM MIDI), Windows Core Audio (WASAPI).
* **Támogatott Hardver:** Native Instruments TRAKTOR KONTROL S2 MK3, Korg KAOSS DJ, Pioneer DDJ-400 / CDJ-350, és általános MIDI vezérlők.

---

## 2. MODULÁRIS DOKUMENTÁCIÓS ROUTER (`docs/`)
Részletes alrendszer-specifikációk a célzott feladatokhoz:
* 🗺️ **[docs/INDEX.md](file:///c:/gitrepo/obs_vj-dj_plugin/docs/INDEX.md)**: AI Router, teljes forrásfájl-térkép és szálkezelési modell.
* 🔊 **[docs/AUDIO_ENGINE.md](file:///c:/gitrepo/obs_vj-dj_plugin/docs/AUDIO_ENGINE.md)**: WMF dekódolás, RAM PCM puffer, 3-sávos DSP, élő Kick követő, WASAPI OUT 3-4 Direct CUE, WSOLA Key Lock, Beatgrid.
* 🎬 **[docs/VIDEO_SHADERS.md](file:///c:/gitrepo/obs_vj-dj_plugin/docs/VIDEO_SHADERS.md)**: OBS források (`vj_deck`, `vdj_master_mixer`), 1080p FIT/FILL, 13 Crossfader shader FX, 15 ISF visualizer shader, natív ISF konverter.
* 🎛️ **[docs/UI_COMPONENTS.md](file:///c:/gitrepo/obs_vj-dj_plugin/docs/UI_COMPONENTS.md)**: Lebegő ablakok (Deck 640×340, Mixer 590×340), WaveformWidget, Jog Scratch, 3-sávos spektrum.
* 🎹 **[docs/MIDI_HARDWARE.md](file:///c:/gitrepo/obs_vj-dj_plugin/docs/MIDI_HARDWARE.md)**: Adatvezérelt MIDI motor, Relatív Jog dekódolás, kétirányú LED visszajelzés.
* ⚙️ **[docs/BUILD_WORKFLOW.md](file:///c:/gitrepo/obs_vj-dj_plugin/docs/BUILD_WORKFLOW.md)**: CMake/MSVC build, GitHub Actions CI és telepítési folyamat.
* 📜 **[milestones/BUILD_HISTORY.md](file:///c:/gitrepo/obs_vj-dj_plugin/milestones/BUILD_HISTORY.md)**: Teljes történeti archívum (Build #190 – #312 leírásai).

---

## 3. SZIGORÚ SZÁLKEZELÉSI MODELL & THREAD-SAFETY
A bővítmény négy független szálon/környezetben dolgozik:
1. **WinMM MIDI Callback Szál (`midi_in_proc`):** Aszinkron hardveres megszakítás. Kizárólag szálbiztos `std::atomic` műveleteket végezhet. **SOHA nem hívhat közvetlen Qt GUI metódust** (GUI szál összeomlás elkerülése miatt).
2. **OBS Render Szál (Graphics Loop):** `mixer_video_render` és `monitor_video_render`. Nem végezhet fájlműveletet vagy blokkoló hívást. Minden grafikai művelet `obs_enter_graphics()` / `obs_leave_graphics()` védelem alatt fut.
3. **Audio DSP & WASAPI Szál:** Valós idejű szál (`THREAD_PRIORITY_TIME_CRITICAL`). Nem végezhet dinamikus memóriafoglalást a feldolgozási ciklusban; kizárólag előre lefoglalt pufferekkel dolgozik.
4. **Qt GUI Főszál (Main UI Thread):** 50 ms-os `QTimer` ütemezővel frissíti a felületet, az atomi változókból olvasva (`!isSliderDown()` védelemmel a visszaugrások ellen).

---

## 4. KÓDOLÁSI ÉS FEJLESZTÉSI KONVENCIÓK
* **Moduláris mérethatárok (SOLID):** Törekedj a maximum 200–300 soros, szigorúan egy felelősséget ellátó forrásfájlokra.
* **Beágyazott shaderek:** Minden HLSL kód be van ágyazva a C++ binárisba (nincsenek külső futásidejű fájlok).
* **Perzisztencia:** A számok metaadatai (BPM, rács, CUE pontok, VJ shader paraméterek) a `<zenefajl>.vdj.json` oldalkocsi fájlba mentődnek.
* **Nulla Autoplay:** Média betöltésekor a lejátszónak CUE (00:00) pozícióban, szüneteltetve kell várnia.
* **Ablakméretek rögzítése:** A felületi lebegő ablakok szigorúan rögzített méretűek (Deck: 640×340 px, Mixer: 590×340 px), tilos átméretezhetővé tenni őket.

---

## 5. MUNKAMENET-PROTOKOLL (CHECK-IN / CHECK-OUT)
1. **Kezdéskor (Check-in):** Minden feladat megkezdése előtt olvasd el a `STATUS.md` fájlt a pillanatnyi sprint és célok megismeréséhez.
2. **Célzott fókusz:** Kizárólag a feladathoz közvetlenül kapcsolódó `docs/*.md` és `src/` fájlokat olvasd be.
3. **Befejezéskor (Check-out):** Mielőtt befejezed a választ, frissítsd a `STATUS.md` fájlt az elért eredménnyel, a következő lépéssel és az esetleges ismert hibákkal.
