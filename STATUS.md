# AKTUÁLIS FEJLESZTÉSI STÁTUSZ & SPRINT (STATUS)

* **Legutolsó stabil mérföldkő:** Build #319 / GitHub Run #44894319532 (Milestone #34)
* **Aktuális Git Commit:** `03031c2`
* **Legutolsó telepített bináris:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 272 320 bájt)
* **Bináris SHA256:** `3D539136406144773F887AC1606D3B5116C2145FEE12AB12BE5EC6A9B2FA1DEE`
* **Kész bináris másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

---

## 1. Jelenlegi Fókusz / Sprint
* **Fő cél:** Korg KAOSS DJ Standby (Stb) automatikus hardveres felébresztése beépített WASAPI audio keep-alive adatfolyammal, függetlenül a Windows alapértelmezett hangkártyájától.
* **Architektúra:** Moduláris dokumentációs rendszer fenntartása, BUILD_HISTORY archiválás és szálbiztos MIDI/Audio vezérlés.

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #318)
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
* [ ] Build #316 lefordítása CI-vel, telepítés és éles tesztelés az OBS-ben.
* [ ] Audioless Controller Mode (Deck B Touch Slider + Headphone Cue bekapcsoláskor) használata, ha külső hangkártya (XR18) van beállítva.

---

## 4. Ismert Elakadások / Figyelmeztetések
* **OBS DLL zárolás:** Futó OBS Studio mellett a DLL nem írható felül; frissítéskor az OBS-nek zárva kell lennie.
