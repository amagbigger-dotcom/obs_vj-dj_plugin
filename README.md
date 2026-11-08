# OBS Video DJ (VJ) Mixer Plugin

Professzionális 2 deckes Video DJ keverő plugin OBS Studio-hoz (Windows x64), amely a **Native Instruments TRAKTOR KONTROL S2 MK3** hardveres kontroller elrendezésére és valós idejű MIDI vezérlésére épül.

## Főbb tulajdonságok
- **Egyetlen standalone DLL (`obs_vj-dj_plugin.dll`)**: Nem igényel külső .effect fájlokat vagy külön data mappákat.
- **Lebegő, független ablak**: Az OBS **Eszközök (Tools) -> Video DJ Mixer** menüjéből nyitható meg.
- **Traktor S2 MK3 Layout**: 
  - Deck A és Deck B: PLAY/PAUSE, CUE, SYNC, AUTO LOOP, időkód kijelzés, és függőleges TEMPO/PITCH fader.
  - 2-csatornás klubkeverő: HI, MID, LOW, FILTER, csatorna hangerő és Crossfader.
- **Valós idejű WinMM MIDI integráció**: Közvetlenül fogadja a Traktor S2 MK3 USB MIDI jeleit.
- **4 videó átmeneti mód**: Linear Fade, Horizontal Wipe, Additive Blend, Hard Cut.
