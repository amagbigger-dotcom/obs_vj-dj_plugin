# PROJEKT RESET & MESTERTERV: OBS VIDEO DJ (VJ) MIXER PLUGIN

Ez a dokumentum a projekt teljes kontextusát, forrásfájljait, hardveres illesztését, rendszerarchitektúráját és aktuális fejlesztési állapotát rögzíti. Bármikor bemásolható új munkamenet indításakor az azonnali folytatáshoz.

---

## 1. ALAPADATOK ÉS CÉLKITŰZÉS
* **Projekt neve / Repó:** `amagbigger-dotcom/obs_vj-dj_plugin`
* **Kimeneti könyvtár / bináris:** `obs-vj-mixer.dll`
* **Cél:** Natív C++17 (Qt 6) OBS Studio bővítmény (Windows x64, OBS 30+/33+), amely 2 deckes professzionális Video DJ keverőként üzemel.
* **Telepítési forma:** Kizárólag **1 darab `obs-vj-mixer.dll`** fájl, amely közvetlenül a `C:\Program Files\obs-studio\obs-plugins\64bit\` mappába másolható. Nincsenek külső `.effect` fájlok vagy segédmappák; az összes HLSL shader C++ karaktersorozatként be van ágyazva a binárisba.
* **Hardveres vezérlő:** **Native Instruments TRAKTOR KONTROL S2 MK3** USB MIDI módban.

---

## 2. A HÁROM REFERENCIA-PILLÉR
1. **Exeldro OBS Architektúra (`obs-media-controls`, `obs-shaderfilter`):**
   - Nem OBS dock panelként épül be, hanem önálló lebegő dialógusként (`QDialog` / `Qt::Window`), ami az OBS felső **Eszközök (Tools) $\rightarrow$ Video DJ Mixer** menüjéből nyitható meg, elkerülve az OBS dokkoló felületének szétesését.
   - Beágyazott HLSL shader C++ karaktersorozatként a binárisban (`gs_effect_create`), így nem igényel külső fájlokat a működéshez.
   - Kétcsatornás textúra-renderelő pipeline (`gs_texrender`) a Deck A és Deck B képének valós idejű Direct3D / OpenGL összefűzéséhez.
   - Privát médiaforrások kezelése (`obs_source_create_private("ffmpeg_source", ...)`).
2. **DJ Sugar / Mixxx Lejátszási Motor:**
   - Kétcsatornás állapotgép: Play/Pause toggle, CUE pont rögzítés és visszaugrás, varratmentes Auto-Loop (1/2 – 64 ütem), pozíciókövetés és időkódlekérés (ms pontosság).
   - Sebesség / Pitch fader kezelés (50% – 150% tartomány).
   - Sync gomb (tempóátvétel a másik deckről oda-vissza).
3. **Traktor Kontrol S2 MK3 Hardver és Vizuális EQ:**
   - 3 sávos EQ és Filter leképzése valós idejű képi paraméterekre:
     * **HI Knob:** PIROS (Red) csatorna szorzója (0.0f – 2.0f, alap: 1.0f).
     * **MID Knob:** ZÖLD (Green) csatorna szorzója (0.0f – 2.0f, alap: 1.0f).
     * **LOW Knob:** KÉK (Blue) csatorna szorzója (0.0f – 2.0f, alap: 1.0f).
     * **FILTER:** SZATURÁCIÓ (Rec.709 Luma alapú: 0.0f = fekete-fehér, 1.0f = natív, 2.0f = túltelített).
   - Csatornánkénti hangerőféderek és központi nagyméretű Master Crossfader (hang és kép egyidejű keverésével).

---

## 3. RENDSZERARCHITEKTÚRA ÉS ADATÁRAMLÁS

### 3.1. Blokkvázlat
```mermaid
flowchart TD
    subgraph Hardware [Hardveres Bemenetek]
        TraktorS2["Native Instruments Traktor S2 MK3\n(USB MIDI)"]
        MediaFiles["Videó / Audió Fájlok\n(mp4, mkv, mov, avi, mp3, wav)"]
    end

    subgraph OBS_Core [OBS Studio Keretrendszer]
        libobs["libobs & obs-frontend-api"]
        ToolsMenu["Eszközök menü:\n'Video DJ Mixer'"]
    end

    subgraph Plugin [obs-vj-mixer.dll]
        MidiDriver["MidiHandler (winmm.lib)\nmidiInOpen / midi_in_proc\nTraktor S2 auto-detect"]
        
        subgraph Engine [Deck Engine]
            DeckA["DeckPlayer A\n(ffmpeg_source private)"]
            DeckB["DeckPlayer B\n(ffmpeg_source private)"]
            DeckPairMgr["DeckPair Menedzser\nCrossfader (0.0 - 1.0)\nTransition Mode (0 - 3)\nAudio Mix Gain"]
        end

        subgraph Rendering [Videó Renderelés & Shaders]
            TexRenderA["gs_texrender Deck A"]
            TexRenderB["gs_texrender Deck B"]
            HLSL["Beágyazott HLSL Effect\n(Linear Fade, Wipe, Additive, Cut\n+ RGB Tint & Saturation)"]
        end

        subgraph GUI [Qt 6 Kezelőfelület]
            MixerDlg["MixerWindow (QDialog)\nSötét Traktor téma"]
            PrevA["VideoPreviewWidget (Deck A)"]
            PrevB["VideoPreviewWidget (Deck B)"]
            PrevMaster["VideoPreviewWidget (Master)"]
            UIControls["Transport, Sliders, Knobs,\nAudio Monitor Selector"]
        end
    end

    subgraph Outputs [OBS Források & Monitor]
        MasterSrc["vdj_master_mixer (1080p Master Source)"]
        MonSrcA["vdj_deck_a_source (Deck A Monitor)"]
        MonSrcB["vdj_deck_b_source (Deck B Monitor)"]
        AudioOut["OBS Audio Monitoring Device"]
    end

    TraktorS2 -->|MIDI CC & Note| MidiDriver
    MidiDriver -->|Atomic setterek| DeckPairMgr
    MediaFiles --> DeckA
    MediaFiles --> DeckB

    ToolsMenu -->|Kattintás| MixerDlg
    DeckA --> TexRenderA
    DeckB --> TexRenderB
    TexRenderA --> HLSL
    TexRenderB --> HLSL
    HLSL --> MasterSrc
    HLSL --> MonSrcA
    HLSL --> MonSrcB

    MasterSrc --> PrevMaster
    MonSrcA --> PrevA
    MonSrcB --> PrevB

    UIControls --> DeckPairMgr
    DeckPairMgr --> AudioOut
