# AKTUÁLIS FEJLESZTÉSI STÁTUSZ & SPRINT (STATUS)

* **Legutolsó stabil mérföldkő:** Build #384 (GitHub Run #45867209383 / Run #421 - Telepítve és igazoltan működik!)
* **Aktuális Git Commit:** `0e3c7c1`
* **Telepített bináris:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll` (1 472 000 bájt, 2026. 11. 26. 18:27)
* **Bináris SHA256:** `FF5F5A792ED63FAF4A87816FE1618C67CF53CCB46358771374620348E24A193A`
* **Kész bináris másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

---

## 1. Jelenlegi Fókusz / Sprint
* **Fő cél:** Build #384 - Zenei Reaktivitás (Beat Sensitivity) Vezérlés, Zenei Alapértelmezések és 9 Mixer FX Élő Audió Bekötése:
  1. **Állítható Master "Beat Érzékenység" (Audio Reactivity):**
     - Új csúszka a VJ FX ablakban (0% .. 200%, alapértelmezett 100%).
     - Szabályozza az összes procedurális shader dob- és dinamika-érzékenységét (`eff_bass`, `eff_mid`, `eff_high`, `eff_amp`).
     - Valós idejű szálbiztos atomi átadás és perzisztens `.vdj.json` mentés/betöltés (`audio_reactivity`).
  2. **Zenei Alapértelmezések mind a 15 procedurális VJ shaderhez:**
     - Az eddig csendes `mod_mode = 0` (OFF) helyett mind a 15 beépített shader kulcsparaméterei (méret, pulzus, fénylés, robbanás, pupilla, stb.) automatikusan dinamikus zenei modulációt kaptak (Bass Pulse +40..60%, Mid Pulse +35%, Macro-Tonus +30..35%).
  3. **9 Traktor Mixer FX Élő Audió Bekötése (Visual EQ 100% érintetlenül hagyásával):**
     - Az összes Traktor Mixer FX (Filter, Delay, Reverb, Flanger, Beatmasher, Turntable, Ramp Delay, Mulholland Drive, Transpose) megkapja az élő zenei spektrumot (`bass_energy`, `mid_energy`, `high_energy`, `audio_amp`).
     - A Beatmasher szaggatása és ugrása, a Delay és Reverb tágulása a lábdobokra és ütemekre lüktet falióra-idő helyett.
     - A Visual EQ korrekciós lánc (RGB erősítések, szaturáció, csatorna potméterek) szigorúan érintetlen maradt a felhasználói direktíva szerint.
  4. **Master Crossfade FX Moduláció Javítása:**
     - A korábban 0.0f-ra kódolt `mod_depth` hiba elhárítva, az intenzitás és ráta modulációk azonnal reagálnak az aktív módokra.

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #383)
* [x] **Tiszta BaseName_vj1080.mp4 Videókonverter & Duplikációmentes Könyvtár (Build #383):**
  - Megszüntetve a `BaseName_[hash]_vj1080.mp4` elnevezés és a `qHash` generálás.
  - Köztes `.tmp.mp4` fájlírás eltávolítva; közvetlen célfájlba kódolás.
  - Telepített bináris: 1 462 784 bájt, SHA256 `BAD5E470E543FCA815F7C00F78603B59BFCA8A31F63B94B3094001FB72BD67D8`.
* [x] **Tiszta Crossfader Dominancia & Fokozatos Box Iris (Build #378):**
  - Crossfader végállás szivárgásmentességének helyreállítása.
  - 16:9 téglalap hirtelen ugrásának megszüntetése, sima lineáris szűkülés.
  - Telepített bináris: 1 462 272 bájt, SHA256 `27A555F57A4B764B85B26BAE66132ACB52B6DC6DA08496B6A461BB0B8D7C7C24`.
* [x] **Zero-Pop Live Crossfade FX Váltás (Build #377):**
  - Hangerő potméter alakzat-kivágások (Iris Circle, Venetian Stripes, Vertical Slices, Zoom Out) valós átlátszósága a másik deck felé.
  - Előző crossfade FX paraméterek megőrzése (`cf_prev_p0..7`), azonnali állapotugrás és torzulás megszüntetése FX váltáskor.
  - Telepített bináris: 1 462 784 bájt, SHA256 `A9EFA0FFADAD44E4C33073A91858C6282561B33B5CD5D8BBFCF3F00F9BEC7C8A`.
* [x] **Zenei Vizuál Reaktivitás & Belső Forrás Ütemezés Helyreállítása (Build #376):**
  - `deck_enum_active_sources` visszaállítva a `deck_source.cpp`-ben.
  - Videó képkockák folyamatos dekódolása és időkód-léptetése garantálva.
  - Kick transient, spektrális envelope és zenei mozgató algoritmusok felélesztve.
* [x] **Steinberg ASIO Host Motor & Párhuzamos WASAPI Támogatás (Build #375):**
  - Valódi ASIO 2.3 driver betöltés COM CLSID alapján registryből.
  - SPSC lockless ringpufferek, 256 mintás dupla-puffer kezelés.
  - Hardveres ASIO Vezérlőpult gomb a Beállítások ablakban.
  - WASAPI Direct CUE (OUT 3-4) és ASIO szabadon választható békés koegzisztenciája.
* [x] **Korg KAOSS DJ Deck-oldalankénti Shift + TAP Reset & Globális Kétoldali Nullázás (Build #374):**
  - Shift A + TAP: Csak a Deck A effektjének azonnali visszaállítása.
  - Shift B + TAP: Csak a Deck B effektjének azonnali visszaállítása.
  - Sima TAP (HOLD OFF): Mindkét oldal egyidejű azonnali visszaállítása alapállapotba (0.0).
  - Inaktív deck felügyelet kikapcsolt HOLD esetén (500 ms idle decay).
* [x] **Korg KAOSS DJ Touchpad Fagyás Megszüntetése & Tiszta Note 11 Feedback (Build #373):**
  - Note 34 és Note 12/13 kiküldés törölve a visszajelzésből (megszűnt a hardver Sampler módba kényszerítése és a Touchpad busz zavarása).
  - Kizárólag a hivatalos Note 11 (TAP gomb) kap kétirányú LED vezérlést.
  - Slot-váltás korai szűrése a Touchpad folytonos húzásánál.
  - Sampler padok (Notes 74..77) kezelése a 4 sarok-effektre.
* [x] **Korg KAOSS DJ Note 11 (TAP) HOLD & Hardveres LED Visszajelzés (Build #372):**
  - Note 11 (TAP gomb) dedikálása Kaoss HOLD funkcióra.
  - Kétirányú MIDI LED visszajelzés Note 11-re (Ch 7 és Ch 1).
  - Unmapped MIDI események naplózása OBS logba.
  - Telepített bináris: 1 444 864 bájt, SHA256 `B884D22C9890B4CB76CF9BFCE4A57F430A1D41E6B49C61C4B2E6F34287425F9A`.
* [x] **Korg KAOSS DJ Touchpad Fast-Path, Univerzális Bipoláris FX & HOLD Vezérlés (Build #371):**
  - Közvetlen fast-path intercept CC 12/13/31/32 és Hold gombokra.
  - Mind a 4 slot bipoláris kétirányú vezérlése (LPF/HPF).
  - 500 ms auto-reset watchdog és szigorú HOLD pozíciózár.
  - Telepített bináris: 1 444 864 bájt, SHA256 `31837C23E0F330BDAE5BA69C122E04A49DA89918ABD4C29F7E501E0AF582F3DE`.
* [x] **Korg KAOSS DJ 2×4 Touchpad FX Mátrix, Enkóder & HOLD Watchdog (Build #370):**
  - 2 oszlop × 4 soros Touchpad mátrix (Deck A bal oszlop, Deck B jobb oszlop, Slot 0..3 vertikális sorok).
  - Érintés-szinkronizáció és race-condition védelem (`touch_pending_` / `touch_triggered_lock_`).
  - Program/Value relatív enkóder bipoláris snap-to-center és egyirányú FX vezérléssel.
  - 200 ms watchdog timer exponenciális lerp visszahúzással.
  - HOLD gomb állapotgép és kétirányú MIDI OUT LED visszajelzés.
  - Telepített bináris: 1 443 328 bájt, SHA256 `86653BFDA91FD27A019C2CEDBA672BFD65F035468D80237321E3ACEEEDC59ED7`.
* [x] **Univerzális Bipoláris Tónus, Delay vs. Reverb & Intenzív Beatmasher (Build #369):**
  - Bipoláris tónuslánc minden effektre (balra: sötétedés + fakulás, jobbra: kifehéredés + szaturáció).
  - Delay (ritmikus multi-tap echo másolatok) és Reverb (diffúz fényköd és spatial bloom) radikális szétválasztása.
  - Beatmasher 2 intenzitás követés (1/4 .. 1/32 darálás, brutális sáv-eltolás, stroboszkóp).
  - Telepített bináris: 1 437 696 bájt, SHA256 `9238A91092DE1CEBBD0EFB8A988414FD6655314D774E87AE65357880C763733C`.
* [x] **libobs HLSL Paraméter Hiba & Végtelen Újrafordítási Hurok Javítása (Build #368):**
  - Eltávolítva a `texture2d` paraméter a 9 shaderből, közvetlen `image.Sample` mintavételezés.
  - Fail-safe guard a 60 FPS D3D fordítási ciklus ellen.
  - Telepített bináris: 1 431 040 bájt, SHA256 `581B19A1E65A4B7A6A295B5FC4154E6D54DBEAFB647A34EE0A48AF8DAE442947`.
* [x] **2-Lépcsős Post-FX Renderelési Architektúra & Reverb Unroll (Build #367):**
  - Dekomponált 2-lépcsős renderelés: Deck Color/Mixer FX független passzban fut, a Master Mixer nem duplikálja a 9 effekt shadereit.
  - Reverb unroll, azonnali D3D11 HLSL fordítás, 0 ms indulási akadás.
  - Telepített bináris: 1 431 552 bájt, SHA256 `2FB5957020656BE86892575B21532048F18C6FDD856BA2D370C0CB4F5A2A1C8B`.
* [x] **9 Traktor Mixer FX HLSL Képtorzítás & Basszus-Vezérelt Visual EQ (Build #366):**
  - Mind a 9 audio effektnek megfelelő vizuális shader: Filter, Delay, Reverb, Flanger, Beatmasher 2, Turntable, Ramp Delay, Mulholland Drive, Transpose Stretch.
  - Low EQ-hoz kötött szaturáció (mélyvágás = szürkeárnyalat, basszus = élénk színek).
  - Korai kilépés középállásban (0% GPU felár).
  - Telepített bináris: 1 430 528 bájt, SHA256 `8D5626FCFB9B3B0DD60D326D8FABE78396913B8A74F4C423F84D39C34BABB01B`.
* [x] **Zéró Holtsáv, Mind a 9 Effekt Bipoláris Szűrése, 2-Pólusú 300 Hz Ramp Delay HPF & Agresszív Kezdeti Felfutás (Build #365):**
  - Perceptuális HPF/LPF görbék: 10%-nál 135 Hz / 3580 Hz azonnali zenei vágás.
  - Bipoláris filter bevezetve Beatmasher és Turntable effektekre is (mind a 9 effekt szűrt).
  - $x^{0.62}$ görbe: 10% poti = 24% effektmélység, zéró holtsáv.
  - Beatmasher azonnali 1/4 beat indítás és átszóló mix.
  - Turntable közvetlen átúszás a lemezre (nincs szellemzene a lassulás alatt).
  - Ramp Delay 2-pólusú 300 Hz HPF: megszüntette a mély Doppler recsegést.
  - Telepített bináris: 1 416 192 bájt, SHA256 `ECE4F2C41A7FA5DF7EEC818D83851551117899BAFF89B4F3FD131334111035A3`.

---

## 2. Legutóbb Elkészült Funkciók (Build #313 – #365)
* [x] **Zéró Holtsáv, Mind a 9 Effekt Bipoláris Szűrése, 2-Pólusú 300 Hz Ramp Delay HPF & Agresszív Kezdeti Felfutás (Build #365):**
  - Perceptuális HPF/LPF görbék: 10%-nál 135 Hz / 3580 Hz azonnali zenei vágás.
  - Bipoláris filter bevezetve Beatmasher és Turntable effektekre is (mind a 9 effekt szűrt).
  - $x^{0.62}$ görbe: 10% poti = 24% effektmélység, zéró holtsáv.
  - Beatmasher azonnali 1/4 beat indítás és átszóló mix.
  - Turntable közvetlen átúszás a lemezre (nincs szellemzene a lassulás alatt).
  - Ramp Delay 2-pólusú 300 Hz HPF: megszüntette a mély Doppler recsegést.
  - Telepített bináris: 1 416 192 bájt, SHA256 `ECE4F2C41A7FA5DF7EEC818D83851551117899BAFF89B4F3FD131334111035A3`.
* [x] **Folytonos Turntable Tehetetlenség, Ramp Delay Recsegésmentesítés, Mulholland Drive Boost, Transpose Iránykorrekció & Agresszív Kezdeti Görbe (Build #364):**
  - Turntable: Folytonos sebességkövetés és tehetetlenség (Brake: $1.0\times \to 0.0\times$, Spinback: $1.0\times \to -3.5\times$), zéró kapcsoló jelleg, zéró DC zúgás.
  - Ramp Delay: 180 Hz send/feedback HPF (megszűnt a mély recsegés), UP = gyorsuló sziréna, DOWN = lassuló Doppler drop.
  - Mulholland: 6.5x csöves overdrive, kompenzált kimeneti szint a stabil RMS hangosságért.
  - Transpose: Előjelhiba javítva (UP = pitch up + HPF, DOWN = pitch down + LPF), feliratokkal egyező működés.
  - Felfutás: $x^{0.70}$ görbe mind a 8 effektre (10% poti = 20% effekt).
  - Telepített bináris: 1 416 704 bájt, SHA256 `AD095240DD17E9E1B63A4C4A658567B810425974A744C37E9F5F8AA568276BB2`.
* [x] **7 Mixer FX Akusztikai & Algoritmikus Finomhangolás (Build #363):**
  - HPF: 7200 Hz cutoff +3.5 dB Sizzle Sheen boosttal (tiszta cicergés).
  - Turntable: Fizikai bakelit motorlassulás 0-ra és koszinuszos stop némítás (Brake) + exponenciális lecsengésű Spinback, némított futó száraz jel.
  - Reverb: 12-beat RT60 lecsengés, 0.978 feedback limit, csökkentett damp (fényes lemez karakter).
  - Beatmasher: 8-fokozatú zenei leosztás (1/2 .. 1/32) holtsávok nélkül, 48-mintás Hann crossfade, full-bandwidth bemenet.
  - Ramp Delay: Barber-Pole kettős tap Doppler motor (180° eltolással, Hann ablakokkal): zéró kattogás, zéró pattogás.
  - Mulholland: 4.5x analóg csöves overdrive aszimmetrikus telítéssel és RMS normalizálóval (erős effekt, stabil hangerő).
  - Transpose: Teljes sávszélességű audió routing, constant-power crossfade, zéró némulás magas állásban.
  - Telepített bináris: 1 416 704 bájt, SHA256 `4AA0ECA6998F22C6EA60281D076F577FCEF65AF1C799016B319139B71C9A49A8`.
  - HPF: 7200 Hz cutoff +3.5 dB Sizzle Sheen boosttal (tiszta cicergés).
  - Turntable: Fizikai bakelit motorlassulás 0-ra és koszinuszos stop némítás (Brake) + exponenciális lecsengésű Spinback, némított futó száraz jel.
  - Reverb: 12-beat RT60 lecsengés, 0.978 feedback limit, csökkentett damp (fényes lemez karakter).
  - Beatmasher: 8-fokozatú zenei leosztás (1/2 .. 1/32) holtsávok nélkül, 48-mintás Hann crossfade, full-bandwidth bemenet.
  - Ramp Delay: Barber-Pole kettős tap Doppler motor (180° eltolással, Hann ablakokkal): zéró kattogás, zéró pattogás.
  - Mulholland: 4.5x analóg csöves overdrive aszimmetrikus telítéssel és RMS normalizálóval (erős effekt, stabil hangerő).
  - Transpose: Teljes sávszélességű audió routing, constant-power crossfade, zéró némulás magas állásban.
  - Telepített bináris: 1 416 704 bájt, SHA256 `4AA0ECA6998F22C6EA60281D076F577FCEF65AF1C799016B319139B71C9A49A8`.
* [x] **Csatornánkénti Független Függőleges 4-Slot Mixer FX & Recsegésmentes TPT Szűrő (Build #362):**
  - Deck A és Deck B független 4-slot konfiguráció és állapotgép.
  - Függőleges 4-gombos oszlop a csatorna faderek mellett, középre néző szimmetrikus elrendezésben.
  - TPT bilineáris trapézintegrátor szűrő + C1 simító trajektória: zéró recsegés villámgyors tekeréskor is.
  - Mintánkénti lineáris Dry/Wet simítás mind a 8 effektprocesszorban.
  - Master panel megtisztítása, optimális belső elrendezés a fix 590×340 px keverőben.
  - Telepített bináris: 1 415 680 bájt, SHA256 `62EAEB6B0E691FF1DD745A24CE2CBCD59E6ABB7C90D754812C6755BE2724D460`.
* [x] **Mixer FX Akusztikai Finomhangolás & Zéró Szintváltozás (Build #361):**
  - HPF: $3400\text{ Hz}$ logaritmikus vágás cicergés-kiemeléssel (eltűnik a középsáv, éppen csak a fényes cicergés marad).
  - Flanger: 0%-nál pontosan $1.0\times$ száraz jel (zéró hangerő-esés lista választásakor).
  - Mulholland Drive: 80 Hz Low-Cut, valódi crossfade, megszűnt a hangerőrobbanás és a mély recsegés.
  - Globális középponti bypass garancia mind a 9 effektre.
  - Telepített bináris: 1 414 144 bájt, SHA256 `0CF11E699D5717F2A34F3867C5AC3C95529ED87CFBF21F01D301DBAD10697EF2`.
* [x] **9-Effektes Native Instruments Pro DJ Suite & Konfigurálható Slot Rendszer (Build #360):**
  - HPF cutoff 1400 Hz-re korlátozva (nincs többé némulás magas irányba, az effektek tisztán és érthetően hallhatók).
  - Delay: 3/4 beat +12ms Haas spread (+0.81 fáziskorreláció), 160 Hz send HPF.
  - Reverb: 280 Hz send HPF, 0.18x fésű összegző skálázás (+9 dB hangerőrobbanás elhárítva).
  - Flanger: 0.707x constant-power gain, 120 Hz HPF visszacsatolásban.
  - 5 új effekt hozzáadva: Beatmasher 2, Turntable FX, Ramp Delay, Mulholland Drive, Transpose Stretch.
  - 4 konfigurálható slot `QMenu` legördülő választóval mind a 4 gombon.
  - Szélesített csatornakijelzők (58px), perzisztens slot mentés és teljes kétirányú MIDI LED integráció.
  - Telepített bináris: 1 415 168 bájt, SHA256 `80DEBAD9A4279A2E5818F794D5D5B43E62CD1441D74859EF6E145EC8FBF78F51`.
* [x] **Mixer FX Tempo-Szinkron, Modulált Reverb & Akusztikai Görbék (Build #359):**
  - HPF cutoff max ~3800 Hz-re finomítva (eltűnik a mély és középsáv, de megmarad az ének és cin levegője, nincs süketség).
  - LPF cutoff 25 Hz-ig engedve a mély szub-basszus dübörgésért.
  - Delay: sztereó ütem-osztott pattogás (Bal: 1/2 beat, Jobb: 3/4 beat), 6500 Hz nyitott sávszűrő, 1.85x wet boost.
  - Reverb: zenei tempo-szinkronizált $RT_{60}$ lecsengés (1.5–8 beat), fésűszűrőnként harmonizált feedback, $\pm 4$ mintás lebegő LFO moduláció (selymes stúdió plate karakter, zéró fémes flutter).
  - Flanger: 8-beat zenei söprés, 1.85x emelt hangerő, 0.85 telített feedback a recsegésmentes Jet hangzásért.
  - Telepített bináris: 1 403 904 bájt, SHA256 `C4071C4174E0152A15CCB7F865C24D0CF4BBD37F8283E9AD73E66C2EB94511EF`.
* [x] **Mixer FX Akusztikai Finomhangolás & Hangosságnövelés (Build #358):**
  - HPF max cutoff 2400 Hz-re optimalizálva (nincs többé 100%-os süketség a magas irányba, az effektek tisztán és fényesen szólnak).
  - LPF cutoff 50 Hz-re mélyítve (200 Hz helyett), igazi mély szub-basszus vágás balra tekerve.
  - Delay, Reverb és Flanger wet szintje 1.30–1.50-re emelve, 0.78–0.80 gerjedésmentes, szaturált feedback korlátozással.
* [x] **Traktor Kontrol S2 MK3 4 Mixer FX Rendszer (Build #357):**
  - **Új `src/engine/mixer_fx.hpp/.cpp` modul:**
    - `MixerFxProcessor` és `MixerFxState` nulla dinamikus allokációjú valósidejű DSP motor.
    - 4 effekt: 🔴 Piros (Sima Filter), 🔵 Kék (Filter + 3/4 Beat Delay analóg csillapítással), 🟢 Zöld (Filter + Reverb 4-comb + 2-allpass Freeverb zengővel), 🟡 Sárga (Filter + Flanger 90°-os sztereó Jet söpréssel).
    - **Tail Spillover (Természetes lecsengés):** A potmétert középre tekerve az új bemenet némul, de a delay/reverb farok zeneien, lágyan cseng le.
  - **MixerMasterPanel 4-Gombos Hardveres Elrendezés:**
    - Dedikált `MIXER FX (TRAKTOR S2)` vezérlődoboz a Crossfader felett (`[ 🔴 FLT ] [ 🔵 DLY ] [ 🟢 REV ] [ 🟡 FLG ]`).
    - Élénk színű aktív állapotjelzés, gombnyomásra azonnali deck-szinkronizált váltás.
  - **MixerChannelStrip Csatorna Visszajelzés:**
    - A csatorna címkéje dinamikusan mutatja az aktív effektet és színét (`FLT`, `DLY`, `REV`, `FLG`).
    - Kattintható címke a csatornánkénti közvetlen váltáshoz.
    - Százalékos iránykijelzés: `LPF 45%` / `HPF 45%`, `LP+DLY 45%` / `HP+DLY 45%` stb.
  - **MIDI Hardver Kiosztás & Kétirányú LED Visszajelzés:**
    - `MIXER_FX_1..4` és `MIXER_FX_SELECT` akciók a `midi_types.hpp`-ban és `midi_dispatcher.cpp`-ben.
    - Traktor S2 MK3 profilban Note 44..47 gyári gombok bekötve.
    - `sendFeedback()`: az aktív effekt fizikai LED-je világít a hardveren.
* [x] **TransportBar Gombok Finomhangolása (Build #356):**
  - CUE (54px), PLAY (76px), SYNC (54px), MASTER (58px) arányosított méretezése.
  - 6 px gombközökkel garantált a 624 px-es belső szélességbe való tökéletes illeszkedés, zéró átfedés.
* [x] **Konvertált Média Megőrzés, MIDI Szálvédelem & UI Stabilitás (Build #355):**
  - **Fájlmegőrzés:** SOHA többé nem törlődhet kész videófájl OBS bezárásakor vagy leállásakor.
  - **Fagyásmegelőzés:** Minden hardveres és külső szálról indított Tracklist akció szigorúan a Qt GUI főszálon fut le.
  - **Optimális felületi frissítés:** A konvertálás ideje alatt nincs táblázat-újraépítés, csupán 1 cella szövege frissül.
  - **Tiszta logok:** Megszűnt a másodpercenkénti 120 monitoring warning az OBS logban.
* [x] **Deck Jog Nudge Áthelyezés & 17-Állapotú Harmadolós Loop Rendszer (Build #354):**
  - **Jog Nudge Gombok Áthelyezése a TransportBar-ba:**
    - A `◀ NUDGE` és `NUDGE ▶` gombok kikerültek a `LoopBar` sorából, így a loop sor teljesen fellélegzett és tiszta lett.
    - Új helyük a közvetlenül alatta lévő `TransportBar`-ban kapott helyet a 4 Hot Cue gomb mellett a jobb szélen.
    - Kényelmes **34 px** magasság (pixelre megegyezik a CUE / PLAY / HOT CUE méretével), 50 px szélesség, gyors `autoRepeat` funkcióval ($\pm 5$ ms mikrolépéses lemezlassítás/tolás).
  - **17-Állapotú Harmadolós (Triola) Loop Rendszer:**
    - Beépítésre került az 5 új harmadolós / 3-bar ciklus szigorú zenei és numerikus rendben:  
      `1/16, 1/8, 1/4, 1/2, 1, 2, 4, [6], 8, [12], 16, [24], 32, [48], 64, [96], 128`.
    - Az `EngineLoop::beatMultiplier` és szorzótábla kiegészült a `6.0`, `12.0`, `24.0`, `48.0`, `96.0` értékekkel.
    - Mind a 17 nyomógomb kényelmesen elfér a felszabadult LoopBar sorban (30 px gombok, 2 px hézag = 578 px a 624 px hasznos ablakszélességből).
    - Vizuális differenciálás: a harmadolós gombok nyugalmi állapotban finom kékes-szürke szövegszínt (`#88b0e8`) és keretet kapnak, aktív állapotban pedig narancssárga (`#ff8800`) fénnyel jelzik az aktív loopot.
  - **Alapértelmezett Loop Méret 4 Beat-re (Ütem) Állítása:**
    - Megszűnt a korábbi téves `1/2` ütem megfogás: a `loop_beat_index_`, `loop_beat_idx_a_` és `loop_beat_idx_b_` atomi kezdőértéke az új tömbnek megfelelő `6`-os indexre (`4` beat) állt át.
    - Loop aktiválásakor a deck mostantól azonnal egy tiszta 4-negyedes (1 teljes takt / 4 beat) loopot indít el.
  - **Hardveres MIDI & Engine Clamp Szinkronizálás:**
    - Az `EngineLoop::MAX_LOOP_INDEX = 16`, `setLoopBeatIndex`, `DeckPair::setLoopBeatIndex`, valamint a MIDI vezérlő `LOOP_DOUBLE` és `LOOP_SIZE_CHANGE` clamp értékei szinkronba kerültek a 17-elemű tömbbel.
  - **Telepített bináris:** 1 381 888 bájt, SHA256 `E1F6C8B3B6382A574BE63EF28E2B0C7C5D56E6E4FDAA514C975AF473ECF8F0CB`.
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
* [x] **Audió Recsegés & ASIO / Csatorna Elemzés (Build #375 Diagnózis):**
  - Feltárva a 960 ms puffer-túlcsordulás és folyamatos forrás-újraindítási ciklus (`vj_deck_source_a` deszinkronizáció).
  - Feltárva a hiányzó ASIO motor és az elcsatolt beállítási felület.
  - Elkészült a részletes cselekvési terv a recsegésmentesítésre és a csatornarouting megoldására.

---

## 3. Következő Lépés (Next Action)
* [ ] **1. Lépés:** `vj_deck_source_a` leválasztása az OBS audió gráfjáról a `src/video/deck_source.cpp` fájlban (`deck_enum_active_sources` javítása), megszüntetve a 960 ms puffer-katasztrófát és recsegést.
* [ ] **2. Lépés:** A `SettingsDialog::onSaveClicked()` és `DeckPair::setCueDeviceId` összekötése az azonnali hardveres végpont-váltásért.
* [ ] **3. Lépés:** WASAPI 2-zónás kimenet beállítása (Master: `OUT 1-2`, CUE: `OUT 3-4`), vagy ASIO host motor integrációjának indítása.

---

## 4. Ismert Elakadások / Figyelmeztetések
* **OBS DLL zárolás:** Futó OBS Studio mellett a DLL nem írható felül; frissítéskor az OBS-nek zárva kell lennie.
* **ASIO Meghajtó hiánya:** A plugin felületén szereplő ASIO választó jelenleg nem rendelkezik mögöttes C++ ASIO motorral; jelenleg a közvetlen WASAPI (pl. Behringer XR18 `OUT 3-4`) használható.
