# AKTUÁLIS FEJLESZTÉSI STÁTUSZ & SPRINT (STATUS)

* **Legutolsó stabil mérföldkő:** Build #320 / GitHub Run #44907080548 (Milestone #35)
* **Aktuális Git Commit:** `21dee34`
* **Legutolsó telepített bináris:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 254 400 bájt)
* **Bináris SHA256:** `D7905AC9DEFAF0367FD5F2DC5E9C5D90977A498A05EB637CFDFCC32157967CA2`
* **Kész bináris másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

---

## 1. Jelenlegi Fókusz / Sprint
* **Fő cél:** Kezelőfelületi ergonómiai megújítás:
  1. Tracklist ablak méretezése pontosan a Deck szélességére (640 px), oszlopok karcsúsítása, kompakt mini Load A/B gombok.
  2. Video FX rendszerek összevonása egyetlen multifunkciós, fix méretű ablakba (540 × 560 px: VJ Shaders, Crossfade FX, LFO fülek).
  3. Felesleges FIT/FILL gomb eltávolítása a Master Mixerről.
* **Architektúra:** Moduláris dokumentációs rendszer fenntartása, BUILD_HISTORY archiválás és szálbiztos vezérlés.

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #320)
* [x] **Kezelőfelületi & Ablak-Ergonómiai Megújítás (Build #320):**
  - **Tracklist Méretének Igazítása a Deckhez:**
    - Az ablak szélessége fix `640 px`-re lett állítva (`setFixedWidth(640)`), így pixelre pontosan illeszkedik a Deck A és Deck B ablakok szélességéhez.
    - Kétsoros szellős felső vezérlősáv (1. sor: VJ mappa, tallózás, IMPORT; 2. sor: Törlés, Konvertálás, Analízis, Új szett, Profil választó).
    - Oszlopok karcsúsítása: ST (30 px), BPM (54 px), KEY (38 px), IDŐ (46 px), FORM (52 px), LOAD (68 px), így a zenecímnek több mint 280 px tiszta hely jut vízszintes görgetés nélkül.
    - **Szuper-kompakt mini betöltő gombok:** A korábbi 175 px-es két batár gomb helyett diszkrét `[ A ]` (kék) és `[ B ]` (narancs) 28×20 px gombok készültek.
  - **Egységes Multifunkciós Video FX Ablak (`VideoFxDialog`, 540 × 560 px):**
    - A 3 korábbi különálló lebegő ablak (`VjFxDialog`, `CrossfadeFxDialog`, `LfoDialog`) összevonva **1 darab fix méretű (540 × 560 px)** multifunkciós ablakba.
    - Felül stílusos fülekkel váltható 3 alrendszer:
      1. `⚡ VJ SHADER`: Deck A/B váltó, 15 shader lista, ISF konverter, alapszín és moduláció, 8 shader paraméter kártya.
      2. `⚙ CROSSFADE FX`: 13 átmenet effekt lista, effektenkénti 8 paraméter és modulációs mátrix.
      3. `〰 LFO MOTOR`: LFO 1 & 2 szekciók és futó valós idejű mini oszcilloszkópok (`LfoScopeWidget`), intelligens CPU-kímélő timerrel.
    - Gomb-fókuszált megnyitás: Deck VJ gomb $\rightarrow$ Shader fül (Deck A vagy B fókusszal); Mixer FX gomb $\rightarrow$ Crossfade fül; Mixer LFO gomb $\rightarrow$ LFO fül.
  - **FIT / FILL Gomb Eltávolítása a Mixerről:**
    - A `scale_btn_` törölve lett a Master Crossfader feletti sávból, megszüntetve a felesleges zsúfoltságot; a háttérben fixen az aránytartó teljes vászonkitöltés (`Fill`) aktív.
* [x] **Korg KAOSS DJ Automatikus WASAPI Hardveres Ébresztés & Keep-Alive:**
  - A Korg KAOSS DJ hardver firmware-e Standby (`Stb`) módban alszik, amíg aktív audio folyamot nem érzékel.
  - A `WasapiOutput` bővítve lett dinamikus tulajdonság-keresővel (`PKEY_Device_FriendlyName` / `DeviceDesc` alapján felismeri a `Vonal (KAOSS DJ)` eszközt).
  - A MIDI handler automatikusan megnyitja és egy csendes háttér-adatfolyammal folyamatosan ébren tartja a Korg audio interfészét, így **azonnal felébred Standby-ból anélkül, hogy a Windowsban alapértelmezettre kellene állítani** (a főkimenet maradhat a Behringer XR18 / X-AIR ASIO)!
* [x] **Beállítások Ablak Azonnali Megnyílása & Fagyás Megszüntetése:**
  - A `SettingsMidiTab`-ban a `currentIndexChanged` esemény helyett `activated` jelzést használunk, így az eszközlisták feltöltésekor nem indul el téves eszköznyitás.
  - `QSignalBlocker` védi a `populateDevices()`, `populatePresets()` és `applyConfig()` hívásokat.
  - Megszűnt a gép 0-s MIDI eszközének (a LAN-os `Akai Network - MIDI`) automatikus megnyitása és hálózati timeout fagyása.
* [x] **Korg KAOSS DJ Standby / Lecsatlakozás Megelőzése a Beállításokban:**
  - A Beállítások megnyitása többé nem bontja le és nem írja felül a KAOSS DJ kapcsolatot (nem vált át Wavetable Synth-re és Akai Network-re).
  - A `vdj_midi_open_input_device` és `vdj_midi_open_output_device` ellenőrzi, hogy már a kért eszköz van-e megnyitva.
* [x] **Szálbiztos MIDI OUT & Feedback Kezelés:**
  - `std::mutex g_midi_out_mutex` védi a `g_midi_out` mutatót a valós idejű LED visszajelző szál (`feedback_sender_loop`), a clock szál és a GUI szál között.
* [x] **Korg KAOSS DJ Jog Kerék Natív BIN_OFFSET Dekódolás:** A fordított működés és túlfutások megszűntek (50 ms normál / 2 ms SHIFT).
* [x] **Folytonos Beatgrid Csúsztatás az Érintőcsíkkal (Touch Strip CC 33):** SHIFT-tel folytonos rácsmozgatás a lábdob elé.

---

## 3. Következő Lépés (Next Action)
* [x] Változtatások commitolása és pusholása a távoli GitHub repóba (Commit `21dee34`).
* [x] Build #320 lefordítása a GitHub Actions CI-n keresztül, DLL letöltése és telepítése.
* [ ] Éles tesztelés az OBS Studio elindításával (Tracklist 640 px elrendezés, mini [ A ]/[ B ] gombok, 3-füles Video FX ablak fókuszálása, FIT/FILL hiánya a keverőn).

---

## 4. Ismert Elakadások / Figyelmeztetések
* **OBS DLL zárolás:** Futó OBS Studio mellett a DLL nem írható felül; frissítéskor az OBS-nek zárva kell lennie.
