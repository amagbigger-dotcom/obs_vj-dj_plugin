# AKTUÁLIS FEJLESZTÉSI STÁTUSZ & SPRINT (STATUS)

* **Legutolsó stabil mérföldkő:** Build #323 (GitHub Run #329)
* **Aktuális Git Commit:** `5c01c23`
* **Legutolsó telepített bináris:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 256 448 bájt)
* **Bináris SHA256:** `1097FD9CE4AE2B0E55917B691F9FACFF5CA112EAFFD9A85ECC47294AE8E2F7EC`
* **Kész bináris másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

---

## 1. Jelenlegi Fókusz / Sprint
* **Fő cél:** Kezelőfelületi Ergonómia & 5-Ablakos Képernyő Elrendezés (Build #324):
  1. Mind az 5 ablak (Deck A, Mixer, Deck B, Video FX, Tracklist) egykattintásos elrendezése az Eszközök menüből (1920×1080).
  2. Ablakméretek és pozíciók pontosítása: OBS menüsor láthatósága (`y = 38`), Deck tálca fölé illesztése, Video FX & Tracklist `500 px`, Deck `410 px` magasság, 0 átfedés.
  3. Kétrétegű Waveform magasság: 175 px (135 px részletes futó hullámforma + 38 px 3-sávos áttekintő).
  4. Deck fejlécének tehermentesítése: zenecímnek 320+ px tiszta hely, /2 és x2 áthelyezve a TAP mellé, triola (3/2) és VJ gomb törölve.
  5. Mixer EQ hangszín potméterek (HI, MID, LOW) értékkijelzésének nullázása (középen 0, vágás `-`, erősítés előjel nélkül).

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #323)
* [x] **Teljes 5-Ablakos Elrendezés & Deck/Mixer Ergonómiai Megújítás (Build #323):**
  - **1920×1080 egykattintásos 5-ablakos layout:** Az OBS Eszközök menüjében a korábbi 3 ablak helyett az új `OBS VJ Teljes Elrendezés (Mind az 5 ablak)` egyszerre nyitja meg és rendezi el pixelre pontosan a Deck A (`640×430`), Mixer (`590×340`), Deck B (`640×430`), Video FX (`540×560`) és Tracklist (`640×560`) ablakokat.
  - **Hullámforma méretoptimalizálás (+90 px):** A teljes extra magasság a hullámformára került (175 px magas). A futó részletes hullámforma 135 px magas (kristálytiszta dinamika és tranziensek), az alsó 3-sávos áttekintő pedig 38 px (kényelmes CUE és ütem drop jelölők).
  - **Szellős Deck fejléc:** A felesleges VJ gomb és a 3/2 triola gomb törölve; a közvetlen BPM beviteli mező, a mértékegység, valamint a `/2` és `x2` gombok lekerültek az alsó rácssávba a `[ TAP ]` gomb mellé. A zenecím mező így több mint 320 px szélességet kap.
  - **Mixer EQ nullázott kijelzés:** A HI, MID és LOW potméterek középállásban `0`-t jeleznek ki; a kivonás/vágás negatív előjellel (`-`), az erősítés előjel nélkül, de `0`-ról indulva jelenik meg (pl. `-0.50` .. `0` .. `0.50`).
  - **Mixer felület tisztítás:** A felesleges `[ ⚡ VJ SHADER FX ]` gomb eltávolítva a keverő alsó sávjából.
  - **Tracklist igazítás:** Fix `640 × 560 px` méret (szélességben a Deckhez, magasságban a Video FX-hez illeszkedve), jobb felső pozicionálással (`x = 1260, y = 10`).
* [x] **Shader Betöltési & OBS Effect-Parser Kompatibilitási Javítás (Build #322):**
  - **Hiba elhárítása:** Az OBS `libobs` shader elemzője (`effect-parser.c`) kifejezéseket tartalmazó `#define` makrók esetén azonnali szintaktikai hibát dob (`Expected ',' or ')'`), emiatt a korábbi `#define TIME (anim_time + bass_energy * 0.12)` sor miatt a `fluid_flow_effect` shader egyáltalán nem fordult le és sem a beépített 15 shader, sem a custom shaderek nem jelentek meg.
  - **Megoldás:** A makrók visszaállítva tiszta aliasokra (`#define TIME anim_time` stb.), a zenei pulzust közvetlenül a C++ oldalon adjuk hozzá az `anim_time` shader paraméterhez (`time_sec + bass_val * 0.12f`) mind a `deck_source.cpp`-ben, mind a `mixer_source.cpp`-ben.
  - **Eredmény:** A shader motor 100%-os hibátlan lefordulása, azonnal látható VJ megjelenítés, a külső/AI és beépített shaderek tökéletes zenei reaktivitása.
* [x] **Vizuális Motor, Crossfade FX és VJ Shader Korszerűsítés (Build #321):**
  - **Crossfade Sötétedés Megszüntetése & Equal Power Átkeverés:**
    - A `mixer_source.cpp` pixel shader kimenetéről törölve a globális `smoothstep(0.015, 1.0, res.rgb)` és `pow(res.rgb, 1.08)` sötétítés, így a kép nem veszít a dinamikájából.
    - Szinuszos-koszinuszos **Equal Power keverési görbe** bevezetése: a felezőponton sem esik be a fényerő.
    - Új **Fényerő Kiemelés (Blend Boost 50% .. 250%)** csúszka a Crossfade FX dialógusban és mixer shaderben.
    - CRT Scanline, Pop-Art Halftone és Neon Sobel Edge effektek fényelnyelésének kompenzálása (energiamegőrző blend).
  - **Deck Volume Fader Alakzat-Áttűnések (Shape Wipes):**
    - A csatorna hangerő faderhez 7 új zenei geometriai átvezető mód készült: Kör szűkülés (Iris Circle), 16:9 téglalap, 1:1 négyzet, rombusz/gyémánt, vízszintes lamellák, függőleges lamellák, középponti zsugorodás (Zoom Out).
    - A fader lehúzásakor a kép az adott alakzatban csukódik be középre, miközben alatta a másik deck teljes pompájában átlátszik.
  - **15 Beépített VJ Shader Zenei Reaktivitásának Normalizálása:**
    - 14 shader végéről törölve a mélyfekete tónusokat levágó `smoothstep(0.015, 1.0, ...)` torzítás.
    - A túlvezérelt `ShapeTwist` (Shader 3) lecsillapítva (`bass_energy * 40.0` helyett stabil `6.0`, zoom és vastagság normalizálás).
    - Az alvó `StringArt` (Shader 4), `Dodecahedron` (Shader 5) és `CubeField` (Shader 11) felébresztve dinamikus zenei pulzusokkal és forgással.
  - **CustomShaderManager Smart Audio Injektor:**
    - A transzpiler a `TIME`, `time`, `iTime` makrókba és a `render_custom` kimenetbe automatikus zenei pulzust (`anim_time + bass_energy * 0.12`) injektál, így a külső ShaderToy és AI által generált shaderek is azonnal lélegezni kezdenek a ritmusra.
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
* [x] Változtatások commitolása és pusholása a távoli GitHub repóba (Build #323, Commit `5c01c23`).
* [x] GitHub Actions CI fordítás sikeres lefutása (Run #329), kész DLL letöltése és telepítése (SHA256: `1097FD9C`).
* [ ] Éles tesztelés az OBS Studio elindításával (5-ablakos layout ellenőrzése, 175 px hullámforma és nullázott EQ működése).

---

## 4. Ismert Elakadások / Figyelmeztetések
* **OBS DLL zárolás:** Futó OBS Studio mellett a DLL nem írható felül; frissítéskor az OBS-nek zárva kell lennie.
