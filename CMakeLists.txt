cmake_minimum_required(VERSION 3.20)
project(obs_vj-dj_plugin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

if(DEFINED ENV{OBS_SDK_DIR} AND NOT "$ENV{OBS_SDK_DIR}" STREQUAL "")
    set(OBS_DIR "$ENV{OBS_SDK_DIR}")
elseif(DEFINED OBS_SDK_DIR AND NOT "${OBS_SDK_DIR}" STREQUAL "")
    set(OBS_DIR "${OBS_SDK_DIR}")
elseif(DEFINED ENV{RUNNER_TEMP})
    set(OBS_DIR "$ENV{RUNNER_TEMP}/obs-sdk")
else()
    set(OBS_DIR "D:/a/_temp/obs-sdk")
endif()

file(TO_CMAKE_PATH "${OBS_DIR}" OBS_DIR)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OBS_DIR}/include
    $ENV{OBS_SDK_DIR}/include
    "D:/a/_temp/obs-sdk/include"
)

link_directories(
    ${OBS_DIR}/lib
    $ENV{OBS_SDK_DIR}/lib
    "D:/a/_temp/obs-sdk/lib"
)

set(SOURCES
    src/plugin-main.cpp
    src/deck-engine.cpp
    src/deck-engine.hpp
    src/mixer-source.cpp
    src/mixer-dock.cpp
    src/mixer-dock.hpp
    src/midi-handler.cpp
    src/midi-handler.hpp
)

add_library(obs_vj-dj_plugin MODULE ${SOURCES})

set_target_properties(obs_vj-dj_plugin PROPERTIES 
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    PREFIX ""
)

target_link_libraries(obs_vj-dj_plugin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    obs
    obs-frontend-api
    winmm
)

target_compile_definitions(obs_vj-dj_plugin PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)
