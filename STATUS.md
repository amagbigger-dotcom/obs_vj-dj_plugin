# AKTUÁLIS FEJLESZTÉSI STÁTUSZ & SPRINT (STATUS)

* **Legutolsó stabil mérföldkő:** Build #315 / GitHub Run #44823480365 (Milestone #32)
* **Aktuális Git Commit:** `21be464`
* **Legutolsó telepített bináris:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 265 664 bájt)
* **Bináris SHA256:** `4EF448253656D34312D0FCB7532ECA8046348E6CB92E47B3058D3756F6CDA556`
* **Kész bináris másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

---

## 1. Jelenlegi Fókusz / Sprint
* **Fő cél:** Korg KAOSS DJ Jog kerék és érintőcsík precíziós irányításának, Beatgrid folytonos húzásának élesítése és ellenőrzése.
* **Architektúra:** Moduláris dokumentációs rendszer fenntartása, BUILD_HISTORY archiválás és szálbiztos MIDI vezérlés.

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #315)
* [x] **Korg KAOSS DJ Jog Kerék Natív BIN_OFFSET Dekódolás:** A korábbi hibás `SPREAD_64` dekódolás cserélve szabványos `BIN_OFFSET`-re (1..63 jobbra/előre, 127..65 balra/hátra), megszüntetve a fordított működést és a hatalmas ugrásokat.
* [x] **Felezve a Normál Jog Sebesség:** A tányér érintéses pozicionálása 120 ms-ról le lett véve 50 ms/tick-re (a külső gyűrűs nudge 12 ms/tick maradt a finom fázisigazításhoz).
* [x] **SHIFT + Jog Mikroszkopikus Tranzienstűpontosság:** SHIFT nyomvatartásával a tányér és a gyűrű is ultragyengéd 2 ms/tick léptetéssel pozicionál, lehetővé téve a lábdobok mintaszintű megtalálását.
* [x] **Folytonos Beatgrid Csúsztatás az Érintőcsíkkal (Touch Strip CC 33):**
  - SHIFT nyomvatartása mellett az érintőcsíkon húzva az ujjat a beatgrid folyamatosan és simán csúsztatható a lábdob elé (`nudgeGrid`), nem ugrik 1/8-adokat a zenén.
  - Normál módban az érintőcsík folytonos audio keresőként (scrub) funkcionál.
* [x] **SHIFT + Nudge Gombok (`<<` / `>>` Note 21 / 23):** Finom rácstologatás (±5 ms), a középső gombbal (Note 22) + SHIFT pedig `SET 1` rácshorgonyzás.
* [x] **Automatikus Telepítés és Biztonsági Mentés:** A Build #315 bináris sikeresen átmásolva az OBS bővítmény mappájába és az Asztalra.

---

## 3. Következő Lépés (Next Action)
* [ ] Éles tesztelés OBS-ben a frissen telepített DLL-lel: Jog forgatási irány, sima/shiftes sebességek, Touch Strip grid húzás és SET 1 rögzítés ellenőrzése.

---

## 4. Ismert Elakadások / Figyelmeztetések
* **OBS DLL zárolás:** Futó OBS Studio mellett a DLL nem írható felül; frissítéskor az OBS-nek zárva kell lennie.
