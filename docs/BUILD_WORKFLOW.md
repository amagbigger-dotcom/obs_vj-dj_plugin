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
* **Kész bináris mentés:** `milestones/obs_dj-vj_plugin_run261_build248.dll` (667 648 bájt)
* **Jellemzők:** A nyílt forráskódú referencia DJ motor legértékesebb elemző moduljainak beépítése a meglévő spektrális/BPM elemző mellé:
  - **`AnalyzerSilence` (Auto-CUE & Csend Levágás):** -60 dBFS pásztázás, az első valódi zenei hang/leütés azonnali CUE ponttá alakítása, megszüntetve a track eleji 0.5–2 mp-es felesleges csendet.
  - **`AnalyzerKey` (Zenei Hangnem & Camelot Kerék Analízis):** 12-fokú tonális kroma vektor és Krumhansl-Schmuckler dúr/moll korreláció; Camelot kód (pl. 8A, 11B) és zenei név (pl. Am, G) kiszámítása a harmonikus keveréshez.
  - **`AnalyzerGain` (ReplayGain & Hangerő Normalizálás):** ITU-R BS.1770 / EBU R128 K-súlyozott integrált hangerő mérés és `replay_gain_db` korrekció tárolása a `.vdj.json`-ban.
  - **Hangmagasság-tartó Key Lock (`DeckPlayer` Time-Stretching):** `[KEY LOCK]` gomb a tempó csúszka mellett (kék/narancs világítással).
  - **Médiatár (Tracklist) 7. Oszlop & Valós BPM Megjelenítés:**
    - Új `KEY` oszlop a táblázatban az aranyszínű Camelot kulcs megjelenítésével.
    - A BPM oszlop a felhasználó által beállított/szerkesztett BPM-et jeleníti meg és menti le a `.vdj.json`-ból.

### Mérföldkő #11: Build #249 / GitHub Run #263 (Zenei Újraanalizálás & Kristálytiszta WSOLA Key Lock)
* **Verzió & Commit:** `e4cc9ef`
* **SHA256:** `69BBBA7B8CA8859136D5E69B8DA41CA51A8CEA3D17F662062975588262E865FC`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (678 400 bájt)
* **Jellemzők:** A felhasználói visszajelzések alapján végzett mélyreható akusztikai és kezelőfelületi javítások:
  - **Kristálytiszta WSOLA (Waveform Similarity Overlap-Add) Hangmagasság-tartás:**
    - Megszüntetve a szemcseugrás miatti fáziskimaradások és a 200 Hz-es akusztikai recsegés.
    - $W = 1024$ minta (~21.3 ms), $O = 512$ minta (~10.7 ms, 50% átfedés), Hann raised-cosine simítás.
    - Keresztkorrelációs fázisillesztés ($\pm 256$ minta) a természetes akusztikai folytatás alapján, bit-pontos pass-through 100% sebességnél.
    - Nullázott seek és loop állapotok, feszített valós idejű szálbiztonság.
  - **Külön Zenei Analizáló Gomb & Háttérfolyamat (`⚡ ANALIZÁLÁS`):**
    - Nem kell órákig All-Intra videót konvertálni: egyetlen gombnyomásra csak az audiót dekódolja és futtatja le a spektrális, BPM, Camelot Key, Auto-CUE és Gain számításokat.
    - Kijelölt dalokra vagy a teljes médiatárra futtatható folyamatjelzővel és `[⏹ MÉGSE]` megszakítási lehetőséggel.
    - Jobb egérgombos menüben: `⚡ Zenei újraanalizálás (BPM & Camelot Key, videó nélkül)`.
    - Automatikus háttér-analízis indul az OBS megnyitásakor minden korábbi számra, amely még nem rendelkezik Camelot kulccsal.
  - **Valós BPM Detektálás Finomhangolása:**
    - 4 perces pásztázási ablak a hosszú bevezetővel rendelkező dalokhoz.
    - Szélesített, finom DJ prior ($\sigma = 65$, 20% tie-breaker), amely megszünteti a 90–175 BPM közötti dalok mesterséges 128 BPM-re erőltetését.

### Mérföldkő #12: Build #250 / GitHub Run #267 (FFT Kroma Kulcsdetektálás, Kattogásmentes Loop Átfedés & BPM /2, x2, 3/2 Vezérlők)
* **Verzió & Commit:** `ff28a2f`
* **SHA256:** `ADA378D3F21C6601F444D2BC48B8993C59ABCE4C4DEB316B5828CC6DBF73BF58`
* **Kész bináris mentés:** `milestones/obs_dj-vj_plugin_run267_build250.dll` (685 056 bájt)

