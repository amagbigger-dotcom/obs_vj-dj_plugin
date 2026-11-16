# OBS VJ/DJ Plugin - Rendszer Architektúra és AI Útválasztó (INDEX)

Ez a dokumentum a projekt központi tartalomjegyzéke és útmutatója emberi fejlesztők és AI asszisztensek számára.

---

## 1. ALAPADATOK ÉS CÉLKITŰZÉS
* **Projekt neve:** `amagbigger-dotcom/obs_vj-dj_plugin`
* **Bináris cél:** Natív C++17 (Qt 6) OBS Studio 30+/33+ bővítmény (Windows x64).
* **Kimenet:** Kizárólag **1 darab `obs_dj-vj_plugin.dll`**, külső runtime függőségek nélkül közvetlenül az OBS bővítmény mappájába másolva:
  `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll`
* **Hardveres vezérlés:** Native Instruments TRAKTOR KONTROL S2 MK3 (USB MIDI módban).

---

## 2. MODULÁRIS DOKUMENTÁCIÓS TÉRKÉP (AI ROUTER)

Amikor egy adott funkción dolgozol, **NEM kell a teljes kódbázist átolvasnod**. Keresd meg az alábbi táblázatban az érintett területet, és kizárólag a hozzá tartozó specifikációt és forrásfájlokat töltsd be:

| Fejlesztési Terület | Részletes Dokumentáció | Érintett Forrásfájlok | Főbb Felelősségek |
| :--- | :--- | :--- | :--- |
| **Audió Motor & DSP** | [`docs/AUDIO_ENGINE.md`](file:///c:/gitrepo/obs_vj-dj_plugin/docs/AUDIO_ENGINE.md) | `src/sources/wmf_decoder.*`<br>`src/analyzer/analyzer_*`<br>`src/track/track_cache.*`<br>`src/audio/*`<br>`src/engine/deck_engine.*` | WMF dekódolás, RAM PCM puffer, 3-sávos EQ/Filter DSP, élő kick envelope follower, WASAPI füles CUE (OUT 3-4), BPM/Beatgrid detektálás, kvantált loop, `.vdj.json` gyorsítótár. |
| **Videó & Shaders** | [`docs/VIDEO_SHADERS.md`](file:///c:/gitrepo/obs_vj-dj_plugin/docs/VIDEO_SHADERS.md) | `src/video/deck_source.*`<br>`src/video/mixer_source.*`<br>`src/effects/*` | OBS forrás életciklus (`vj_deck`, `vdj_master_mixer`), 1080p GPU méretezés (FIT/FILL), 12 Master crossfader shader, 8 ISF visualizer shader, `.vdj.json` perzisztencia. |
| **Felület & Widgetek** | [`docs/UI_COMPONENTS.md`](file:///c:/gitrepo/obs_vj-dj_plugin/docs/UI_COMPONENTS.md) | `src/gui/deck_window.*`<br>`src/gui/mixer_window.*`<br>`src/gui/tracklist_dock.*`<br>`src/gui/vj_fx_dialog.*`<br>`src/waveform/waveform_widget.*` | Lebegő `QDialog` ablakok (640×340 Deck, 590×340 Mixer, 540×520 VJ ablak), gördülő hullámforma jog scratch-csel, 3-sávos spektrális színezés. |
| **MIDI Hardver** | [`docs/MIDI_HARDWARE.md`](file:///c:/gitrepo/obs_vj-dj_plugin/docs/MIDI_HARDWARE.md) | `src/controllers/midi_handler.*` | Traktor S2 MK3 USB MIDI (WinMM API), CC 8, 16–21 és Note 35–38 mapping, aszinkron szálbiztos atomi vezérlés. |
| **Építés & Munkafolyamat** | [`docs/BUILD_WORKFLOW.md`](file:///c:/gitrepo/obs_vj-dj_plugin/docs/BUILD_WORKFLOW.md) | `CMakeLists.txt`<br>`.github/workflows/build.yml` | CMake konfiguráció, MSVC build, GitHub Actions felhős fordítás, telepítés, mérföldkövek és visszaállítás. |

---

## 3. RENDSZERARCHITEKTÚRA ÉS SZÁLMODELL

A bővítmény szigorúan négy független szálon/környezetben dolgozik a fagyások és a hang/képhibák elkerülése érdekében:

```mermaid
flowchart TD
    subgraph Thread_MIDI [1. WinMM MIDI Callback Szál]
        MidiIn["midi_in_proc (Hardveres megszakítás)"]
        AtomicSetters["Atomi változók írása (std::atomic)\nGain, EQ, Filter, Crossfader, Play/Cue"]
    end

    subgraph Thread_Audio [2. Audio DSP & WASAPI Szál]
        AudioDsp["DeckPlayer::processAudioDsp (48 kHz)"]
        EnvFollower["Élő Envelope Follower (RMS, Kick)"]
        WasapiStream["WasapiOutput::feedAudio (OUT 3-4 Direct CUE)"]
    end

    subgraph Thread_Render [3. OBS Render Szál (Graphics Loop)]
        DeckRender["deck_source_video_render"]
        MixerRender["mixer_video_render"]
        HLSL["Direct3D 11 / OpenGL Shaders (Crossfader & ISF Visualizers)"]
    end

    subgraph Thread_GUI [4. Qt GUI Főszál (Main UI Thread)]
        MixerWin["MixerWindow & DeckWindow"]
        WaveformDraw["WaveformCanvas repainting (50 ms QTimer)"]
        VjDialog["VjFxDialog Paraméterablak"]
    end

    MidiIn --> AtomicSetters
    AtomicSetters -.->|Zéró lock| AudioDsp
    AtomicSetters -.->|Zéró lock| HLSL
    AtomicSetters -.->|Zéró lock| WaveformDraw

    AudioDsp --> EnvFollower
    AudioDsp --> WasapiStream
    EnvFollower -.->|p0..p7 moduláció| HLSL

    DeckRender --> HLSL
    MixerRender --> HLSL
```

### Szálbiztonsági Szabályok:
1. **WinMM Szál (`midi_in_proc`):** Kizárólag atomi (`std::atomic`) settereket hívhat. **SOHA nem hívhat közvetlen Qt GUI metódust** (ez azonnali alkalmazás-összeomlást okozna).
2. **OBS Render Szál:** Nem végezhet fájlműveletet vagy blokkoló hívást. Minden grafikai művelet `obs_enter_graphics()` / `obs_leave_graphics()` védelem alatt fut.
3. **Audio DSP Szál:** Valós idejű szál. Nem allokálhat dinamikus memóriát a feldolgozási ciklusban (`frames`); előre lefoglalt pufferekkel dolgozik.
4. **Qt GUI Főszál:** A felhasználói beavatkozásokat kezeli, és egy 50 ms-os `QTimer` ütemezővel frissíti a pozíciókat, időket és animációkat.
