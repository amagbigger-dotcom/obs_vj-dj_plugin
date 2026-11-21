# AKTUÁLIS FEJLESZTÉSI STÁTUSZ & SPRINT (STATUS)

* **Legutolsó stabil mérföldkő:** Build #315 / GitHub Run #44823480365 (Milestone #32)
* **Aktuális Git Commit:** `21be464`
* **Legutolsó telepített bináris:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 265 664 bájt)
* **Bináris SHA256:** `4EF448253656D34312D0FCB7532ECA8046348E6CB92E47B3058D3756F6CDA556`
* **Kész bináris másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

---

## 1. Jelenlegi Fókusz / Sprint
* **Fő cél:** Korg KAOSS DJ Standby (Stb) feloldása, Settings ablak megnyílásának felgyorsítása (Akai Network MIDI freeze megszüntetése), szálbiztos MIDI OUT kezelés.
* **Architektúra:** Moduláris dokumentációs rendszer fenntartása, BUILD_HISTORY archiválás és szálbiztos MIDI vezérlés.

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #316)
* [x] **Beállítások Ablak Azonnali Megnyílása & Fagyás Megszüntetése:**
  - A `SettingsMidiTab`-ban a `currentIndexChanged` esemény helyett `activated` jelzést használunk, így az eszközlisták feltöltésekor nem indul el téves eszköznyitás.
  - `QSignalBlocker` védi a `populateDevices()`, `populatePresets()` és `applyConfig()` hívásokat.
  - Megszűnt a gép 0-s MIDI eszközének (a LAN-os `Akai Network - MIDI`) automatikus megnyitása és hálózati timeout fagyása.
* [x] **Korg KAOSS DJ Standby / Lecsatlakozás Megelőzése:**
  - A Beállítások megnyitása többé nem bontja le és nem írja felül a KAOSS DJ kapcsolatot (nem vált át Wavetable Synth-re és Akai Network-re).
  - A `vdj_midi_open_input_device` és `vdj_midi_open_output_device` ellenőrzi, hogy már a kért eszköz van-e megnyitva, megelőzve a felesleges lecsatlakozást és újranyitást.
* [x] **Szálbiztos MIDI OUT & Feedback Kezelés:**
  - `std::mutex g_midi_out_mutex` védi a `g_midi_out` mutatót a valós idejű LED visszajelző szál (`feedback_sender_loop`), a clock szál és a GUI szál között.
  - A `MidiDispatcher::sendFeedback` belső állapota (`last_feedback_sent_`, `feedback_blink_counter_`) szálbiztos tagváltozóvá vált `mutex_` védelem alatt.
* [x] **Korg KAOSS DJ Jog Kerék Natív BIN_OFFSET Dekódolás:** A korábbi hibás `SPREAD_64` dekódolás cserélve szabványos `BIN_OFFSET`-re (1..63 jobbra/előre, 127..65 balra/hátra), megszüntetve a fordított működést és a hatalmas ugrásokat.
* [x] **Felezve a Normál Jog Sebesség:** A tányér érintéses pozicionálása 120 ms-ról le lett véve 50 ms/tick-re (a külső gyűrűs nudge 12 ms/tick maradt a finom fázisigazításhoz).
* [x] **SHIFT + Jog Mikroszkopikus Tranzienstűpontosság:** SHIFT nyomvatartásával a tányér és a gyűrű is ultragyengéd 2 ms/tick léptetéssel pozicionál, lehetővé téve a lábdobok mintaszintű megtalálását.
* [x] **Folytonos Beatgrid Csúsztatás az Érintőcsíkkal (Touch Strip CC 33):**
  - SHIFT nyomvatartása mellett az érintőcsíkon húzva az ujjat a beatgrid folyamatosan és simán csúsztatható a lábdob elé (`nudgeGrid`), nem ugrik 1/8-adokat a zenén.
  - Normál módban az érintőcsík folytonos audio keresőként (scrub) funkcionál.
* [x] **SHIFT + Nudge Gombok (`<<` / `>>` Note 21 / 23):** Finom rácstologatás (±5 ms), a középső gombbal (Note 22) + SHIFT pedig `SET 1` rácshorgonyzás.

---

## 3. Következő Lépés (Next Action)
* [ ] Build #316 lefordítása CI-vel, telepítés és éles tesztelés az OBS-ben.
* [ ] Audioless Controller Mode (Deck B Touch Slider + Headphone Cue bekapcsoláskor) használata, ha külső hangkártya (XR18) van beállítva.

---

## 4. Ismert Elakadások / Figyelmeztetések
* **OBS DLL zárolás:** Futó OBS Studio mellett a DLL nem írható felül; frissítéskor az OBS-nek zárva kell lennie.