### Mérföldkő #13: Build #251 / GitHub Run #268 (Key Lock 44.1k->48k Resampling, Shader Potméter Memória, Egyedi Színmoduláció & Makro-Tónus)
* **Verzió & Commit:** `cb3cb8a`
* **SHA256:** `AB2FB42FCDE6D634B302534CD852DB8091158E655D8387A4A5ABB879C9F86DDC`
* **GitHub Run ID:** `44157498011`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (686 080 bájt)
* **Jellemzők:** A felhasználó által jelzett 4 kulcsfontosságú hiba és funkcionális hiányosság teljes körű megoldása:
  1. **Key Lock 100% Tempó Elhangolódás Megszüntetése (44.1 kHz $\rightarrow$ 48.0 kHz Resampling):**
     - A hangfájlokból származó 44.1 kHz-es WSOLA szemcsék ezentúl nem közvetlenül kerülnek az OBS 48.0 kHz-es kimenetére (+1.47 félhangos mesterséges transzponálást okozva), hanem valós idejű lineáris interpolációval reszamplálódnak `sr_step = audio_sample_rate_ / out_sr` sebességgel.
     - 1.0x eredeti sebességnél (`|speed_ratio - 1.0| < 0.0005`) 100% bit-pontos bypass lép életbe: nulla fázistorzítás, nulla CPU terhelés, kristálytiszta eredeti hangzás.
     - A valós idejű audioszál szinkron `obs_source_media_set_time` hívásai kiváltva egy aszinkron atomi ugrásjelzőre (`pending_loop_video_seek_ms_`), megszüntetve a loop visszaugráskor jelentkező puffer-kimaradást és kattanást.
  2. **Shaderek Potmétereinek Számonkénti és Shaderenkénti Megőrzése (Per-Shader Persistence):**
     - A `VjDeckConfig` mostantól mind a 12 shader egyedi paramétereit (`all_shader_params[12][8]`) külön-külön tárolja el a memóriában és a `.vdj.json` gyorsítótárban.
     - Shaderváltáskor az előző shader módosított értékei azonnal elmentődnek, az új shader kiválasztásakor pedig a hozzá tartozó korábbi egyedi potméter-állások töltődnek vissza ahelyett, hogy defaultra állnának.
  3. **Szabad Vizuális Színbeállítás & Zenei Színmoduláció:**
     - A színek nincsenek többé kényszerűen a Deck A (kék) és Deck B (narancs) profilhoz láncolva.
     - A VJ ablakban új színválasztó (`QColorDialog`) és 4-féle modulációs algoritmus kapott helyet:
       * `0: 🔒 Fix Szín`: Kiválasztott egyedi szín megjelenítése.
       * `1: 🥁 Lábdob Pulzus`: A lábdob energiájára lüktető fényerő és szaturáció.
       * `2: 📈 Zenei Tónus`: Dinamikus színátmenet a nyugodt, hűvös kiállásokból a tüzes, intenzív drop felé haladva.
       * `3: 🌈 Színkör Ciklus`: A BPM tempóra és az időkódra folyamatosan forgó szivárvány spektrum.
     - Mind a 12 ISF shader HLSL renderelője megkapta a `custom_theme_color` uniformot.
  4. **Lassú, Zenei Felépülést Követő Makro-Tónus Algoritmus:**
     - Az azonnali dobütéseken ugráló, zajos tónus helyett egy 8-bucketos (~5.5 másodperc, 2-3 zenei ütem) Gauss-szűrő követi a zene valódi strukturális fokozódását és energiáját (`overview_tonus`).
     - A pozíció áttekintő ablak alsó sávján egy finom makro-tónus feszültség-szalag jelenik meg, az áttekintő hullámforma színezése pedig a track felépítését tükrözi (hideg bevezető $\rightarrow$ arany feszültség $\rightarrow$ meleg, tüzes drop).
  5. **BPM /2 és ×2 Gombok a DISP Fejlécben:**
     - A hullámforma felső információs fejlécében közvetlen gyorsgombok a fél- és duplatempós BPM azonnali, egykattintásos beállításához és szinkronizálásához.
