cmake_minimum_required(VERSION 3.20)
project(obs-vj-mixer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $ENV{OBS_SDK_DIR}/include
)

link_directories(
    $ENV{OBS_SDK_DIR}/lib
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

add_library(obs-vj-mixer MODULE ${SOURCES})

target_link_libraries(obs-vj-mixer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    obs
    obs-frontend-api
    winmm
)

target_compile_definitions(obs-vj-mixer PRIVATE
    UNICODE
    _UNICODE
)
