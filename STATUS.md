# AKTUÁLIS FEJLESZTÉSI STÁTUSZ & SPRINT (STATUS)

* **Legutolsó stabil mérföldkő:** Build #353 (GitHub Run #45042690877)
* **Aktuális Git Commit:** `d7e5d71`
* **Legutolsó telepített bináris:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 380 352 bájt)
* **Bináris SHA256:** `0EEAC4ACD3F4479455EC86436510C8FEB7D7044ACB6C537D620EF848F35AD8F6`
* **Kész bináris másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

---

## 1. Jelenlegi Fókusz / Sprint
* **Fő cél:** Build #353 Tracklist 3-Állapotú Oszloprendezés & Stabilitási Tesztek OBS Studio-ban:
  1. **3-Állapotú Oszloprendezés:**
     - Fejléc oszlopra kattintva: 1. klikk = Növekvő ($\blacktriangle$), 2. klikk = Csökkenő ($\blacktriangledown$), 3. klikk = Visszaállítás a listába illesztés (import) ideje szerinti eredeti sorrendre.
     - ST (státusz), CÍM (ABC A-Z), BPM (számszerű), KEY (Camelot skála), IDŐ (hossz), FORM (azonos formátumok: audio / 1080p / konvertálatlan).
     - A rendezés szálbiztos és virtuális (`filtered_track_indices_`), az eredeti gyűjtemény sorrendje és a JSON fájl nem módosul.
  2. **Nyers Videó Relink & Önjavítás (Build #351):**
     - Ellenőrizni az auto-konvertálást és az akadásmentes lejátszást az OBS Studio-ban.

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #353)
* [x] **Tracklist 3-Állapotú Oszloprendezés (Build #353):**
  - **3-állapotú ciklikus rendezés:**
    - 1. kattintás: Növekvő rendezés (Ascending Order, $\blacktriangle$).
    - 2. kattintás: Csökkenő rendezés (Descending Order, $\blacktriangledown$).
    - 3. kattintás: Visszaállítás az importálás/beillesztés sorrendjére (eredeti sorrend, indikátor eltűnik).
  - **Oszloponkénti logika:**
    - `CÍM / NÉV`: ékezetérzékeny magyar ABC sorrend (`QString::localeAwareCompare`).
    - `ST`: Státuszok csoportosítása (Deck A -> Deck B -> ⚠️ Hiányzó fájlok -> Lejátszott -> Kész -> Várakozik).
    - `BPM`: Lebegőpontos számszerű sorrend.
    - `KEY`: Camelot skála (1A..12B).
    - `IDŐ`: Zeneszámok hossza (ms szerint).
    - `FORM`: Azonos formátumok egymás mellé (Audio -> VJ 1080p -> Konvertálatlan).
    - `LOAD`: Fix gombok, a fejléc nem rendezhető.
  - **Telepített bináris:** 1 380 352 bájt, SHA256 `0EEAC4ACD3F4479455EC86436510C8FEB7D7044ACB6C537D620EF848F35AD8F6`.
* [x] **Nyers Videó Relink Védelem, Auto-Konvertálás & Tick Monitoring Optimalizálás (Build #351):**
  - **Fagyási ok feltárása:** A `B166ER - Vp32 (Sequence 1)` egy 2 GB-os, 2560×1440 (2K QHD), 31 Mbps Long-GOP videó volt, ami a relinkelés után nem konvertálódott 1080p All-Intrára. A valós idejű dekódolás túlterhelte az OBS audió alrendszerét (741 ms buffer lag), a frame-enkénti monitoring type reset pedig szál-holtpontot és fagyást idézett elő.
  - **Relink Auto-Konvertálás:** Ha a felhasználó egy olyan videót tallóz be újrakereséskor, aminek nincs még kész 1080p változata a VJ könyvtárban, a rendszer automatikusan sorba állítja és elindítja az RTX NVENC 1080p All-Intra konvertálást.
  - **Deck Betöltési Védelem (`loadTrackToDeck`):** Nyers, nem konvertált videó nem tölthető be közvetlenül a forgó deckbe; a rendszer figyelmeztetést ad és azonnal elindítja a konvertálást.
  - **Önjavítás (`loadLibraryJson`):** A könyvtár betöltésekor automatikusan korrigálja azokat a videókat, amelyek tévesen késznek voltak jelölve, de a cél `_vj1080.mp4` fájljuk hiányzott a lemezről.
  - **Tick Monitoring Optimalizálás (`deck_source.cpp`):** A `deck_video_tick` ciklusból eltávolításra került a folyamatos, másodpercenként 60-szor feleslegesen lefutó `obs_source_set_monitoring_type` és `obs_source_set_audio_mixers` hívás.
  - **Telepített bináris:** 1 366 528 bájt, SHA256 `50B6E954E957B4CAC4E4F9A74FE31ED3AA27C321CEF4B499E07FB01660F58D74`.
* [x] **Tracklist Lejátszási Listák, 4 Gyorsgomb, Élő Kereső & Fájl Relink (Build #349):**
  - **4 Gyorsgomb Sáv (Quick Slots):**
    - Slot 1: `[ 🎵 MIND ]` fixen a teljes könyvtárra vált.
    - Slot 2, 3, 4: Split gomb elrendezés (bal oldal: tiszta lista név gyorsváltáshoz; jobb oldal: `[ ▾ ]` mini legördülő menü lista kiválasztásához, új lista készítéséhez, szerkesztéshez vagy törléshez).
    - `[ ➕ ]` gomb: dedikált `PlaylistDialog` lista név, műfaj és okos szűrő kulcsszó megadásához.
    - `🔍 Keresés...`: azonnali élő szűrés a címre, útvonalra, BPM-re vagy Camelot hangnemre.
  - **Kettős Törlési Védelem:**
    - Egyedi lejátszási listában a törlés kizárólag az adott listából veszi ki a dalt, a globális médiatárban megőrzi.
    - Teljes könyvtárban (`MIND`) történő törléskor a meglévő megerősítő párbeszédablak jelenik meg, és a lemezről is törli a gyorsítótárat.
  - **Hiányzó Fájlok Észlelése & Újrakeresése (Relink):**
    - A `ST` oszlopban sárga `⚠️` jelzés mutatja a lemezről hiányzó / áthelyezett fájlokat.
    - Dupla kattintással vagy jobb klikkes helyi menüvel (`📁 Fájl újrakeresése / elérési útvonal megadása...`) tallózással azonnal javítható az útvonal.
  - **Moduláris Architektúra & JSON Perzisztencia:**
    - Új `src/gui/playlist_manager.hpp/.cpp` és `src/gui/playlist_dialog.hpp/.cpp` modulok.
    - A `vj_tracklist.json` visszafelé kompatibilis módon tárolja a trackeket, a lejátszási listákat és a 4 slot kiosztását.
  - **Ablakméret & Geometria:**
    - 640 × 560 px Tracklist méret: 56 px biztonsági légrés a Deck B felett, teljes átfedés-mentesség.
  - **Telepített bináris:** 1 362 432 bájt, SHA256 `303F2B39406588559A46E3726A1520FBD42A6A8E4DFDF96F98580613A7CF41E8`.
* [x] **Csatorna Hangerő Fader Alakzat Áttűnések & Crossfader Keverés Javítása (Build #346):**
  - **Hiba elhárítása (`mixer_source.cpp`):** Eltávolításra került a korábbi hibás `lerp(b.rgb, a.rgb, m_a)` és `lerp(a.rgb, b.rgb, m_b)` shader ág, amely a crossfadert figyelmen kívül hagyva 100%-os tömörséggel kényszerítette rá a másik deck videóját a maszkolt felületekre.
  - **Valódi Transzparencia:** Ha csak az egyik decken aktív az alakzat-áttűnés (Kör, Lamellák, Téglalap, Négyzet, Rombusz, Zoom), a maszkolt részeken a háttér tiszta fekete (átlátszó), a másik deck csak akkor és olyan mértékben jelenik meg alatta, amennyire a crossfader át van húzva.
  - **Finom Videókeverés (Lamellák / Csíkozódás):** A vízszintes és függőleges lamelláknál megszűnt a másik deck hirtelen, 100%-os átlátszatlan beugrása: a sávokban a crossfader súlyozásának megfelelő arányos, lágy videókeverés érvényesül.
  - **Luma Dissolve Küszöb Javítás:** A Luma Dissolve átmenet skálázása fel lett készítve a teljes tartományra (`thresh = lerp(-width, 1.0 + width, crossfade)`), így 0.0-s crossfader állásnál nem szivárog be 50%-os Deck B.
  - **Telepített bináris:** 1 308 160 bájt, SHA256 `6A9BFB11596C5EA59E8820B1D595A2C4EECAAE7651CAB03753DBC5CE75027051`.
* [x] **VJ Shader Láthatóság, Valós DSP VU Műszer, Deck Sidecar & Késleltetett Master FX (Build #344):**
  - **Üres és Lejárt Deck Tiszta Fekete:** A `deck_source.cpp` és `mixer_source.cpp` renderelőjében szigorítva lett a feltétel: `deck->hasAudioOnlyTrack() && deck->isPlaying() && !deck->isAtEnd()`. Üres decken vagy befejeződött számnál a shader nem renderelődik, a textúra tiszta fekete marad és nem szivárog be a master kimenetre.
  - **Valós DSP VU Kivezérlésjelző:** A `MixerChannelStrip` korábbi szimulált (`rand() % 10`) logikája törölve lett. A szintjelző kizárólag valós PCM RMS amplitúdót (`currentAmplitude()`) jelenít meg a csatorna hangerő és Gain szorzataként. Üres decken Play gombbal is stabil 0 szinten marad.
  - **Deck-oldalankénti Shader Perzisztencia:** A `.vdj.json` oldalkocsi fájl kiegészült `vj_shader_deck_a` és `vj_shader_deck_b` objektumokkal. Mindkét deck függetlenül menti és tölti be az adott trackhez tartozó shader indexet és potméter-értékeket, megőrizve a visszafelé kompatibilis `vj_shader` mezőt.
  - **Késleltetett Master FX Prioritás & Kézrátétel:** A Crossfade FX és Dual LFO beállítások a korábban betöltött trackhez tartoznak prioritásban. Amikor a prioritásos zene véget ér, a rendszer azonnali rántás helyett várakozó állapotba (`pending_fx_handover_ready_`) lép. A bent maradó track mentett master FX beállításai csak akkor lépnek életbe, amikor a felhasználó shader- vagy Crossfade FX váltást kér.
  - **Telepített bináris:** 1 309 184 bájt, SHA256 `F88DCEDFCA3F462D35DED6A3196AE3EBDEC65311842B0095ABD8CB29630ABC52`.
* [x] **VJ Shader Fordítási Hiba Elhárítása & Libobs Kompatibilitás (Build #340):**
  - **Hiba elhárítása:** Az OBS `libobs` shader elemzője (`cf-parser.c`) az `#define iFrame ((int)(anim_time * 60.0))` makró zárójelezése miatt szintaktikai hibát dobott (`fluid_flow_effect (25, 17): Expected identifier`), emiatt sem a 15 beépített VJ shader, sem a custom shaderek nem jelentek meg.
  - **Függvény túlterhelés megszüntetése:** A libobs `effect-parser.c` szimbólumtáblája nem engedélyezi a függvények túlterhelését (`'mod' already exists`); a túlterhelt `mod()` függvények helyett visszaálltak a szabványos, egyedi azonosítójú `glsl_mod`, `glsl_mod2`, `glsl_mod2s`, `glsl_mod3`, `glsl_mod3s`, `glsl_mod4`, `glsl_mod4s` segédfüggvények.
  - **Törölt érvénytelen makrók:** A nem támogatott `#define texture(...)` makrók eltávolításra kerültek; a procedurális zajmintavételezés dedikált `sample_procedural_noise(float2 p)` HLSL függvénnyel működik.
  - **C++ ShaderToy Transzpiler:** Minden ShaderToy futtatási token (`iFrame`, `iTimeDelta`, `iMouse`, `iDate`, `iChannelResolution`, `texture`, `texture2D`, `textureLod`, `mod`) kezelése közvetlenül a C++ oldali regex transzpilerbe került át.
  - **Telepített bináris:** 1 290 240 bájt, SHA256 `0B1EEEBD96BA73877DCB8099179552D21615337106D07CA4D2E3664DFD484F60`.
* [x] **BPM-Szinkronizált Áttűnések & Közvetlen Shader Beágyazó Motor (Build #327 – #339):**
  - **BPM-Szinkronizált Lágy Áttűnések (Crossfade FX & VJ Shader lista váltáskor):**
    - Az effektek és shaderek közötti váltáskor a korábbi hard cut helyett a zenei tempóhoz (BPM) igazított, konfigurálható hosszúságú (Azonnali / 1 / 2 / 4 / 8 ütemes) lágy áttűnés valósult meg ($T = \frac{60}{\text{BPM}} \times \text{beats}$).
    - `DeckPair` és `DeckPlayer` állapotgép valós idejű órajel-vezérelt progressz követéssel ($0.0 \rightarrow 1.0$).
    - Direct3D 11 HLSL zéró nyugalmi GPU terhelés: nyugalmi állapotban (`t >= 0.999`) kizárólag az aktív effekt fut, az áttűnési blend ág 0% extra GPU-t fogyaszt.
    - Új Áttűnés (BPM Beat) választó UI vezérlők a Crossfade FX dialógusban és a VJ Shader ablakban.
  - **Közvetlen ShaderToy / GLSL / ISF Beágyazó Motor (CustomShaderManager & IsfConverterDialog):**
    - Megszűnt a kötelező külső AI prompt és címkekényszer (`[NAME]`, `[PARAMS]`, `[CODE]`): a vágólapról közvetlenül beilleszthető bármilyen ShaderToy vagy ISF shader.
    - Automatikus ShaderToy / GLSL felismerés (`mainImage`, `vec4`, `vec3`, `gl_FragColor`, `iResolution`, `fragColor`).
    - Natív túlterhelt HLSL `mod()` függvények minden vektortípusra (`float2`, `float3`, `float4`), kiküszöbölve a törékeny szöveges típusfelismerést.
    - ShaderToy futási környezet emuláció (`iResolution`, `iTime`, `iMouse`, `iDate`, `iFrame`, `sample_procedural_noise` procedurális zajtextúra).
    - 1-kattintásos beillesztés, tesztelés és azonnali bináris beágyazás.
  - **VJ Shader Láthatóság Javítása (Üres és Audió Decken):**
    - Megszüntetve a szigorú `hasAudioOnlyTrack()` feltétel: a shader mostantól zene betöltése nélkül is azonnal látható és animálódik, így a felhasználó azonnal tesztelheti a beépített és beágyazott shadereket.
* [x] **Ablakpozíciók Perzisztenciája & Beállítások Vezérlők (Build #325):**
  - Mind az 5 ablak (`DeckWindow A`, `DeckWindow B`, `MixerWindow`, `TracklistDock`, `VjFxDialog`) mozgatásakor az aktuális koordináták rögzítésre kerülnek a konfigurációban.
  - A Beállítások menü `🌐 Nyelv & Rendszer` fülén a felhasználó a `[ 📍 Jelenlegi Ablakpozíciók Mentése ]` gombbal közvetlenül elmentheti az általa beállított képernyőelrendezést.
  - A `[ ↺ Alapértelmezett 5-Ablakos Layout ]` gombbal bármikor visszaállítható a tiszta referencia-elrendezés.
  - OBS bezárásakor vagy modul unloadkor az ablakpozíciók automatikusan beíródnak az `obs_vj_settings.json` fájlba.
* [x] **Hullámforma Arányok & Szubpixel Simítás (Build #325):**
  - **Arányok:** A részletes futó hullámforma 83 px, az alsó áttekintő sáv 50 px, a teljes canvas magassága 135 px.
  - A DeckWindow ablakmagasság 410 px-ről **370 px**-re karcsúsodott, így sosem ér le a tálca alá és ~86 px tiszta légrés marad a felső panelek alatt.
  - **Anti-pixelation:** A diszkrét lépcsős mintavételezést folyamatos tört-indexű lineáris interpoláció (`lerp`) váltotta fel, 3-pontos oszlopsimítással és szubpixel anti-aliasinggal.
* [x] **Ablakméretek & Pozíciók Geometriai Korrekciója (Build #324):**
  - **OBS menüsor láthatóság:** A felső panelek (`VideoFxDialog` és `TracklistDock`) koordinátája `y = geom.top() + 38`-ra lett állítva, így az OBS Studio felső menüsora (Fájl, Szerkesztés, Nézet stb.) 100%-ban szabadon látható és kattintható.
  - **Tálca alácsúszás megszüntetése:** A Deck (`640 × 410 px`) és a Mixer (`590 × 340 px`) pozíciója közvetlenül a képernyő munkaterületének aljához (`geom.bottom() - target_h - 38`) igazodik, így a fader- és bend-gombok pontosan a Windows tálca felett ülnek.
  - **Összecsúszás felszámolása:** A felső ablakok magassága fix `500 px`-re, a Deck magassága `410 px`-re lett rögzítve, így a felső panelek és az alsó deckek között tiszta, ~11 px-es légrés alakult ki teljes átfedés-mentességgel.
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
* [x] Változtatások commitolása és pusholása a távoli GitHub repóba (Build #326, Commit `92842a5`).
* [x] GitHub Actions CI fordítás sikeres lefutása (Run #44961894749), kész DLL letöltése és telepítése (SHA256: `3E775DE6`).
* [ ] Éles tesztelés az OBS Studio elindításával:
  - Mind az 5 ablak egyszerre megnyílik a referencia pozíciókban (`x=10, 660, 1255`, `y=654` alul; `x=10, 1255`, `y=38` felül).
  - DeckWindow magasság szigorúan 340 px, MixerWindow 340 px, Waveform 105 px (65 px futó + 38 px alsó áttekintő).
  - Beállítások menü pozíció mentés (`[ 📍 Jelenlegi Ablakpozíciók Mentése ]`) és visszaállítás (`[ ↺ Alapértelmezett 5-Ablakos Layout ]`) ellenőrzése.

---

## 4. Ismert Elakadások / Figyelmeztetések
* **OBS DLL zárolás:** Futó OBS Studio mellett a DLL nem írható felül; frissítéskor az OBS-nek zárva kell lennie.