* **Telepítve:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`
* **Jellemzők & Megoldott Problémák:**
  - **FFT Kroma 24-Hangnem Detektálás (`AnalyzerKey`):**
    - A Goertzel szűrő mélyfrekvenciás $1/\omega^2$ torzítása felszámolva: 2048 pontos valós FFT Hann ablakkal és spektrális fehérítéssel.
    - Kizárólag a 130 Hz (C3) és 2100 Hz közötti tiszta harmonikus tartományt elemzi, teljesen kiküszöbölve a 40–90 Hz-es lábdob energiát.
    - Képkockánkénti L2 normalizálás és logaritmikus kompresszió: mind a 24 zenei hangnem (1A–12B Camelot) pontosan és megbízhatóan felismerésre kerül.
  - **Kattogásmentes Varratmentes Loop Visszaugrás:**
    - Key Lock / WSOLA módban a puffer és szemcse megőrzésre kerül a loop végéről; az 512 mintás Hann átfedés zökkenőmentesen és kattanás nélkül köti össze a loop végét az elejével.
    - Varispeed módban 10 ms-os simított Hann raised-cosine crossfade garantálja a pattogásmentes loopolást.
  - **Key Lock Pop & Holttér Megszüntetése:**
    - Eltávolítva a `(abs(speed - 1.0) > 0.002)` durva ugrási küszöb; a hangmagasság-tartás finom tempómozgatásnál is észrevétlenül és folyamatosan dolgozik.
    - WSOLA Sum of Absolute Differences (SAD) fázisillesztés a dinamikatartomány torzításának kivédésére.
  - **BPM Oktáv Védelem & Felhasználói Vezérlők:**
    - Az elemző felismeri és a 90–175 BPM közötti tánczenei tartományba emeli az eddig 65–71 BPM-re felezett ritmusokat.
    - Deck ablak felső sávjában azonnali `[/2]`, `[x2]`, `[3/2]` gombok a BPM gyors felezéséhez, duplázásához és másfélszerezéséhez.
    - Médiatár (Tracklist) jobb klikkes menü: `⚡ BPM Duplázása (x2)`, `⚡ BPM Felezése (/2)`, `⚡ BPM Másfélszerezése (x1.5)`.
    - Dupla kattintás a táblázat BPM oszlopán: azonnali numerikus párbeszédablak közvetlen kézi BPM megadáshoz.
    - `.vdj.json` gyorsítótár és `vj_tracklist.json` szinkronizáció és perzisztencia.

### Mérföldkő #14: Build #272 / GitHub Run #272 (RGB/BGR & Szaturáció Inverzió, Crossfader FX Moduláció & 3 Új Hű ISF Shader)
* **Verzió & Commit:** `2be2012`
* **SHA256:** `915AA888D321809C71BB833E5F95B87E0719793CD2D404FCB75163A7FB798B42`
* **GitHub Run ID:** `44181503348`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (750 592 bájt)
* **Jellemzők:**
  1. **RGB/BGR és Szaturáció Irány Inverzió (`[ ⇄ BGR ]` Gomb):**
     - Mindkét keverőcsatornán egyedi toggle gomb az EQ potméterek képi színkorrekciós leképezésének és a szaturáció irányának megfordítására.
     - Normál mód (`[ ⇄ BGR ]` kikapcsolva): HI = Red, MID = Green, LOW = Blue, Filter = $1.0 + \text{flt}$.
     - Megfordított mód (`[ ⇄ RGB ]` bekapcsolva): HI = Blue, MID = Green, LOW = Red, Filter = $1.0 - \text{flt}$ (a potméter ellenkező irányba színez/telít).
     - Mind a Traktor S2 fizikai potméterei, mind a felületi EQ csúszkák támogatják az inverziót, és a beállítás elmentődik a `.vdj.json`-ba.
  2. **Crossfader Vizuális FX Paraméterezés & Zenei Moduláció (`CrossfadeFxDialog`):**
     - A Master Crossfader átmeneti effektjei (12+ shader) valós időben paraméterezhetővé váltak az `intensity` (0.0x–2.0x) és `rate` (0.1x–3.0x) értékekkel.
     - A Crossfader melletti `[ ⚙ FX ]` gombbal megnyitható kompakt dialógus (380×215 px).
     - 4 modulációs algoritmus: `Kézi (Off)`, `Lábdob pulzus (Kick)`, `Zenei Tónus (Macro-Tonus)` és `Mindkettő (Both)`.
  3. **3 Új Magas Hűségű ISF Shader Integrálása (Összesen 15 Shader):**
     - #12 **Electro Bands** (echophons): pulzáló neon energiasávok a lábdob és frekvencia dinamikájára.
     - #13 **GlobularCluster** (mojovideotech): 3D gömbhalmaz torzulás és volumetrikus örvénylés.
     - #14 **HexTruchWarpFlow2** (mojovideotech): procedurális hexagonális Truchet rácsáramlás.
     - Teljes per-track és per-shader potméter memória mind a 15 shaderre.

### Mérföldkő #15: Build #274 / GitHub Run #274 (Kifordított Crossfade FX Paraméterek & Egyenkénti Zenei Moduláció)
* **Verzió & Commit:** `9798de7`
* **SHA256:** `32AE7B5504021EA9044675B532D282DBF5453B4ECB55C60376B1BAA833CCAB8B`
* **GitHub Run ID:** `44189017987`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (751 616 bájt)
* **Jellemzők:**
  1. **Effektenként Kifordított 8 Egyedi Paraméter (`CrossfadeFxDialog`, 480 × 440 px):**
     - Nem csupán globális intenzitás és ráta állítható, hanem mind a 13 Crossfade FX átmenethez ki lettek fordítva a valódi, specifikus paraméterek:
       * **Kaleidoszkóp (Mirror Fold):** `sides` (Tükrök száma: 2–16 valós poláris tükrözéssel), `rot_speed` (Forgási sebesség), `zoom`, `fold_angle`, `center_x`, `center_y`, `blend`.
       * **Spirális Örvény (Swirl Vortex):** `twist` (Csavarás szöge), `radius`, `dir` (Forgásirány), `zoom`, `center_x`, `center_y`.
       * **Halszem Optika (Fisheye Lens):** `distortion`, `zoom`, `vignette`, `center_x`, `center_y`.
       * **RGB Split (Kromatikus Aberráció):** `distance` (Szétválási távolság), `angle`, `green_shift`, `blur`, `sat_boost`.
       * **Radial Zoom Blur:** `amount`, `samples`, `center_x`, `center_y`, `decay`, `glow`.
       * **Pixelesedés (Pixelate):** `max_grid`, `min_grid`, `aspect`, `grid_border`, `color_quant`.
       * **Luma Dissolve:** `softness`, `glow`, `glow_r`, `glow_g`, `glow_b`, `invert`.
       * **Folyadék Hullámzás (Liquid Ripple):** `wave_count`, `speed`, `amplitude`, `aspect_x`, `decay`, `surge`.
       * **Glitch Displacement:** `bands`, `speed`, `shift`, `jitter_freq`, `color_split`.
       * **CRT / VHS Scanline:** `density`, `intensity`, `curvature`, `flash`, `vignette`.
       * **Pop-Art Halftone:** `grid_x`, `grid_y`, `dot_size`, `softness`, `posterize`, `brightness`.
       * **Neon Kontúr (Sobel Edge):** `thickness`, `glow`, `neon_color`, `threshold`, `blend_base`.
       * **Normál Átúsztatás (Fade):** `curve` (Görbület), `smoothness` (S-Curve lágyság).
  2. **Paraméterenkénti Független Zenei Modulációs Algoritmusok:**
     - Minden egyes csúszka mellett önálló modulációs választó található:
       * `✋ Kézi (Off)`: Statikus beállítás.
       * `🥁 Kick Pulzus`: Az adott paraméter értéke a lábdob leütések pillanatában azonnal megrándul a valós idejű basszus tranziensek (`bass_energy`) alapján.
       * `📈 Zenei Tónus`: A zene strukturális felépülését követő Gauss-simított makro-tónus (`macro_tonus`) arányában növeli az adott paramétert a kiállásoktól a drop felé haladva.
       * `⚡ Mindkettő`: A lassú zenei fokozódásra ráül a dobütések gyors lüktetése.
  3. **Effektenkénti Paraméter Memória (Per-Effect Persistence):**
     - A 13 effekt mindegyikének 8 paramétere külön memóriaterületen tárolódik a `DeckPair`-ben. Váltáskor minden módosított beállítás megmarad.
  4. **Lock-Free OBS Render Szál:**
     - A paraméterek Direct3D 11 / HLSL átadása (`cf_p0`..`cf_p7`) 100% zéró-lock atomi olvasással történik a valós idejű renderelési ciklusban.

### Mérföldkő #16: Build #276 / GitHub Run #276 (Tört Ütemes Szinkron, Kézi BPM Zárolás & Crossfade FX Dialog Stabilizálás)
* **Verzió & Commit:** `8b17a5f`
* **SHA256:** `471C11DFF8B2E9F8DC7DE6AF101B74011AEB47E6982BE5CBF79E8174A9029848`
* **GitHub Run ID:** `44202002435`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (751 616 bájt)
* **Jellemzők:**
  1. **Crossfade FX Dialógus Csúszka & Dropdown Stabilizáció:**
     - Megszüntetve az 50 ms-os UI ciklusban futó destruktív `rebuildParamControls()` hívás.
     - A csúszkák és a legördülő modulációs menük stabilak maradnak, azonnal foghatók és nem tűnnek el kattintáskor.
  2. **Kézi BPM & Beatgrid Zárolás (`user_bpm_locked`, `user_grid_locked`):**
     - A `.vdj.json` gyorsítótár és a motor megjegyzi a kézi beállításokat (`[edit]`, `/2`, `x2`, `TAP`, vagy rácstolás).
     - Bármilyen háttér-analízis (pl. Camelot Key vagy Gain pótlása) szigorúan tiszteletben tartja és soha nem írja felül a felhasználó által beállított BPM-et és rácsot.
  3. **Kétsávos (Kick + Snare) Tranzienstelítés Tört Ütemekhez:**
     - Az `AnalyzerBeats` a 160 Hz-es lábdob mellett bevonja a 200–2500 Hz-es Mid sávot (Snare, Clap) a novelty energianövekménybe (`0.65 * kick + 0.35 * snare`).
     - Tört ritmusoknál (Breakbeat, Drum & Bass, UK Garage) az autokorreláció a valódi 140 / 174 egész BPM-et találja meg.
  4. **Bit-Perfect Varispeed & Adaptív Key Lock:**
     - Kikapcsolt Key Lock mellett a lejátszás szigorúan bit-perfect matematikai mintalépkedéssel (`playback_sample_pos_ += step`) fut.
     - Bekapcsolt Key Lock esetén az ablakkeresés adaptívan igazodik a sebességeltéréshez (`K_max = clamp(|speed - 1| * 512 + 16, 16, 192)`).
  5. **0.001 BPM Tűpontos Sync Motor:**
     - A Sync holtsáv lecsökkentve 0.05-ről 0.001 BPM-re.

### Mérföldkő #17: Build #279 / GitHub Run #279 (Bipoláris Moduláció, 3-Sávos Pulzus, 2x BPM LFO, Szimmetrikus Kaleidoszkóp & 8 Paraméteres VJ Shaderek)
* **Verzió & Commit:** `8634ae2`
* **SHA256:** `AA45B4E24690FCDAF4806C7BCBABB0176FEBE90A7206625361B3FAE9B28C7355`
* **GitHub Run ID:** `44399542597`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (770 048 bájt)
* **Jellemzők:**
  1. **Kattintásra Beírható Számértékek & `[-]` / `[+]` Finomhangoló Gombok:**
     - Mind a `VjFxDialog`, mind a `CrossfadeFxDialog` felületén a numerikus értékjelzők kattintható `QLineEdit` beviteli mezők lettek közvetlen billentyűzetes beírással, fókuszvesztésre / Enterre azonnali validációval.
     - Minden csúszka mellett kompakt `[-]` és `[+]` gombok a lépésközönkénti (`step`) tűpontos állításhoz.
  2. **Középpont Eltolások Eltávolítása & Szimmetrikus Kaleidoszkóp Belső Képeltolással:**
     - Eltávolítva az aszimmetriát és torzítást okozó `center_x` és `center_y` eltolás minden crossfade effektből (rögzített képernyő-középpont: `(0.5, 0.5)`).
     - A Kaleidoszkóp effekt 16:9 képarány-kompenzált poláris transzformációt kapott, folytonos háromszöghullámos tükrözéssel (`abs(a_mod*2-1)`), tökéletes szimmetriával és belső képeltolással (`pan_x`, `pan_y`).
  3. **8 Kifordított Paraméter Mind a 15 VJ Shaderben:**
     - Mind a 15 VJ shaderhez ki lett alakítva a 8 aktív paraméter (`p0`..`p7`), a HLSL kódokban közvetlen funkcióhoz kötve (részecskesűrűség, színezet, sebesség, deformáció, emisszió, mélység, stb.).
  4. **Kétsoros Paraméterkártya & Bipoláris Modulációs Mélység (-100% .. 0% .. +100%):**
     - 1. sor: Alapérték csúszka + beviteli mező + `[-]`/`[+]` gombok.
     - 2. sor: Modulációs forrás választó + Bipoláris potméter (-1.0 .. +1.0, középállásban 0.0 = 0% hatás) + közvetlen százalékos beviteli mező + léptető gombok. Pozitív és invertált fázisú moduláció tetszőleges arányban keverhető.
  5. **3-Sávos Zenei Pulzus Envelopes:**
     - `Bass Pulse` (< 250 Hz - kick és szub), `Mid Pulse` (250 Hz – 5000 Hz - pergő, vokál, szintik), `High Pulse` (> 5000 Hz - cinek, lecsengések).
  6. **Kettős BPM-Szinkronizált LFO Motor & Lebegő `LfoDialog` (520 × 400 px):**
     - Ütemosztások és szorzatok: `1/16, 1/8, 1/4, 1/2, 1, 2, 4, 8, 16, 32, 64` ütem!
     - 4 klasszikus hullámforma: Szinusz, Háromszög, Fűrészfog (fel/le), Kocka.
     - Állítható paraméterek: hullámforma, ráta, depth (mélység), offset, phase (fázis 0°..360°).
     - Élő, valós idejű mini oszcilloszkóp widget (`LfoScopeWidget`) a futó hullámforma és fázis vizualizálásához.
     - Megnyitható a mixer mester panelének `[ 〰 LFO ]` gombjából és az OBS Eszközök menüből (`OBS VJ LFO Oszcillátor`).
  7. **Crossfade FX Legördülő Menü Automatikus Kiszélesítése:**
     - A Master Mixeren lévő kompakt dropdown kinyitásakor minimum 280 px szélességre nyílik (`QAbstractItemView` nézet), így a leghosszabb effektnevek is tisztán olvashatók.

### Mérföldkő #18: Build #282 / GitHub Run #282 (Enter Fókuszjavítás, Duplaklikkes Potméter Reset, Forgásmentes Kaleidoszkóp, Feketeszint & Kontraszt Védelem)
* **Verzió & Commit:** `9f6b67d`
* **SHA256:** `74D85BE24020934BF622D48C7F0F5612986C856DEBD8285679BE228E6E5C4CAC`
* **GitHub Run ID:** `44423572351`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (773 120 bájt)
* **Jellemzők:**
  1. **Enterre Defaultra Állás Megszüntetése & Default Gomb Törlése:**
     - A `Reset FX` gomb törölve a dialógus fejlécéből (ez aktiválódott korábban a Qt autoDefault mechanizmusa miatt az Enter leütésekor).
     - Minden gombon explicit módon tiltva az `autoDefault` és `default` viselkedés.
     - A `QLineEdit` mezőkben az Enter leütése érvényesíti az értéket és elengedi a fókuszt (`clearFocus()`), a `keyPressEvent` pedig elnyeli az Enter/Return billentyűt, így a dialógus semmilyen akciót nem vált ki.
  2. **Duplaklikkes Reset Minden Potméteren (`DJSlider`):**
     - Mind a `CrossfadeFxDialog`, mind a `VjFxDialog` összes csúszkája `DJSlider`-re cserélve:
       * **Alapérték csúszka:** Dupla kattintásra azonnal a specifikációban definiált gyári alapértékre (`def_val`) ugrik.
       * **Bipoláris Modulációs Mélység csúszka:** Dupla kattintásra azonnal `0%`-ra (középállásba, inaktív állapotba) ugrik.
  3. **Kaleidoszkóp Forgás Teljes Eltávolítása:**
     - A `rot_speed` és `anim_time * rot_spd` forgási számítás törölve a HLSL kódból.
     - 8 tiszta, szimmetrikus paraméter: `sides`, `zoom`, `pan_x`, `pan_y`, `fold_angle` (statikus dőlésszög, nincs forgás), `softness`, `contrast`, `blend`.
  4. **Finom, Nem Ugrásszerű Paraméterek & Szimmetria:**
     - A korábbi durva, ugrásszerű Glitch helyett **Harmonikus Pásztázás** (Scan Dispersion): szimmetrikus, simított szinuszos hullámzó pásztázás és színcsúszás a képernyő középpontja körül.
     - Minden effekt rögzített `(0.5, 0.5)` középponttal üzemel.
  5. **Hülyebiztos Feketeszint & Kontraszt Védelem (No-Fogging Rendering):**
     - **VJ Shaderek:** Megszüntetve az összes képernyőszintű lapos téglalap-villantás (`col += theme * bass_energy * 0.4`), ami eddig szürkévé és tejszerűvé mosta a fekete hátteret. A zenei lüktetés kizárólag a meglévő geometriai vonalakat, ragyogásokat és csúcsfényeket modulálja vagy luminancia-maszkkal érvényesül. A háttér mélyfekete (`0.0`) marad!
     - **Mixer Forrás:** A végső pixelfeldolgozóban mélyfekete levágási küszöb (`smoothstep(0.015, 1.0, res.rgb)`) és lágy S-görbe tónus, amely több réteg és összetett effektek egyidejű használata mellett is borotvaéles, telt és dinamikus képet garantál.
  6. **MSVC C2026 Védelem a Mixer Shaderben:**
     - A `MIXER_EFFECT` HLSL kód méretnövekedése miatt a shader két részre (`MIXER_EFFECT_PART1` és `PART2`) lett bontva, megkerülve az MSVC 16 KB-os string literál korlátját.


### Mérföldkő #19: Build #284 / GitHub Run #284 (Pontos Ablakelrendezés & ISF Shader Beágyazó Motor / IsfConverterDialog)
* **Verzió & Commit:** `b1d2def`
* **SHA256:** `0FC17DF09535175D44CB2F36D888EF4E8BBF638C2564FEE6A78D467B32E448BC`
* **GitHub Run ID:** `44478061391`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (967 680 bájt)
* **Jellemzők:**
  1. **Pontos Ablakelrendezés & Tálca Védelem:**
     - Az alap 3 ablak (Deck A, Mixer, Deck B) alulról megemelve (`y = geom.bottom() - 340 - 40`), így a 340 px-es kliensfelület a Windows ablakfejléccel és árnyékokkal együtt sem lóg bele a Windows tálcába.
     - A további lebegő eszközök a felhasználó által beküldött elrendezés szerint pontos koordinátákra nyílnak:
       * **Bal felül:** `CrossfadeFxDialog` (`x = geom.left() + 12, y = geom.top() + 38`)
       * **Jobb felül:** `VjFxDialog` (`x = geom.right() - target_w - 12, y = geom.top() + 38`)
       * **Közép-bal:** `LfoDialog` (`x = geom.left() + 315, y = geom.top() + 390`)
       * **Közép-jobb:** `TracklistDock` (`x = geom.left() + 845, y = geom.top() + 390`)
       * **Bal alul:** `Deck A` (`x = geom.left() + 12, y = geom.bottom() - 340 - 40`)
       * **Közép alul:** `Mixer` (`x = geom.left() + 665, y = geom.bottom() - 340 - 40`)
       * **Jobb alul:** `Deck B` (`x = geom.left() + 1268, y = geom.bottom() - 340 - 40`)
  2. **ISF Shader Beágyazó Motor & Konverter (`CustomShaderManager` & `IsfConverterDialog`):**
     - `%APPDATA%/obs-studio/plugin_config/obs_vj_dj_plugin/custom_shaders.json` fájlba perzisztálja az egyedi felhasználói shadereket.
     - A VJ Shader ablakból elérhető `[ 🪄 ISF Konverter ]` gomb egy önálló stúdió dialógust (780 × 680 px) nyit meg.
     - **Egy kattintásos AI Prompt sablon:** `[ 📋 AI Prompt Másolása Vágólapra ]` gomb kimásolja a szigorú szabványos direktívát az AI-k (ChatGPT, Claude, Gemini, DeepSeek) számára:
       * 16:9 középpontos szimmetria (`uvn = (uv - 0.5) * float2(16.0 / 9.0, 1.0)`).
       * Zéró aszimmetrikus középpont-eltolás vagy dőlés.
       * Mély fekete háttér és kontrasztvédelem (köd- és fehéredésvédelem).
       * Zenei lábdob tranziens lüktetés (`bass_energy`) integráció.
       * Pontosan 8 kifordított paraméter (`p0`..`p7`).
     - **Automatikus Tag- és Kód Parser:** Értelmezi a `[NAME]`, `[CATEGORY]`, `[PARAMS]`, `[CODE]` blokkokat, valamint a JSON és a nyers HLSL kódokat is.
     - **Élő GPU Direct3D 11 Tesztfordítás (`testCompileShader`):** A `[ ⚡ Tesztelés & Hibaellenőrzés ]` gomb valós időben megkísérli lefordítani a kódot a Direct3D 11 GPU pipeline-on, azonnal visszajelezve a sikert vagy a pontos hibaüzenetet/sorszámot.
     - **Azonnali Beágyazás & Mentés Hot-Reloadinggal:** A `[ 💾 Beágyazás & Mentés ]` gomb elmenti a shadert, frissíti mindkét deck VJ Shader legördülő menüjét, azonnal kiválasztja az új shadert, és a `CustomShaderManager::version()` mechanizmusnak köszönhetően OBS újraindítás nélkül azonnal felépíti a GPU pipeline-t!
     - Kiterjesztett shader paraméter tömb (64 slot) mind a memóriában, mind a `.vdj.json` gyorsítótárban.


### Mérföldkő #20: Build #287 / GitHub Run #287 (Szimmetriapontok és Vignetta Törlése, Világos Teljes Képernyős VJ Shader Motor)
* **Verzió & Commit:** `b159f14`
* **SHA256:** `360F9105EDFF724466D21AE0EE6EC5960F0E0E1E37EECB5AEDBB3021F6F42C92`
* **GitHub Run ID:** `44484838750`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (968 192 bájt)
* **Jellemzők:**
  1. **ISF Eredeti Geometria 1:1 Megőrzése (Kaleidoszkóp Szeletelés Törlése):**
     - Az AI sablonból és a shader pipeline-ból teljesen eltávolítva a kötelező szimmetriapontok és poláris tükrözések (`atan2`, `sector`, `sym_a`) előírása.
     - Az AI-nak szigorúan tilos mesterséges tükrözést vagy pizzaszelet-torzítást belevinnie a shaderekbe; az eredeti ISF kód geometriáját 1:1-ben kell megőrizni.
  2. **Képernyősötétedés & Vignettálás Felszámolása:**
     - Tilos a képet elsötétítő körmaszk (`smoothstep(1.05, 0.15, r)`), a sötétítő hatványozás (`pow(col, >1.0)`) és a szűk vágás.
     - A shaderek a teljes 16:9 vásznon élénken, fényesen, telt neon színekkel és tiszta kontraszttal jelennek meg alapbeállításon is.
  3. **Valós Eredeti Paraméter Leképzés:**
     - A `p0`..`p7` paraméterek közvetlenül az eredeti shader saját, létező változóit vezérlik (rate, zoom, density, detail, glow, shift, pulse).
  4. **TIME, iTime, RENDERSIZE, iResolution & rotate2D Beépített Aliasok:**
     - Teljes kompatibilitás mind az ISF, mind a ShaderToy formátumokkal fordítási hiba nélkül.
  5. **Felhasználói `custom_shaders.json` Azonnali Kijavítása:**
     - A korábbi "Symmetric Smooth XOR" átírva a tiszta, világos és szimmetriatorzulástól mentes "Smooth Matrix XOR" verzióra.

### Mérföldkő #21: Build #289 / GitHub Run #289 (Natív C++ ISF & GLSL Transzpiláló Motor, Külső AI Igény Nélkül)
* **Verzió & Commit:** `925ced1`
* **SHA256:** `5137D9133F1B6F774E13ABD5B805F85C76D437999B856B510184E19F406D3FBF`
* **GitHub Run ID:** `44601675463`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (970 240 bájt)
* **Jellemzők:**
  1. **100% Natív C++ ISF Transzpiláló Motor (`CustomShaderManager::transpileIsfToHlsl`):**
     - Nincs szükség külső AI asszisztensre, chat-ablakra vagy külső eszközökre.
     - Közvetlenül és azonnal fogadja az `editor.isf.video`, `.fs` fájlokat és ShaderToy kódokat.
  2. **Automatikus JSON Fejléc és Paraméter Elemzés:**
     - `/*{ ... }*/` JSON blokkból kinyeri a nevet (`DESCRIPTION`/`CREDIT`), kategóriát (`CATEGORIES`) és az `INPUTS` listát.
     - Akár 8 bemenetet automatikusan leképez `p0`..`p7` paraméterekre (név, min, max, default), és a kód tetején `#define <input_name> p0..p7` direktívákkal globálisan elérhetővé teszi minden segédfüggvény számára.
  3. **Intelligens GLSL $\rightarrow$ Direct3D 11 HLSL Transzpilálás:**
     - Zárójel-mélység elemzővel kezeli a 2-argumentumos `atan(y, x)` $\rightarrow$ `atan2(y, x)` és 1-argumentumos `atan(x)` függvényeket.
     - Automatikusan átalakítja az 1-argumentumos skalár vektor-konstruktorokat `floatN(val)` $\rightarrow$ `(floatN)(val)`.
     - Túlterhelt `glsl_mod` vektorokra és skalárokra, `Rot2D` / `rot2D` aliasok.
     - Belépési pont átalakítása: `void main()` $\rightarrow$ `float4 render_custom(float2 uv)` helyi `gl_FragColor` inicializálással és `saturate` lezárással.
  4. **Kezelőfelületi Bővítések (`IsfConverterDialog`):**
     - `[ 📂 .fs Fájl Tallózása ]`: Helyi lemezről azonnal betölt és átalakít `.fs` vagy `.txt` fájlokat.
     - `[ ⚡ ISF Átalakítás ]`: A beillesztett kódot egyetlen kattintással HLSL-re fordítja.
     - `[ 📋 Vágólap Beillesztése ]`: Automatikus ISF felismerés és azonnali GPU tesztfordítás.

