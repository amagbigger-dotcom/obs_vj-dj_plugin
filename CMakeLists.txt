cmake_minimum_required(VERSION 3.25)
project(obs_vj-dj_plugin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

if(DEFINED ENV{OBS_SDK_DIR})
    set(OBS_DIR "$ENV{OBS_SDK_DIR}")
elseif(DEFINED RUNNER_TEMP)
    set(OBS_DIR "$env:RUNNER_TEMP/obs-sdk")
else()
    set(OBS_DIR "D:/a/_temp/obs-sdk")
endif()

file(TO_CMAKE_PATH "${OBS_DIR}" OBS_DIR)

set(SOURCES
    # Core Plugin Entry
    src/plugin-main.cpp

    # Engine (Playback & State Machine)
    src/engine/deck_engine.cpp
    src/engine/deck_engine.hpp

    # Audio (WMF Analysis & WASAPI CUE)
    src/audio/audio_analyzer.cpp
    src/audio/audio_analyzer.hpp
    src/audio/wasapi_output.cpp
    src/audio/wasapi_output.hpp

    # Sources (Audio Decoders)
    src/sources/wmf_decoder.cpp
    src/sources/wmf_decoder.hpp

    # Analyzer (Beats, Grid & Waveform)
    src/analyzer/analyzer_beats.cpp
    src/analyzer/analyzer_beats.hpp
    src/analyzer/analyzer_waveform.cpp
    src/analyzer/analyzer_waveform.hpp

    # Track (Cache & Metadata)
    src/track/track_cache.cpp
    src/track/track_cache.hpp

    # Video (OBS Sources & Transcoder)
    src/video/deck_source.cpp
    src/video/mixer_source.cpp
    src/video/video_converter.cpp
    src/video/video_converter.hpp

    # Effects (ISF Shaders & Modulation)
    src/effects/vj_config.hpp
    src/effects/vj_shaders.hpp
    src/effects/fluid_flow.hpp

    # Controllers (Hardware MIDI)
    src/controllers/midi_handler.cpp
    src/controllers/midi_handler.hpp

    # Waveform (Canvas & Spectral Rendering)
    src/waveform/waveform_widget.cpp
    src/waveform/waveform_widget.hpp

    # Widget (Reusable UI Controls)
    src/widget/dj_slider.cpp
    src/widget/dj_slider.hpp
    src/widget/loop_bar.cpp
    src/widget/loop_bar.hpp
    src/widget/transport_bar.cpp
    src/widget/transport_bar.hpp
    src/widget/tempo_control.cpp
    src/widget/tempo_control.hpp

    # GUI (Windows, Docks & Dialogs)
    src/gui/deck_window.cpp
    src/gui/deck_window.hpp
    src/gui/mixer_window.cpp
    src/gui/mixer_window.hpp
    src/gui/tracklist_dock.cpp
    src/gui/tracklist_dock.hpp
    src/gui/vj_fx_dialog.cpp
    src/gui/vj_fx_dialog.hpp
)

add_library(obs_vj-dj_plugin MODULE ${SOURCES})

set_target_properties(obs_vj-dj_plugin PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    PREFIX ""
    OUTPUT_NAME "obs_dj-vj_plugin"
)

target_include_directories(obs_vj-dj_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    "${OBS_DIR}/include"
    "${OBS_DIR}/include/libobs"
    "${OBS_DIR}/include/obs-frontend-api"
)

target_link_directories(obs_vj-dj_plugin PRIVATE
    "${OBS_DIR}/lib"
    "${OBS_DIR}/bin"
)

target_link_libraries(obs_vj-dj_plugin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    libobs
    obs-frontend-api
    winmm
    mfplat
    mfreadwrite
    mfuuid
    propsys
    ole32
)

target_compile_definitions(obs_vj-dj_plugin PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

target_precompile_headers(obs_vj-dj_plugin PRIVATE
    <obs-module.h>
    <obs-frontend-api.h>
    <QWidget>
    <QString>
    <vector>
    <string>
    <atomic>
    <memory>
    <algorithm>
)
