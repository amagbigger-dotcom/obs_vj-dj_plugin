# OBS VJ/DJ Mixer Plugin - Teljes Építési Történet és Mérföldkövek Archívuma (BUILD HISTORY)

Ez a dokumentum a projekt teljes történeti fejlődését, a korábbi build naplókat, a részletes funkcionalitási jegyzékeket, a git commit hash-eket és az SHA256 lenyomatokat tartalmazza a Build #190-től a legújabb Build #312-ig.

---

# I. RÉSZ: HIVATALOS MÉRFÖLDKÖVEK ÉS RÉSZLETES VÁLTOZÁSNAPLÓK (BUILD #222 – #312)

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
* **Jellemzők:** Key Lock reszamplálás (44.1k -> 48k), Per-Shader potméter perzisztencia (12 shader), Egyedi színválasztó és 4 színmodulációs algoritmus, 8-bucketos Gauss makro-tónus feszültség-szalag, BPM /2 és x2 gombok a fejlécben.

### Mérföldkő #14: Build #272 / GitHub Run #272 (RGB/BGR & Szaturáció Inverzió, Crossfader FX Moduláció & 3 Új Hű ISF Shader)
* **Verzió & Commit:** `2be2012`
* **SHA256:** `915AA888D321809C71BB833E5F95B87E0719793CD2D404FCB75163A7FB798B42`
* **GitHub Run ID:** `44181503348`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (750 592 bájt)
* **Jellemzők:** RGB/BGR inverzió (`[ ⇄ BGR ]`), `CrossfadeFxDialog` zenei moduláció, 3 új ISF shader (Electro Bands, GlobularCluster, HexTruchWarpFlow2 - összesen 15 shader).

### Mérföldkő #15: Build #274 / GitHub Run #274 (Kifordított Crossfade FX Paraméterek & Egyenkénti Zenei Moduláció)
* **Verzió & Commit:** `9798de7`
* **SHA256:** `32AE7B5504021EA9044675B532D282DBF5453B4ECB55C60376B1BAA833CCAB8B`
* **GitHub Run ID:** `44189017987`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (751 616 bájt)
* **Jellemzők:** Effektenként 8 kifordított egyedi paraméter a Crossfade FX dialógusban, független modulációs algoritmusok (Kick, Makro-tónus, Both), lock-free OBS render átadás.

### Mérföldkő #16: Build #276 / GitHub Run #276 (Tört Ütemes Szinkron, Kézi BPM Zárolás & Crossfade FX Dialog Stabilizálás)
* **Verzió & Commit:** `8b17a5f`
* **SHA256:** `471C11DFF8B2E9F8DC7DE6AF101B74011AEB47E6982BE5CBF79E8174A9029848`
* **GitHub Run ID:** `44202002435`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (751 616 bájt)
* **Jellemzők:** Dropdown stabilizálás UI ciklusban, Kézi BPM és grid zárolás (`user_bpm_locked`), Kétsávos (Kick+Snare) tranziens Breakbeat-hez, 0.001 BPM tűpontos Sync motor.

### Mérföldkő #17: Build #279 / GitHub Run #279 (Bipoláris Moduláció, 3-Sávos Pulzus, 2x BPM LFO, Szimmetrikus Kaleidoszkóp & 8 Paraméteres VJ Shaderek)
* **Verzió & Commit:** `8634ae2`
* **SHA256:** `AA45B4E24690FCDAF4806C7BCBABB0176FEBE90A7206625361B3FAE9B28C7355`
* **GitHub Run ID:** `44399542597`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (770 048 bájt)
* **Jellemzők:** Kattintásra beírható számértékek és léptetők, szimmetrikus kaleidoszkóp belső eltolással, 8 aktív paraméter mind a 15 VJ shaderben, Bipoláris moduláció (-100%..+100%), 3-sávos pulzus envelopes (Bass, Mid, High), Kettős BPM-szinkronizált LFO motor és `LfoDialog` oszcilloszkóppal.