### Mérföldkő #22: Build #290 / GitHub Run #290 (ISF Transzpiler GPU Hibajavítás, Zéró Overload és UI Szinkronizáció)
* **Verzió & Commit:** `9645012`
* **SHA256:** `8B704349C18F337B77BF740401FC896D3F9ECFA9AC64CF76CD10D3ECA5D103FA`
* **GitHub Run ID:** `44608521617`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (970 752 bájt)
* **Jellemzők:**
  1. **libobs OBS-SL Parser `Expected ',' or ')'` Hiba Megszüntetése:**
     - **GLSL `in` paraméter-minősítő lecsupaszítása:** A GLSL függvényfejlécekben lévő `in` kulcsszót (`float random(in vec2 st)`, `vec3 invertColor(in vec3 color)`) az OBS-SL parser nem minősítőként, hanem típusként értelmezte. A transzpiler most automatikusan eltávolítja az `in` minősítőt a paraméterdeklarációkból.
     - **Függvény-túlterhelés (Function Overloading) Megszüntetése:** Az OBS Studio effekt-értelmezője (`shader-parser.c`) nem támogatja a függvények túlterhelését. A `glsl_mod` helyett szigorúan egyedi függvénynevek kerültek bevezetésre (`glsl_mod`, `glsl_mod2`, `glsl_mod2s`, `glsl_mod3`, `glsl_mod3s`, `glsl_mod4`, `glsl_mod4s`).
     - **Biztonságos Vektor Konstruktorok (`to_float2`, `to_float3`, `to_float4`):** Dedikált segédfüggvények hozzák létre a skalárból sugárzott vektorokat.
  2. **Kétoldali UI Mező Szinkronizáció (`IsfConverterDialog`):**
     - A tesztelés és átalakítás során automatikusan megjelenik a kinyert név, kategória és a tiszta HLSL kód a szerkesztőben.

