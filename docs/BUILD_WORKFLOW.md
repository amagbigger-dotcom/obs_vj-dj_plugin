# Építés, Telepítés & Munkafolyamat Specifikáció (BUILD_WORKFLOW)

Ez a dokumentum a fordítási környezetet, a GitHub Actions CI folyamatot, a helyi gyorsfordítási tervet és a hivatalos mérföldköveket tartalmazza.

---

## 1. FORRÁSFAJLOK ÉS KONFIGURÁCIÓK
* **`CMakeLists.txt`:** C++17, Qt6 (Core, Gui, Widgets), libobs, obs-frontend-api, Windows Media Foundation és WinMM könyvtárak.
* **`.github/workflows/build.yml`:** Automatizált felhős fordítás Windows 2022 virtuális gépen, OBS 33 SDK-val és Qt6-tal.

---

## 2. KÖVETELMÉNYEK ÉS FÜGGŐSÉGEK

* **Fordító:** Microsoft Visual C++ (MSVC v143 - Visual Studio 2022 x64).
* **Nyelvi szabvány:** C++17 (`set(CMAKE_CXX_STANDARD 17)`).
* **Keretrendszerek:**
  * **OBS Studio SDK:** Fejlécek és könyvtárak (`libobs.lib`, `obs-frontend-api.lib`).
  * **Qt 6.6+:** `Qt6Core.lib`, `Qt6Gui.lib`, `Qt6Widgets.lib`.
  * **Windows API:** `winmm.lib` (MIDI), `mfplat.lib`, `mfreadwrite.lib`, `mfuuid.lib`, `propsys.lib` (WMF audió dekódolás).
* **Generált Bináris:** Szigorúan **1 darab `obs_dj-vj_plugin.dll`**.

---

## 3. TELEPÍTÉSI ÚTVONALAK ÉS JOGOSULTSÁGOK