### Mérföldkő #18: Build #282 / GitHub Run #282 (Enter Fókuszjavítás, Duplaklikkes Potméter Reset, Forgásmentes Kaleidoszkóp, Feketeszint & Kontraszt Védelem)
* **Verzió & Commit:** `9f6b67d`
* **SHA256:** `74D85BE24020934BF622D48C7F0F5612986C856DEBD8285679BE228E6E5C4CAC`
* **GitHub Run ID:** `44423572351`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (773 120 bájt)
* **Jellemzők:** Enter leütés fókusz- és alaphelyzetbe állás javítása, Duplaklikkes reset minden potméteren (`DJSlider`), Forgásmentes szimmetrikus kaleidoszkóp, Feketeszint és kontrasztvédelem (No-Fogging) mélyfekete levágási küszöbbel, MSVC C2026 string literál védelem.

### Mérföldkő #19: Build #284 / GitHub Run #284 (Pontos Ablakelrendezés & ISF Shader Beágyazó Motor / IsfConverterDialog)
* **Verzió & Commit:** `b1d2def`
* **SHA256:** `0FC17DF09535175D44CB2F36D888EF4E8BBF638C2564FEE6A78D467B32E448BC`
* **GitHub Run ID:** `44478061391`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (967 680 bájt)
* **Jellemzők:** Pontos ablakkoordináták a képernyő 1/3-án (Tálca védett megemelés: 40 px), ISF Shader konverter és beágyazó motor (`CustomShaderManager`, `IsfConverterDialog`), AI prompt generátor, élő GPU Direct3D 11 tesztfordítás.

### Mérföldkő #20: Build #287 / GitHub Run #287 (Szimmetriapontok és Vignetta Törlése, Világos Teljes Képernyős VJ Shader Motor)
* **Verzió & Commit:** `b159f14`
* **SHA256:** `360F9105EDFF724466D21AE0EE6EC5960F0E0E1E37EECB5AEDBB3021F6F42C92`
* **GitHub Run ID:** `44484838750`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (968 192 bájt)
* **Jellemzők:** ISF eredeti geometria 1:1 megőrzése (poláris szeletelés törlése), vignettálás felszámolása, TIME/iTime/RENDERSIZE kompatibilitási aliasok.

### Mérföldkő #21: Build #289 / GitHub Run #289 (Natív C++ ISF & GLSL Transzpiláló Motor, Külső AI Igény Nélkül)
* **Verzió & Commit:** `925ced1`
* **SHA256:** `5137D9133F1B6F774E13ABD5B805F85C76D437999B856B510184E19F406D3FBF`
* **GitHub Run ID:** `44601675463`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (970 240 bájt)
* **Jellemzők:** 100% natív C++ ISF/GLSL transzpiler C++-ban megírva, Automatikus JSON fejléc és paraméter elemzés, GLSL -> HLSL transzpilálás (atan2, mod, vektor konstruktorok).

### Mérföldkő #22: Build #290 / GitHub Run #290 (ISF Transzpiler GPU Hibajavítás, Zéró Overload és UI Szinkronizáció)
* **Verzió & Commit:** `9645012`
* **SHA256:** `8B704349C18F337B77BF740401FC896D3F9ECFA9AC64CF76CD10D3ECA5D103FA`
* **GitHub Run ID:** `44608521617`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (970 752 bájt)
* **Jellemzők:** libobs parser hiba javítás (`in` kulcsszó lecsupaszítása), függvény túlterhelés (function overloading) felszámolása (`glsl_mod2`, stb.), biztonságos `to_floatN` segédfüggvények.

### Mérföldkő #23: Build #291 / GitHub Run #291 (Többmint-8 Bemenetes ISF Shaderek és Vektor Cast Hibajavítás)
* **Verzió & Commit:** `2b82f07`
* **SHA256:** `20A5BA4E38FD91ACBBB98713C1057FE4A0C11DD4F8C6E2E131B58A769A91E9A2`
* **GitHub Run ID:** `44612830884`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (971 776 bájt)
* **Jellemzők:** 8-nál több bemenetű ISF shaderek kezelése (overflow inputs `#define` default értékekkel), Intelligens vektor cast felismerés, HLSL X4014 biztonsági lezárás segédfüggvények végén.

### Mérföldkő #24: Build #292 / GitHub Run #295 (Crossfade FX Keverési Görbe és Effekt-Paraméter Szétválasztás)
* **Verzió & Commit:** `2ed2c58`
* **SHA256:** `B9C3AD54466E7D84A82C026D5853D48BD1D6A884007E2E9A77316EBACEA71281`
* **GitHub Run ID:** `44623244789`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 061 376 bájt)
* **Jellemzők:** Dedikált `cf_curve` és `cf_smooth` uniformok bevezetése; a képi effekt torzítás (`cf_p0`..`cf_p7`) nem befolyásolja az alatta lévő lineáris video/audio keverési arányt.