### Mérföldkő #23: Build #291 / GitHub Run #291 (Többmint-8 Bemenetes ISF Shaderek és Vektor Cast Hibajavítás)
* **Verzió & Commit:** `2b82f07`
* **SHA256:** `20A5BA4E38FD91ACBBB98713C1057FE4A0C11DD4F8C6E2E131B58A769A91E9A2`
* **GitHub Run ID:** `44612830884`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (971 776 bájt)
* **Jellemzők:**
  1. **8-nál több bemenettel rendelkező ISF shaderek teljes támogatása (Overflow Inputs):**
     - Korábban, ha egy ISF shaderben 8-nál több `INPUT` szerepelt (pl. a Joseph Fiola shaderben 10 bemenet: `pos`, `invert`, `function`, `speed`, `multiplier`, `grid`, `detail`, `contrast`, `contrastShift`, `mode`), a ciklus 8 után megszakadt, és a kimaradó változók (`contrast`, `contrastShift`, `mode`) deklarálatlanok maradtak (`undeclared identifier`), meghiúsítva a fordítást.
     - **Megoldás:** Az első 8 paraméterkapcsolat megkapja a valós idejű `p0`..`p7` MIDI/LFO csúszkákat, a 8 feletti bemenetek pedig automatikusan beégetésre kerülnek a shader kód elejére a JSON fejlécben megadott `DEFAULT` értékeikkel (`#define contrast (0.0)`, `#define mode (0)`). Így a shader hibátlanul fordul és fut le!
  2. **Intelligens Vektor Cast (`to_floatN` védelem):**
     - Ha a forrásban a változó eleve vektor volt (pl. `st -= vec2(pos);` ahol a `pos` már `float2`, vagy `return vec3(color * -1.0 + 1.0);`), a korábbi logika skalárnak hitte és meghívta a `to_float2(pos)` függvényt, ami típuseltérési hibát okozott (`cannot convert from 'float2' to 'float'`).
     - **Megoldás:** A transzpiler most felismeri, ha a zárójelen belüli kifejezés már az adott vektortípust képviseli, és nem hív felesleges `to_floatN` konverziót.
  3. **Vezérlési Folyam Lezárási Védelem (HLSL X4014):**
     - Ha egy segédfüggvény (pl. `float random(float2 st)`) csupán `if (...) return ...; else if (...) return ...;` ágakkal zárult feltétel nélküli záró `return` nélkül, a Direct3D 11 HLSL fordító elutasította a kódot.
     - **Megoldás:** A segédfüggvények törzsének végére automatikusan bekerül egy biztonsági `return 0.0;` lezárás.
  4. **Automatikus Névgenerálás Üres `DESCRIPTION` esetén:**
     - Ha a JSON-ban a `DESCRIPTION` üres sztring, a transzpiler a `CREDIT` mezőből hoz létre érvényes nevet (pl. "Joseph Fiola Shader").


