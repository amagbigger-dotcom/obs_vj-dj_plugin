# OBS Video DJ (VJ) Mixer - Mentett Mérföldkövek és Támpontok (Milestones)

Ez a mappa a projekt hivatalos mérföldköveit, stabil támpontjait és a hozzájuk tartozó binárisokat dokumentálja. Bármelyik mérföldkőre azonnal vissza lehet ugrani.

---

## 📌 Hivatalos Mérföldkő: Run #231 / Build #222 (Kiemelt Támpont)
* **Dátum:** 2026. november 15.
* **Git Tag:** `milestone-run231-build222`
* **Git Commit:** [`c680bd5f72af90d939729dd54952c5ed2d60c59f`](https://github.com/amagbigger-dotcom/obs_vj-dj_plugin/commit/c680bd5)
* **GitHub Actions Run:** [#231](https://github.com/amagbigger-dotcom/obs_vj-dj_plugin/actions/runs/19385555132)
* **Bináris fájlnév:** `obs_dj-vj_plugin_run231_build222.dll`
* **SHA256 hash:** `5CA2942365846B944C89AD3347F84D5AA141DBBF4082D7D12464214BE80C401D`
* **Helyi biztonsági másolat:** `c:\gitrepo\obs_vj-dj_plugin\milestones\obs_dj-vj_plugin_run231_build222.dll`

### Miért kiemelt támpont? (Elért funkciók):
1. **Stabil 2-deckes Video DJ motor:** Valós idejű zenei és videó lejátszás akadásmentes motorral, 4 órás track támogatással.
2. **Külön füles előhallgatás (CUE / PGM):** Független WASAPI streaming kimenet (OUT 3-4), a Crossfadertől teljesen független CUE belehallgatással és Master arány keveréssel.
3. **4 Hot Cue pont deckenként:** Tárolás `.vdj.json`-ben és interaktív jelölők a kétzónás gördülő hullámformán.
4. **12 Professzionális VJ Crossfader Shader:** Swirl, Fisheye, RGB Split, Zoom Blur, Pixelate, Luma Dissolve, Liquid Ripple, Kaleidoscope, Glitch, VHS CRT, Pop-Art Halftone, Neon Sobel kontúr.
5. **Lejátszás alatti számbetöltés védelem:** Nem enged véletlenül zenét rátölteni forgó deckre.
6. **Deck-színű vizuális felület:**
   - Deck A: Mély kék alapszín, ciánkék fader és BPM kijelzés.
   - Deck B: Mély meleg borostyán/narancs alapszín, narancs fader és BPM kijelzés.
   - Mixer oldalpanelek: Deck-színű bal és jobb sáv, sötét grafit középső Master blokk.
7. **Munkamenet-alapú Tracklist:** Zöld pipák csak az aktuális szettre maradnak meg, kilépéskor törlődnek + `[ ↺ ÚJ SZETT ]` gyors gomb.
8. **Fix ablakméretek és vibrálásmentes felület:** Nem ugrálnak a csúszkák és a kijelzők semmilyen értékváltozáskor.

---

### Visszaállítás erre a mérföldkőre:
Ha a jövőben bármilyen módosítás után vissza szeretnél térni erre a stabil változatra:
1. **Azonnali DLL másolással:**
   ```powershell
   Copy-Item "c:\gitrepo\obs_vj-dj_plugin\milestones\obs_dj-vj_plugin_run231_build222.dll" "C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll" -Force
   ```
2. **Git kódbázis visszaállítással:**
   ```powershell
   git checkout milestone-run231-build222
   ```
