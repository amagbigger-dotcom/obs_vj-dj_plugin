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
    src/plugin-main.cpp
    src/audio-analyzer.cpp
    src/audio-analyzer.hpp
    src/deck-engine.cpp
    src/deck-engine.hpp
    src/deck-source.cpp
    src/deck-window.cpp
    src/deck-window.hpp
    src/waveform-widget.cpp
    src/waveform-widget.hpp
    src/midi-handler.cpp
    src/midi-handler.hpp
)

add_library(obs_vj-dj_plugin MODULE ${SOURCES})

set_target_properties(obs_vj-dj_plugin PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    PREFIX ""
    OUTPUT_NAME "obs-vj-mixer"
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
)

target_compile_definitions(obs_vj-dj_plugin PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)