### Mérföldkő #24: Build #292 / GitHub Run #295 (Crossfade FX Keverési Görbe és Effekt-Paraméter Szétválasztás)
* **Verzió & Commit:** `2ed2c58`
* **SHA256:** `B9C3AD54466E7D84A82C026D5853D48BD1D6A884007E2E9A77316EBACEA71281`
* **GitHub Run ID:** `44623244789`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (1 061 376 bájt)
* **Jellemzők:**
  1. **Globális Crossfade Keverési Görbe Függetlenítése (`cf_curve`, `cf_smooth`):**
     - Korábban a `src/video/mixer_source.cpp` `PS` belépési pontjában a lineáris átúsztatás görbéjét a `cf_p0` és `cf_p1` paraméterekből számolta (`cf = pow(saturate(cf), cf_p0)`).
     - Ha bármely Crossfade effekt (pl. Swirl twist=5.0, Pixelate tiles=30.0, CRT lines=300.0) aktív volt, a crossfader arányát ez a hatalmas hatvány kitevő 80%-ig szinte nullán tartotta, ami miatt a fader 2/3-ánál (`34% A | 66% B`) sem látszott még semmi a Deck B-ből, majd a legvégén hirtelen átugrott.
     - **Megoldás:** Dedikált `cf_curve` és `cf_smooth` uniform változók kerültek bevezetésre. A `cf_p0`..`cf_p7` tömb kizárólag a kiválasztott effekt belső képi torzítását végzi, míg az alatta lévő két deck/shader keverése folytonos, lágy és arányos marad.
  2. **Valós Idejű Videó-Bizonyítás:**
     - Az asztali videófelvétel (`Rögzítés 2026-11-19 224148.mp4`) alapján pontosan igazolt hiba elhárítva, letesztelve és telepítve.