### Mérföldkő #25: Build #293 / GitHub Run #297 (Új Beállítások Ablak: ASIO, Külső Keverő, MIDI Profilok & Nyelv)
* **Verzió & Commit:** `6526e87`
* **SHA256:** `17A22616D82F8DF97D629BFFAA0E0E3B42C96FCBFEE93F47959C3A2535D2F45B`
* **GitHub Run ID:** `44629047953`
* **Kész bináris mentés:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (1 123 328 bájt)
* **Jellemzők:** Önálló `SettingsDialog` (680×540 px), ASIO driver felderítés a Windows Registry-ből, Belső és Külső keverő (External Split Deck pl. DJM-350) támogatás, MIDI Learn és JSON profilok, 4-nyelvű lokalizáció (HU, EN, DE, FR).

### Mérföldkő #26: Build #294 / GitHub Run #299 (Teljes Bemeneti & Kimeneti Audió Mátrix: Deckenkénti In/Out, Master, CUE, Booth & Ch 1-16)
* **Verzió & Commit:** `4861847`
* **SHA256:** `36A9504FA514ECA908831250C209D854E3047FD65A3D8E60919AE7D8A1532694`
* **GitHub Run ID:** `44633166869`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 165 312 bájt)
* **Jellemzők:** Teljes kimeneti mátrix (Master, CUE, Booth, Deck A Direct, Deck B Direct Ch 1-16 routing), Teljes bemeneti mátrix (Deck A Line/Phono, Deck B Line/Phono, Aux/Mic), görgethető felület (`QScrollArea`).

### Mérföldkő #27: Build #295 / GitHub Run #302 (Adatvezérelt Kontroller Architektúra, Relatív Jog Dekóder, 200-soros Moduláris Refaktoring)
* **Verzió & Commit:** `f020530`
* **SHA256:** `516ABDAB0A5BBDDE915809717C4567B7B666133EF06BB0D1E0904E04DBCF8B64`
* **GitHub Run ID:** `44640937517`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 220 608 bájt)
* **Jellemzők:** Adatvezérelt MIDI keretrendszer (`MidiAction`, `MidiJogDecoder`, `MidiMappingManager`, `MidiDispatcher`), Traktor S2 MK3, Korg Kaoss DJ, Pioneer DDJ-400/FLX4, Numark, Hercules profilok, Settings dialógus 200 soros modularizációja.

### Mérföldkő #28: Build #296 / GitHub Run #305 (Korg KAOSS DJ Gyári Csatornák [7/8/9], Élő Bemenet-Váltás & Realtime MIDI Monitor)
* **Verzió & Commit:** `ab2326f`
* **SHA256:** `838CC59780E738DBB0BE82050EE45091DC44E3790D06453F4A4CEDB305E2CC5A`
* **GitHub Run ID:** `44698122274`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 229 312 bájt)
* **Jellemzők:** Automatikus valódi MIDI eszköz prioritás (virtuális hálózati portok kizárva), Korg KAOSS DJ gyári csatornák (Ch 7 Global, Ch 8 Deck A, Ch 9 Deck B), Valós idejű élő MIDI monitor a beállítások fülön (`🟢 ÉLŐ JEL ÉRKEZETT`).

### Mérföldkő #29: Build #297 / GitHub Run #44706004528 (Korg KAOSS DJ & Traktor S2 Teljes Hardver Térkép, Valós Jog Scratch & Zenei Navigáció)
* **Verzió & Commit:** `d07141b`
* **SHA256:** `ABDDBC8DD44AC4F22B8440A7790C8B65B396659B9AF1E29057A871A31092FF29`
* **GitHub Run ID:** `44706004528`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 243 136 bájt)
* **Jellemzők:** Valós Jog scratch hangpozicionálás beatgrid nudge helyett (120 ms/tick touch, 25 ms/tick nudge), Korg gyári EQ/Filter címzés (Gain CC 26, Hi CC 27, Mid CC 28, Low CC 29, Filter CC 22/30), Tracklist böngészés és betöltés forgatógombbal, Loop felezés/duplázás.

