# OBS Video DJ / VJ Mixer Plugin

A dual-deck Video DJ & VJ mixer plugin for OBS Studio with real-time HLSL video crossfading, color grading (3-band EQ mapped to RGB gains), and saturation/filter control.

## Features
- **2 Independent Decks (Deck A & Deck B)** with file loading, play/pause, looping, and seeking.
- **Master Video Output Source** featuring hardware-accelerated HLSL shader mixing:
  - Linear Crossfade
  - Horizontal Wipe
  - Additive Blend
  - Hard Cut
- **Visual FX & Tone Controls:**
  - 3-Band EQ mapped directly to Red, Green, and Blue channel gains.
  - Filter knob mapped to color saturation (0 = grayscale, 1 = normal, 2 = vibrant).
- **Qt6 Mixer Dock:** Integrated OBS UI dock with deck controls, media position tracker, sliders, and crossfader.
- **Automated CI/CD:** GitHub Actions workflow compiling for Windows x64 using MSVC 2022 and Qt 6.5.3.

## Building
This plugin is built with CMake and Qt6 against OBS Studio 30+.
Pushing to your GitHub repository triggers the automated build under the Actions tab.
