cmake_minimum_required(VERSION 3.25)
project(obs_vj-dj_plugin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Meghatározzuk az OBS SDK mappáját a környezeti változókból vagy a runner temp könyvtárából
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

# Alaposabb include keresés: támogatja a hivatalos ZIP struktúrát és a git source layoutot is
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OBS_DIR}/include
    ${OBS_DIR}/include/libobs
    ${OBS_DIR}/UI/obs-frontend-api
    $ENV{OBS_SDK_DIR}/include
    $ENV{OBS_SDK_DIR}/include/libobs
    $ENV{OBS_SDK_DIR}/UI/obs-frontend-api
    "D:/a/_temp/obs-sdk/include"
    "D:/a/_temp/obs-sdk/include/libobs"
    "D:/a/_temp/obs-sdk/UI/obs-frontend-api"
    "C:/obs-source"
    "C:/obs-source/libobs"
    "C:/obs-source/UI/obs-frontend-api"
)

link_directories(
    ${OBS_DIR}/lib
    ${OBS_DIR}/bin
    $ENV{OBS_SDK_DIR}/lib
    $ENV{OBS_SDK_DIR}/bin
    "D:/a/_temp/obs-sdk/lib"
    "D:/a/_temp/obs-sdk/bin"
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