### Mérföldkő #25: Build #293 / GitHub Run #297 (Új Beállítások Ablak: ASIO, Külső Keverő, MIDI Profilok & Nyelv)
* **Verzió & Commit:** `6526e87`
* **SHA256:** `17A22616D82F8DF97D629BFFAA0E0E3B42C96FCBFEE93F47959C3A2535D2F45B`
* **GitHub Run ID:** `44629047953`
* **Kész bináris mentés:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (1 123 328 bájt)
* **Jellemzők:**
  1. **Önálló `SettingsDialog` Lebegő Ablak (680×540 px):**
     - Elérhető az OBS **Eszközök $\rightarrow$ OBS VJ Beállítások (Settings)** menüjéből és a Mixer ablak alsó sávjában lévő `[ ⚙ BEÁLLÍTÁSOK ]` gombbal.
  2. **1. Fül: Audió & Hangkártya (ASIO + WASAPI + Külső Keverő):**
     - Automatikusan felderíti a Windows Registry-ből az összes telepített ASIO drivert (pl. DJM-450 ASIO Driver, Akai MPC, FlexASIO, Voicemeeter, Korg Kaoss DJ).
     - Támogatja a **Belső Keverő (Internal Mixer)** (Master 1-2 + CUE füles 3-4) és a **Külső Keverő / Split Deck (External Mixer)** (Deck A $\rightarrow$ Ch 1-2, Deck B $\rightarrow$ Ch 3-4 pl. Pioneer DJM-350-hez) működést.
     - Állítható mintavételezési ráta (44.1k, 48k, 96k) és pufferméret (64–1024 minta).
  3. **2. Fül: Videó & Shaders:**
     - Kimeneti felbontás (1080p FHD, 720p HD, Vászonhoz igazodó).
     - Alapértelmezett méretezési mód (FIT aránytartó, FILL kitöltés, STRETCH nyújtás).
     - VJ Shader renderelési minőség választó (1080p teljes vagy 540p feles felbontás).
  4. **3. Fül: MIDI & Kontrollerek (Presets & MIDI Learn):**
     - WinMM bemeneti és kimeneti eszközök listája.
     - Gyári profilok: Traktor Kontrol S2 MK3, Korg Kaoss DJ (Touchpad VJ p0/p1 bekötéssel), Pioneer CDJ-350 / DDJ.
     - `[ 📥 Export JSON ]` és `[ 📤 Import JSON ]` funkciók a `.midimap.json` fájlok kezelésére.
     - Interaktív MIDI Learn lista táblázat `[ 🔴 LEARN ]` gombokkal.
  5. **4. Fül: Nyelv & Rendszer (Multilingual / i18n):**
     - Nyelvválasztó: Magyar (HU), Angol (EN), Német (DE), Francia (FR).
     - Automatikus mentés és betöltés (`settings.json`).