```

### 3.2. Szálkezelési modell és Thread-Safety
A bővítmény három független szálon/környezetben dolgozik:
1. **OBS Render Thread (Graphics Loop):**
   - Itt fut a `mixer_video_render` és a `monitor_video_render`.
   - `obs_enter_graphics()` / `obs_leave_graphics()` védelem a textúrák és display-ek készítésekor.
   - Nem végezhet blokkoló műveletet; az értékeket közvetlenül az atomi változókból olvassa.
2. **WinMM MIDI Callback Szál (`midi_in_proc`):**
   - Aszinkron érkező hardveres megszakítás Windows Multimedia szinten.
   - Kizárólag szálbiztos `std::atomic` műveleteket végez a `DeckPlayer` és `DeckPair` paraméterein (`setRed`, `setGreen`, `setBlue`, `setSaturation`, `setVolume`, `setSpeed`, `setCrossfader`).
   - Nem hívhat közvetlen Qt GUI metódusokat (GUI szál crash elkerülése miatt).
3. **Qt GUI Eseményhurok (Main Thread):**
   - Felhasználói egér- és gombesemények (`MixerWindow`).
   - 80 ms-os `QTimer` gondoskodik a pozíciók, időkódok, gombfeliratok és MIDI kapcsolat állapotának lekérdezéséről és frissítéséről (`refreshDeck`, `refreshMixer`).

---

## 4. TRAKTOR KONTROL S2 MK3 MIDI KIOSZTÁS

| Kezelőszerv | Esemény típusa | MIDI Csatorna & Paraméter | Plugin Funkció | Skálázás / Matematika |
| :--- | :--- | :--- | :--- | :--- |
| **Deck A HI** | Control Change | Ch 1, CC 16 (0–127) | Deck A Vörös (Red) EQ | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck A MID** | Control Change | Ch 1, CC 17 (0–127) | Deck A Zöld (Green) EQ | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck A LOW** | Control Change | Ch 1, CC 18 (0–127) | Deck A Kék (Blue) EQ | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck A FILTER** | Control Change | Ch 1, CC 19 (0–127) | Deck A Szaturáció | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck A VOL FADER**| Control Change | Ch 1, CC 20 (0–127) | Deck A Hangerő | `val / 127.0f` $\rightarrow$ $0.0 - 1.0$ |
| **Deck A TEMPO** | Control Change | Ch 1, CC 21 (0–127) | Deck A Sebesség (Pitch) | $50 + (val \times 100) / 127$ $\rightarrow$ $50\% - 150\%$ |
| **Deck A PLAY** | Note On ($vel > 0$) | Ch 1, Note 35 | Deck A Play / Pause toggle | Állapotfüggő váltás |
| **Deck A CUE** | Note On ($vel > 0$) | Ch 1, Note 36 | Deck A CUE mentés / ugrás | Lejátszáskor: ugrás + pause; Stopkor: CUE mentés |
| **Deck A SYNC** | Note On ($vel > 0$) | Ch 1, Note 37 | Deck A Sync Deck B-hez | Deck A sebességének beállítása Deck B sebességére |
| **Deck B HI** | Control Change | Ch 2, CC 16 (0–127) | Deck B Vörös (Red) EQ | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck B MID** | Control Change | Ch 2, CC 17 (0–127) | Deck B Zöld (Green) EQ | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck B LOW** | Control Change | Ch 2, CC 18 (0–127) | Deck B Kék (Blue) EQ | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck B FILTER** | Control Change | Ch 2, CC 19 (0–127) | Deck B Szaturáció | `val / 63.5f` $\rightarrow$ $0.0 - 2.0$ (közép: 1.0) |
| **Deck B VOL FADER**| Control Change | Ch 2, CC 20 (0–127) | Deck B Hangerő | `val / 127.0f` $\rightarrow$ $0.0 - 1.0$ |
| **Deck B TEMPO** | Control Change | Ch 2, CC 21 (0–127) | Deck B Sebesség (Pitch) | $50 + (val \times 100) / 127$ $\rightarrow$ $50\% - 150\%$ |
| **Deck B PLAY** | Note On ($vel > 0$) | Ch 2, Note 35 | Deck B Play / Pause toggle | Állapotfüggő váltás |
| **Deck B CUE** | Note On ($vel > 0$) | Ch 2, Note 36 | Deck B CUE mentés / ugrás | Lejátszáskor: ugrás + pause; Stopkor: CUE mentés |
| **Deck B SYNC** | Note On ($vel > 0$) | Ch 2, Note 37 | Deck B Sync Deck A-hoz | Deck B sebességének beállítása Deck A sebességére |
| **CROSSFADER** | Control Change | Global / Ch 1, CC 8 (0–127) | Master Crossfader | `val / 127.0` $\rightarrow$ $0.0$ (A) és $1.0$ (B) között |

*Megjegyzés:* A Traktor S2-n a Note On 0 velocity értékét a Windows MIDI Note Off-ként kezeli; a gombesemények csak valós lenyomáskor ($vel > 0$) váltanak ki akciót.

---

## 5. FORRÁSFAJLOK ÉS AKTUÁLIS ÁLLAPOTUK

* **`CMakeLists.txt`:** *(Kész)*
  - CMake 3.25+, C++17, Qt6 (Core, Gui, Widgets), libobs, obs-frontend-api.
  - A `winmm` könyvtár hozzáadva a `target_link_libraries`-hez a Windows Multimedia API hívásokhoz.
  - Generált célpont: `obs-vj-mixer.dll`.
* **`.github/workflows/build.yml`:** *(Kész)*
  - Automatikus CI folyamat Windows Server 2022 runneren.
  - Letölti és lefordítja az OBS Studio 33.0.1 SDK-t (`libobs`, `obs-frontend-api`).
  - Qt 6.6.1-gyel és MSVC 2022-vel lefordítja a plugint.
  - Kimeneti artifact: 1 db önálló `obs-vj-mixer.dll`.
* **`src/plugin-main.cpp`:** *(Kész)*
  - Modul belépési pont (`obs_module_load`, `obs_module_unload`).
  - Regisztrálja az OBS forrásokat (`vdj_register_sources`).
  - Inicializálja a globális motort (`init_deck_engine`), beindítja a MIDI figyelőt (`vdj_midi_init`), és hozzáadja az OBS Eszközök menüjébe a "Video DJ Mixer" ablaknyitó elemet.
* **`src/waveform-widget.hpp` & `src/waveform-widget.cpp`:** *(Kész)*
  - `WaveformWidget`: Valós idejű zenei hullámforma és BPM kijelző widget.
  - Dinamikus digitális BPM kijelzés, `TAP` tempó gomb és finomhangoló (`+`/`-`) gombok.
  - Függőleges beatgrid ütemvonalak negyedenként és ütemenként (vastagabb sávok számozással).
  - Folyamatosan gördülő hullámforma az aktuális lejátszási fej körül, loop szakasz kiemeléssel és interaktív pozíció-kereséssel (scrubbing).
* **`src/deck-engine.hpp` & `src/deck-engine.cpp`:** *(Kész)*
  - `DeckPlayer`: Privát `ffmpeg_source` kezelő. Támogatja a Play/Pause, CUE, seek, sebesség (50-150%), hangerő, atomi RGB és telítettségi értékek, BPM és kvantált ütemhelyes beat loop (1/2, 1, 2, 4, 8, 16, 32, 64 ütem) futtatását.
  - Tempóváltás pozíció-megőrzéssel (`pending_speed_seek_ms_`), amely megakadályozza, hogy a videó a legelejére ugorjon sebességállításkor.
  - Fejhallgató CUE (PFL) állapotkezelés és Pre-Fade hangerővezérlés.
  - `DeckPair`: Deck A és B párosítása, mester forrás (`vdj_master_mixer`) összerendelése, crossfader és átmeneti módok kezelése, tempószinkron (`syncTempoAtoB`, `syncTempoBtoA`), OBS aktív kimenet választó (`obs_output_mode_`), fejhallgató hangerő és Cue/Master mix.
* **`src/mixer-source.cpp`:** *(Kész)*
  - Kettős `gs_texrender` renderelési lánc.
  - Beágyazott `MIXER_EFFECT` HLSL shader:
    - RGB szorzók (`gains_a`, `gains_b`) és Rec.709 luma alapú színtelítettség (`saturation_a`, `saturation_b`).
    - 4 átmeneti mód: 0 = Linear Fade, 1 = Horizontal Wipe, 2 = Additive Blend, 3 = Hard Cut.
  - OBS kimeneti mód választó támogatása: Master Crossfade (0), Direkt Deck A (1), Direkt Deck B (2).
  - OBS források regisztrációja: `vdj_master_mixer` (mester kimenet), `vdj_deck_a_source` és `vdj_deck_b_source` (egyedi monitorok) `enum_active_sources` és `enum_all_sources` támogatással.
* **`src/shaders/crossfade.effect`:** *(Kész)*
  - Referencia HLSL effect fájl külső teszteléshez és későbbi kibővítésekhez.
* **`src/midi-handler.hpp` & `src/midi-handler.cpp`:** *(Kész)*
  - Windows Multimedia API alapú MIDI illesztő (`midiInOpen`, `midiInStart`, `midiInStop`, `midiInClose`).
  - Automatikus kontroller-keresés: név alapján detektálja a "traktor" és "s2" sztringeket a csatlakoztatott MIDI bemenetek között.
  - Feldolgozza a CC 8, 16–21 és Note 35–38 üzeneteket (Play, Cue, Sync, Headphone Cue), és közvetlenül a `DeckPair` / `DeckPlayer` atomi változóiba írja.
* **`src/mixer-dock.hpp` & `src/mixer-dock.cpp`:** *(Kész)*
  - Traktor Pro stílusú professzionális sötét kezelőfelület:
    - A kis videó előnézetek helyett interaktív zenei `WaveformWidget` hullámforma és BPM sávok.
    - **OBS ACTIVE OUTPUT** gombok: `[ MASTER MIX ]` | `[ DECK A ]` | `[ DECK B ]`, amelyek közvetlenül a fő OBS vászonra küldik a választott forrást.
    - Fejhallgató CUE (PFL) gombok mindkét deckhez (`[ 🎧 CUE A ]`, `[ 🎧 CUE B ]`).
    - Fejhallgató Cue/Master Mix és Hangerő szabályozók.
    - Kétcsatornás EQ és Volume fader szekció, nagyméretű Master Crossfader.
    - Hangkártya választó és automatikus inicializálás (`obs_enum_audio_monitoring_devices`).
  - 50 ms-os frissítési ciklus (`QTimer`).

---

## 6. FEJLESZTÉSI EREDMÉNYEK ÉS HÁTRALÉVŐ LÉPÉSEK (BACKLOG)

### 1. Kétirányú GUI $\leftrightarrow$ Deck szinkronizáció *(KÉSZ)*
- A kezelőfelületen lévő `HI`, `MID`, `LOW`, `FILTER` csúszkák `valueChanged` eseményei rákötve a `setRed()`, `setGreen()`, `setBlue()`, `setSaturation()` hívásokra ($1.0 + val / 100.0f$).
- A `refreshDeck` ciklus `QSignalBlocker`-rel és `!isSliderDown()` védelemmel lekérdezi és simán animálja az EQ és Volume csúszkákat a Traktor S2 hardveres potmétereinek forgatásakor.
- A Master Crossfader és a Pitch slider szintén védve van a visszaugrásoktól (`!isSliderDown()`).

### 2. Valós BPM kezelés, Tap Tempo és Ütemvonalak *(KÉSZ)*
* **Megvalósítás:** A `WaveformWidget` valós idejű digitális BPM kijelzést, TAP Tempo számlálót, `+`/`-` finomhangolást és gördülő beatgrid ütemvonalakat kapott. Az Auto-Loop a valós BPM és sebesség alapján kvantált ütemhelyes loopolást végez.

### 3. Tempóállítás pozíció-megőrzéssel *(KÉSZ)*
* **Megvalósítás:** A sebesség csúszka mozgatásakor az aktuális lejátszási pozíció rögzítésre kerül, így a dekóder újraindulásakor nem ugrik a videó a legelejére.

### 4. Fejhallgató Előhallgatás (Headphone CUE / PFL) *(KÉSZ)*
* **Megvalósítás:** Külön CUE A és CUE B előhallgatási gombok, független Pre-Fade jelszint kezelés és fejhallgató hangerő / Cue-Master mix potméter.

### 5. OBS Aktív Kimenet Választó *(KÉSZ)*
* **Megvalósítás:** A felületről közvetlenül vezérelhető, hogy az OBS fő kimenetén a Master Mix, csak a Deck A vagy csak a Deck B jelenjen meg.

### 6. MIDI Out / Hardveres LED visszajelzés (Prioritás: KÉNYELMI)
* **Jelenlegi állapot:** A plugin jelenleg kizárólag MIDI IN-t olvas.
* **Fejlesztési cél:** `midiOutOpen` és `midiOutShortMsg` megvalósítása a gombok és LED-ek hardveres visszajelzéséhez.

### 4. Átfogó Dokumentáció (`README.md`) *(KÉSZ)*
* Elkészült a teljes körű `README.md` a rendszerarchitektúra, hardveres bekötés, Mermaid folyamatábra, valamint a build és telepítési lépések bemutatásával.

---

## 7. ÉPÍTÉSI ÉS TELEPÍTÉSI ÚTMUTATÓ

### 7.1. Helyi fordítás (Windows 10/11 x64)
Előfeltételek:
* Visual Studio 2022 (MSVC v143 C++ eszközök)
* CMake 3.25 vagy újabb
* Qt 6.6+ (MSVC 2022 x64 build)
* Előre lefordított OBS Studio 33+ SDK (fejlécek és `.lib` könyvtárak)

Parancsok:
```powershell
# Környezeti változó beállítása az OBS SDK helyére:
$env:OBS_SDK_DIR = "C:\path\to\obs-studio-sdk"

# CMake konfigurálás
cmake -S . -B build -A x64 "-DOBS_SDK_DIR=$env:OBS_SDK_DIR"

# Fordítás Release módban
cmake --build build --config Release --parallel
```

### 7.2. Telepítés
1. A sikeres fordítás után a `build\Release\obs-vj-mixer.dll` fájl jön létre.
2. Zárja be az OBS Studio-t.
3. Másolja be a fájlt az OBS bővítmény mappájába:
   `C:\Program Files\obs-studio\obs-plugins\64bit\obs-vj-mixer.dll`
4. Indítsa el az OBS-t, és nyissa meg a keverőt az **Eszközök (Tools) $\rightarrow$ Video DJ Mixer** menüpontból.