### Mérföldkő #30: Build #309 / GitHub Run #44779958565 (Korg KAOSS DJ Teljes Gomb- és LED-Leképezés, Tracklist Navigáció & Loop Érintőcsík)
* **Verzió & Commit:** `697adcd`
* **SHA256:** `B284A4B4E68A66B69CB41EFACE60D9734CC37FA12B5225FE3AD22979787A92C5`
* **GitHub Run ID:** `44779958565`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 261 056 bájt)
* **Jellemzők:** 25 Hz-es kétirányú MIDI OUT LED visszajelzés (WinMM `midiOutShortMsg`), Tracklist léptetés és vizuális kurzor kijelölés, Loop érintőcsík (Halve Note 15, Toggle Note 16, Double Note 17), `install_plugin.bat` automatikus telepítő.

### Mérföldkő #31: Build #312 / GitHub Run #44805359543 (Korg KAOSS DJ SHIFT Modosító, Finom Jog Pozicionálás & Beatgrid Igazítás)
* **Verzió & Commit:** `44bca9a` / `d937838`
* **SHA256:** `0D6A26A27923F5AA93DF0F16F0F37C3DCFB45C43886D1950AE231C0907F8FDBE`
* **GitHub Run ID:** `44805359543`
* **Kész bináris mentés:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` és `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll` (1 264 640 bájt)
* **Jellemzők:**
  1. Fizikai SHIFT gomb kezelése (Note 26 Ch 8/9) és LED visszajelzése.
  2. Finom Jog pozicionálás SHIFT módban (8-szor finomabb léptetés: 15 ms/tick touch, 5 ms/tick nudge).
  3. Jog feletti érintőcsík kettős működése: Normál módban zenei nudge (±20 ms), SHIFT gombbal Beatgrid állítás (±10 ms) és középső SET 1 rácshorgonyzás `.vdj.json` mentéssel.
  4. Közvetlen egykattintásos telepítés.

---

# II. RÉSZ: KORÁBBI FEJLESZTÉSI EREDMÉNYEK ÉS RENDSZEREVOLÚCIÓ (BUILD #190 – #233)

### 1. Kétirányú GUI <-> Deck szinkronizáció
- A kezelőfelületen lévő HI, MID, LOW, FILTER csúszkák valueChanged eseményei rákötve a setRed(), setGreen(), setBlue(), setSaturation() hívásokra.
- A refreshDeck ciklus QSignalBlocker-rel és !isSliderDown() védelemmel lekérdezi és animálja a csúszkákat a MIDI potméterek forgatásakor.

### 2. Valós BPM kezelés, Tap Tempo és Ütemvonalak
- A WaveformWidget valós idejű digitális BPM kijelzést, TAP Tempo számlálót, +/- finomhangolást és gördülő beatgrid ütemvonalakat kapott.

### 3. Tempóállítás pozíció-megőrzéssel
- A sebesség csúszka mozgatásakor az aktuális lejátszási pozíció rögzítésre kerül, megelőzve a videó 0:00-ra ugrását.

### 4. Fejhallgató Előhallgatás (Headphone CUE / PFL)
- Külön CUE A és CUE B előhallgatási gombok, független Pre-Fade jelszint kezelés és fejhallgató hangerő / Cue-Master mix potméter.

### 5. OBS Aktív Kimenet Választó
- A felületről közvetlenül vezérelhető, hogy az OBS fő kimenetén a Master Mix, csak a Deck A vagy csak a Deck B jelenjen meg.

### 6. Valós BPM Analízis, Kick Transient Detektálás & Hullámforma
- Az AudioAnalyzer 44.1 kHz-es mono WMF dekódolással és 160 Hz-es lowpass szűrővel elkülöníti a lábdob energiát.
- 500 Hz-es Onset Novelty görbe és autokorreláció 3 pontos parabolikus interpolációval (0.05 BPM pontosság) és automatikus downbeat (Kick 1) ütemrács-igazítás.

### 7. OBS Videó Renderelés & 1080p GPU DJ Vizualizáció
- MIXER_EFFECT HLSL shader mul(v.pos, ViewProj) projekcióval.
- Valós videófájloknál dekódolt videó 4 átmeneti móddal (Linear Fade, Wipe, Additive, Cut).
- Tisztán audió fájloknál 1080p 60 FPS sebességű procedurális GPU DJ stúdió vizualizáció forgó vinyl lemezzel és frekvenciasávokkal.

### 8. Audió Útvonalválasztás és 3-sávos DSP Szűrés
- A privát dekóder forrás OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT módban dolgozik natív OBS audio filterrel.
- 3-sávos EQ és DJ Filter valós időben közvetlenül a mintákra fut le csatornánként.

### 9. Varratmentes Ütemhelyes Beat Loop
- Auto-Loop (1/2 – 32 ütem) bármely pillanatban azonnal aktiválható 40 ms-os debounce védelemmel.

### 10. Interaktív Ütemrács (Beatgrid) Csúsztatás és Duplaklikkes Reset
- Shift + Bal egérgombos húzás VAGY Jobb egérgombos húzás a hullámformán a Beatgrid eltolásához.
- MOVE szekció << és >> finomhangoló gombokkal (10 ms). Dupla kattintásos reset a potmétereken.

### 11. Teljes Számanalízis és Oldalkocsi Leíró Fájl (.vdj.json)
- Teljes számléptékű elemzés (korlát nélkül).
- Első betöltéskor menti a <fajl>.vdj.json gyorsítótárat (BPM, downbeat eltolás, CUE pont, csúcsok), későbbi betöltéskor 1 ms alatt betöltődik.

### 12. Kétirányú MIDI Clock & Központi Master Architektúra
- EXT SYNC: 0xF8 Tick és 24 PPQN számlálás DAW-hoz.
- SEND CLOCK: Független mikroszekundumos háttérszálon futó MIDI Clock generátor.

### 13. OBS Videó Vászon Renderelés és Akadozásmentes Lejátszás
- 100% Opak Alpha és direkt felülíró blend (GS_BLEND_ONE, GS_BLEND_ZERO).
- Gyermek forrás pumpálás (.video_tick).
- Seek debounce és inaktív audio deduplikáció védelem.

### 14. Moduláris Standalone VJ Deck Architektúra (Build #190)
- Független Deck koncepció (vj_deck OBS_SOURCE_VIDEO).
- Közvetlen vászon renderelés és saját QDialog ablak.

### 15. Valós Idejű Élő Tempó, Hullámforma és Hangkeverő Javítások (Build #192)
- Folyamatos gördülő hullámforma 50 ms-os frissítéssel, helyben szerkeszthető BPM mező, vonalszintű hangerő.

### 16. Audió Forwarding, Automatikus 1080p GPU Méretezés (Build #193)
- Automatikus 1080p GPU méretezés mátrix transzformációval (gs_matrix_scale3f).
- Pozícióbiztos kétfázisú tempóváltás, 32px magas overview sáv, állítható zoom.

### 17. Dinamikus Hullámforma Illesztés és Sáv-Hozzárendelés (Build #197)
- Dinamikus skálázás a teljes sávhosszon, [ ⛶ FIT 1080p ] gomb, 6 sávos kényszerített audió hozzárendelés.

### 18. GPU Frame Freeze, Deck A/B Routing és Integrált Tracklist Panel (Build #198)
- GPU Frame Freeze: az FFmpeg 0:00 képkockája soha nem villan fel tempóváltáskor.
- Tracklist Panel: Drag & drop import, RTX NVENC 1080p All-Intra konvertálás, perzisztens zöld pipák.

### 19. Automatikus Formátumfelismerés és NVENC Hibajavítás (Build #199)
- NVENC paraméter optimalizálás (-g 2 -bf 0 -cq 18), törlés megerősítéssel, egységes import csővezeték.

### 20. Teljes 2-Deck VJ Mixer Integráció, Pontos BPM & Kvantált Loop (Build #200)
- Három OBS forrás: vj_deck_a, vj_deck_b, vdj_master_mixer.
- Kétcsatornás vizuális és audió EQ (HI, MID, LOW, FILTER), beágyazott DECK_COLOR_EFFECT shader.

### 21. OBS Stúdió Mód T-Bar, 0.1 BPM Finomhangolás & Gapless Loop Híd (Build #207)
- Stúdió Mód jelenetátúsztató T-Bar vezérlés (obs_frontend_set_tbar_position 0-1023).
- Autoplay blokkolás, 0.10 BPM léptetők, memóriapufferes audió híd.

### 22. Crossfader B-Oldali Hangkiesés és EQ Recsegés Megszüntetése (Build #208)
- Biztosított folyamatos forrásláthatóság, exponenciális DJ filter csillapítás, dinamikus MIDI eszköznév kijelzés.

### 23. Crossfader Némulás Megszüntetése és Peak Max-Pooling Waveform (Build #209)
- Eltávolítva a kéretlen release_tbar, Snap BPM kerekítés, peak max-pooling borotvaéles kick tranziensekhez.

### 24. OBS Kilépési Crash Megszüntetése, RAM Audió Motor & Zero Autoplay (Build #210)
- Felszámolva a 0x38 Read AV crash (nullázott mutatók, védett destruktor).
- RAM alapú 44.1 kHz float sztereó PCM motor akadásmentes loophoz. Fix méretű alsó képernyő GUI (340 px magasság).

### 25. Valós Idejű Videókeverés Stúdió Mód Nélkül, Behringer XR18 Puffer Védelem (Build #211)
- Hivatalos átnevezés: obs_dj-vj_plugin.dll.
- Élő videókeverés Stúdió Mód nélkül közvetlenül a privát dekóder textúrákból.
- Behringer XR18 tiszta sztereó hang, képernyő 1/3 magasságú elrendezés (Deck A 640x340, Mixer 590x340, Deck B 640x340).

### 26. ViewProj Mátrix Fixálás, Ütemfázis Szinkron és DJ Jog Nudge (Build #213-#215)
- ViewProj vetítési mátrix korrekció Direct3D alatt.
- Ütemfázis (beat-phase) szinkronizáció, DJ Jog nudge lökdöső gombok, szigorú méretfixálás (QLayout::SetNoConstraint).

### 27. Hangkártya Automatikus Élesztése és 4 Órás Sávhossz Támogatás (Build #216-#217)
- Indításkori WASAPI/audio monitor ébresztés, 4 órás (14 400 mp) kiterjesztett sávhossz, 12-fokozatú Auto-Loop mátrix (1/16 .. 128).

### 28. Robusztus NVENC/CPU Konverter és Közvetlen WASAPI OUT 3-4 CUE (Build #218-#219)
- WasapiOutput alrendszer közvetlen füles előhallgatáshoz OUT 3-4 kimenetre az OBS busz megkerülésével.
- Dupla jel és fáziskioltás megszüntetése, tiszta OBS kilépés.

### 29. Valódi Pre-Fade CUE & PGM Mix, 4 Hot Cue, VOL FX és XFADE FX (Build #220)
- Pre-Fade füles keverés CUE/PGM aránnyal, 4 db kattintható és színkódolt Hot Cue gomb deckenként, Csatorna VOL FX és Master XFADE FX keretrendszer.

### 30. Lejátszás Alatti Számbetöltés Védelem & 12 VJ Crossfader Shader (Build #221)
- Playback Load Protection: forgó deckre tilos számot tölteni.
- 12 Crossfader shader (Swirl, Fisheye, RGB Split, Zoom Blur, Pixelate, Luma Dissolve, Ripple, Kaleidoscope, Glitch, Scanline, Halftone, Neon Sobel).

### 31. Munkamenet-alapú Zöld Pipák & Deck-Színek (Build #222 - Kiemelt Bázis Mérföldkő)
- Tracklist lejátszási pipák csak az aktuális szettre érvényesek ([ ↺ ÚJ SZETT ] gomb).
- Deck-színű felületek: Deck A kék (#00d2ff), Deck B meleg borostyán/narancs (#ff8800).

### 32. Interaktív Jog Scratch, 3-Sávos Spektrum és FIT/FILL Képméretezés (Build #223)
- Gördülő hullámforma egérrel húzható jog scratch funkcióval.
- 3-sávos spektrális színezés (Low kék, Mid arany, High cián, Kick fehér csúcsok).
- 1080p FIT / FILL azonnali vászonméretezési módok.

### 33. Mojovideotech FluidFlow GPU VJ Shader és 8 ISF Visualizer Suite (Build #224-#233)
- Audio-reaktív procedurális shader tiszta audióhoz (.mp3, .wav).
- 8 ISF visualizer shader (Fluid Flow, Truchet Spin, Hypno Worm, Shape Twist, String Art, Dodecahedron, Kali Circuits, Diamond Vision).
- Paraméterenkénti zenei moduláció (Beat Pulse, Spectral Tone, Both) és .vdj.json perzisztencia.