A kész binárist a Windows OBS Studio 64 bites bővítmény mappájába kell másolni:
* **Elsődleges célhely:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll`
* **Biztonsági másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

*Megjegyzés:* A `Program Files` mappába történő másoláshoz rendszergazdai jogosultság (Elevation / RunAs) szükséges. PowerShell parancs:
```powershell
Start-Process powershell -ArgumentList '-NoProfile -Command "Copy-Item -Path \"$DLL_PATH\" -Destination \"C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll\" -Force"' -Verb RunAs -Wait
```

---

## 4. GITHUB ACTIONS FELHŐS FORDÍTÁS (CI PIPELINE)

Amikor a kódot felküldjük a GitHubra (`git push origin main`), lefut a `.github/workflows/build.yml`:
1. Elindul egy tiszta `windows-2022` futtató környezet.
2. Letölti a Qt6 csomagokat és az előre lefordított OBS Studio 33 SDK-t.
3. CMake-kel és MSVC-vel lefordítja a plugint Release módban.
4. Létrehoz egy `obs-vj-mixer.zip` artifactot.
5. Az artifact a GitHub API-n vagy a nightly.link-en keresztül automatikusan letölthető és telepíthető.

---

## 5. HIVATALOS MÉRFÖLDKÖVEK ÉS TÁMPONTOK (MILESTONES)

Ha bármikor vissza kell állítani egy stabil, bevált állapotot:

### Mérföldkő #1: Build #222 / GitHub Run #231 (Kiemelt Referencia Támpont)
* **Verzió & Commit:** `c680bd5f72af90d939729dd54952c5ed2d60c59f`
* **Git Tag:** `milestone-run231-build222`
* **Kész bináris mentés:** `milestones/obs_dj-vj_plugin_run231_build222.dll`
* **SHA256:** `5CA2942365846B944C89AD3347F84D5AA141DBBF4082D7D12464214BE80C401D`
* **Jellemzők:** 2 deckes stabil független motor, OUT 3-4 WASAPI füles keverő, 4 Hot Cue, 12 Crossfader shader, deck-színek, zöld pipák.

### Mérföldkő #2: Build #233 / GitHub Run #237 (8 ISF Shader & Paraméter Moduláció)
* **Verzió & Commit:** `bdfb49b`
* **SHA256:** `83606345F4A24A12296C2A5E57A6CD9CC436E30E1EEA6A28DF21982979508020`
* **Jellemzők:** 8 ISF visualizer shader tiszta audióhoz, kifordított paraméter csúszkák, paraméterenkénti zenei lüktetés és tónuskövetés moduláció, külön Deck A/B vezérlés, számszintű `.vdj.json` perzisztencia.

### Mérföldkő #3: Build #240 / GitHub Run #240 (Mélyebb Moduláris Architektúra & PCH)
* **Verzió & Commit:** `c5926b6`
* **SHA256:** `373D728EDCE57B6E7BCEA0F0B0C309806A927CB19AC19A158D219E5EF220D802`
* **Jellemzők:** Tiszta, logikus alrendszer-struktúra (`engine/`, `audio/`, `video/`, `effects/`, `controllers/`, `waveform/`, `gui/`), `VjFxDialog` önálló modulba kiemelve, Precompiled Headers (PCH) bekapcsolva a gyors fordításért, modern include hierarchia.

### Mérföldkő #4: Build #241 / GitHub Run #241 (Snap 0.10 BPM Léptetés & Tized Kerekítés)
* **Verzió & Commit:** `0f92aa2`
* **SHA256:** `25728F98A1A2238413E59471FBD9B7A9B82A13F2F8CC4180F971997D30425977`
* **Jellemzők:** `playback_bpm_` nullázódás megszűntetve a sebességállítóban, intelligens snap tized-kerekítés és 0.10 BPM lépés a pitch fader melletti `[ − ]` és `[ + ]` gombokkal, azonnali GUI visszajelzés, auto-repeat tartás, Deck A/B témaszínek és 2 tizedesjegyű hullámforma kijelzés.

### Mérföldkő #5: Build #242 / GitHub Run #242 (1. Fázis: Audió Elemző & Dekódoló Rendszer Modularizáció)
* **Verzió & Commit:** `04e27e4`
* **SHA256:** `E9AD9DD9F7A54D7B7D43776E9ACFB7D6AC50B548BEDD621E9617A9070BA00B2B`
* **Jellemzők:** A korábbi 912 soros `audio_analyzer.cpp` monolit teljes modularizációja:
  - `src/sources/wmf_decoder`: Önálló WMF SourceReader dekóder (sztereó/mono lebegőpontos PCM kinyerés COM szálbiztonsággal).
  - `src/analyzer/analyzer_waveform`: 3-sávos spektrális szűrők (Low, Mid, High), 160 Hz-es Kick lowpass, 500-pontos overview és 100 pont/mp-es részletes hullámforma.
  - `src/analyzer/analyzer_beats`: 500 Hz Onset Novelty görbe, 65–190 BPM autokorreláció, 3-pontos parabolikus csúcsinterpoláció és Downbeat (Kick 1) rácshorgonyzás.
  - `src/track/track_cache`: `.vdj.json` gyorsítótár, kézi szerkesztések és Hot Cue perzisztencia.
  - `src/audio/audio_analyzer`: 139 soros könnyű koordinátor homlokzat.

### Mérföldkő #6: Build #243-244 / GitHub Run #244 (2. Fázis: UI Widget Modularizáció & Deck Kötés Javítás)
* **Verzió & Commit:** `a957c23`
* **SHA256:** `D2BA5C53F286410A11579FCA6E55E99F7DEDB264A18998FDA2E546486A2F59E7`
* **Jellemzők:** A Deck és Mixer ablakok kezelőszerveinek kiemelése önálló `src/widget/` modulokba:
  - `src/widget/dj_slider`: Dupla-kattintásos azonnali reset képességű csúszka (féderek, EQ potméterek, crossfader).
  - `src/widget/loop_bar`: Beat Loop mátrix (1/16 .. 128) aktív narancssárga fénnyel és Jog Nudge lassító/gyorsító gombokkal.
  - `src/widget/transport_bar`: CUE, megvilágított PLAY/PAUSE, SYNC, MASTER és 4 független, színkódolt Hot Cue gomb.
  - `src/widget/tempo_control`: Tempó / Pitch fader, intelligens snap tized-kerekítő `[ − ]` és `[ + ]` 0.10 BPM léptetők, debounced sebességállítás és Deck A (kék) / Deck B (narancs) stílustémázás.
  - `src/gui/deck_window`: 1031 sorról ~650 sorra csökkent; `deck_` mutató azonnal átadva a widgetek konstruktorának és a `bindDeck()` hívásnak, garantálva a Play, Cue, Loop és Pitch gombok hibátlan működését.

### Mérföldkő #7: Build #245 / GitHub Run #245 (3. Fázis: Motor Alrendszer Modularizáció & Master SYNC Védelem)
* **Verzió & Commit:** `044ab46`
* **SHA256:** `83D421BD48FDC2CFA8AB396D490631773764E7D18EA6941853D3B8716633DC05`
* **Jellemzők:** A motor alrendszerek modularizálása és a szinkronizációs logika javítása:
  - `src/engine/engine_sync`: A Master deck tempója védett, SYNC gomb megnyomásakor tilos a másik deck 128-as alapértékére ugrania; a Slave deck veszi át a Master deck tempóját.
  - `src/engine/engine_dsp`: 48 kHz-es 3-sávos EQ és DJ Filter szűrő, élő tranzienstelítő Kick envelope követő.
  - `src/engine/engine_loop`: Ütemhelyes kvantált beat loop és ütemhossz számítások.
  - `src/engine/engine_buffer`: Pozíció- és sebességkonverziós segédosztály.
  - Számbilincselés: Szám betöltésekor a Master deck felismert tempója azonnal frissíti a Master Clockot is a keverőn.

### Mérföldkő #8: Build #246 / GitHub Run #246 (4. Fázis: Mixer Ablak Modularizáció)
* **Verzió & Commit:** `f6feccc`
* **SHA256:** `850DCFBCB2D967A3AA37FB39F1BCAFA02027276D9178582EBAF005BB26824C6D`
* **Jellemzők:** A monolitikus mixer ablak (`src/gui/mixer_window.cpp`) szétbontása önálló, célzott modulokba:
  - `src/gui/mixer_channel`: Önálló `MixerChannelStrip` osztály csatornánkénti Gain, 3-sávos EQ (HI, MID, LOW), Filter, Headphone CUE, VIZ EQ, Volume fader, LED kivezérlésjelző és Volume FX vezérléssel.
  - `src/gui/mixer_master`: Önálló `MixerMasterPanel` osztály Master Clock, TAP tempo, ± léptetők, Master BPM szerkesztő, Headphone monitor hangerő és CUE/Master mix, 13-féle Master Crossfader shader FX, 1080p FIT/FILL képméretező gomb és Crossfader kezeléssel.
  - `src/gui/mixer_window`: 1032 sorról 265 sorra csökkent karcsúsított keretablak; a szigorú 590×340 px méret és a képernyő alsó közepére igazított elrendezés érintetlen maradt.

### Mérföldkő #9: Build #247 / GitHub Run #247 (12 Eredeti Hű VJ Shader & 3D Raymarching Visszaállítás)
* **Verzió & Commit:** `e7b38a3`
* **SHA256:** `E678A6CF982D42AEA198C33F0CBF66574CDDFD55712197231B48AF291364B508`
* **Jellemzők:** A VJ vizualizációs shaderek lebutításának teljes megszüntetése és 12 darab autentikus ISF shader integrálása:
  - **Eredeti 3D Raymarching Visszaállítása:** A `FluidFlow` valódi 3D térbeli kamerát és 14-lépéses volumetrikus rétegzést kapott; a `TruchetSpin` visszakapta a teljes 48-lépéses 3D raymarchert diffúz/spekuláris fényekkel és árnyékokkal; a `HypnoWorm` és `ShapeTwist` visszaállítva a 24-lépéses OpArt és 18-lépéses elektro hullámcsavarásra.
  - **4 Új Hű ISF Shader Integráció:**
    1. `Pupila` (Uará Lab / wtX3Dl): Fotorealisztikus emberi szem és írisz, multi-oktávos poláris zajjal, lábdob (Kick transient) pupilla-kitágulással és tükröződéssel.
    2. `MandalaScope` (mojovideotech): 72-iterációs rekurzív poláris fraktál kaleidoszkóp.
    3. `SearchingLight`: Kiber-fényalagút és kereső fénynyalábok harmonikus térbeli osztással.
    4. `CubeField 3D` (sheltron3030): Végtelen 3D kockamező raymarching `opRep` domén-ismétléssel, `sdBox` távolságmezővel, gradiens felületi normál-árnyékolással és koszinusz palettával.
  - **Paraméter- és UI Kezelés:** `src/effects/vj_config.hpp` és `src/gui/vj_fx_dialog.cpp` mind a 12 shadert dinamikusan kezeli a dedikált `p0..p7` potméterekkel és zenei modulációval.

### Mérföldkő #10: Build #248 / GitHub Run #261 (Intelligens Elemzők, Key Lock & Tracklist Camelot Key Oszlop)
* **Verzió & Commit:** `483129a`
* **SHA256:** `CEF342C7309275516BAD0C99642061191ACCAEA20DB7455E24E957D51C63EEEE`
* **Kész bináris mentés:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` és `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (667 648 bájt)
* **Jellemzők:** A nyílt forráskódú referencia DJ motor legértékesebb elemző moduljainak beépítése a meglévő spektrális/BPM elemző mellé:
  - **`AnalyzerSilence` (Auto-CUE & Csend Levágás):** -60 dBFS pásztázás, az első valódi zenei hang/leütés azonnali CUE ponttá alakítása, megszüntetve a track eleji 0.5–2 mp-es felesleges csendet.
  - **`AnalyzerKey` (Zenei Hangnem & Camelot Kerék Analízis):** 12-fokú tonális kroma vektor és Krumhansl-Schmuckler dúr/moll korreláció; Camelot kód (pl. 8A, 11B) és zenei név (pl. Am, G) kiszámítása a harmonikus keveréshez.
  - **`AnalyzerGain` (ReplayGain & Hangerő Normalizálás):** ITU-R BS.1770 / EBU R128 K-súlyozott integrált hangerő mérés és `replay_gain_db` korrekció tárolása a `.vdj.json`-ban.
  - **Hangmagasság-tartó Key Lock (`DeckPlayer` SOLA Time-Stretching):** `[KEY LOCK]` gomb a tempó csúszka mellett (kék/narancs világítással); bekapcsolva a SOLA fázisillesztett algoritmus a tempó változtatásakor (50%-150%) 100%-ban megőrzi a dal eredeti hangmagasságát.
  - **Médiatár (Tracklist) 7. Oszlop & Valós BPM Megjelenítés:**
    - Új `KEY` oszlop a táblázatban az aranyszínű Camelot kulcs megjelenítésével.
    - A BPM oszlop mostantól a felhasználó által beállított/szerkesztett BPM-et jeleníti meg és menti le a `.vdj.json`-ból, nem a nyers beolvasott értéket.
    - Kétirányú értesítés: a Deckben végzett BPM módosítás azonnal átíródik a Médiatárban is (`vdj_notify_track_bpm_changed`).
