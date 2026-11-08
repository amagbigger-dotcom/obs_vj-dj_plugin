# OBS Video DJ (VJ) Mixer Plugin

Professzionalis 2 deckes Video DJ kevero plugin OBS Studio-hoz (Windows x64), amely a **Native Instruments TRAKTOR KONTROL S2 MK3** hardveres kontroller elrendezesere es valos ideju MIDI vezerlesere epul.

## Fobb tulajdonsagok
- **Egyetlen standalone DLL (`obs-vj-mixer.dll`)**: Nem igenyel kulso .effect fajlokat vagy kulon data mappakat (a HLSL shader be van agyazva a C++ forrasba).
- **Lebego, fuggetlen ablak**: Az OBS **Eszkozok (Tools) -> Video DJ Mixer** menujebol nyithato meg. Nem ugrik ossze tole az OBS felulete, kulon monitorra is athuzhato.
- **Traktor S2 MK3 Layout**:
  - Deck A es Deck B: PLAY/PAUSE, CUE, SYNC, AUTO LOOP, idokod kijelzes (`00:00 / 00:00`), es fuggoleges TEMPO/PITCH fader.
  - 2-csatornas klubkevero: HI (Red), MID (Green), LOW (Blue), FILTER (Saturation), kulon csatorna hangero federek es nagymereu Crossfader.
- **Valos ideju WinMM MIDI integracio**: Kozvetlenul fogadja a Traktor S2 MK3 USB MIDI jeleit, es a kepernyon levo csuszkakat is szinkronban mozgatja a fizikai potikkal.
- **4 video atmeneti mod**: Linear Fade, Horizontal Wipe, Additive Blend, Hard Cut.

## Forditas
A GitHub Actions automatikusan leforditja minden commit utan (`.github/workflows/build.yml`), es a kesz DLL letoltheto az Actions -> Artifacts alol.