### Mérföldkő #26: Build #294 / GitHub Run #299 (Teljes Bemeneti & Kimeneti Audió Mátrix: Deckenkénti In/Out, Master, CUE, Booth & Ch 1-16)
* **Verzió & Commit:** `4861847`
* **SHA256:** `36A9504FA514ECA908831250C209D854E3047FD65A3D8E60919AE7D8A1532694`
* **GitHub Run ID:** `44633166869`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (1 165 312 bájt)
* **Jellemzők:**
  1. **Teljes Kimeneti Audió Mátrix (Output Routing Matrix):**
     - **Főkimenet (Master Out):** Független Hangkártya Eszköz választó (WASAPI / ASIO) és L/R csatorna választók (Ch 1 - Ch 16).
     - **Fejhallgató CUE (PFL Out):** Független kártya és L/R csatornák (alap: Ch 3-4).
     - **Monitor / Booth Out:** Dedikált monitorozó kimenet klub/stúdió hangfalakhoz L/R csatornaválasztással.
     - **Deck A Közvetlen Kimenet:** Külső analóg/digitális keverőhöz (pl. Pioneer DJM-350 Ch 1/2 bemenetre).
     - **Deck B Közvetlen Kimenet:** Külső keverőhöz (pl. Pioneer DJM-350 Ch 3/4 bemenetre).
  2. **Teljes Bemeneti Audió Mátrix (Input Routing Matrix):**
     - **Deck A Vonal/Phono In:** Külső lemezjátszó/CDJ vagy vonalbemenet fogadása külön bal/jobb csatornával.
     - **Deck B Vonal/Phono In:** Külső bemenet fogadása Deck B-hez.
     - **Aux / Mikrofon Bemenet:** Külön mikrofon / élő beszéd bemenet csatornaválasztókkal.
  3. **Gördíthető Kényelmes Felület (`QScrollArea`):**
     - Az audió fül görgethetővé vált, így alacsonyabb képernyőfelbontásokon is tökéletesen átlátható és kezelhető az összes beállítás.
  4. **Telepítés és Hitelesítés:**
     - Sikeresen felülírva és ellenőrizve az OBS telepítési könyvtárában (`C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll`) és az Asztalon.
