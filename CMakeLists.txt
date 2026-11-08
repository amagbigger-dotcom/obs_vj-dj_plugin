cmake_minimum_required(VERSION 3.20)
project(obs-vj-mixer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Ha nincs megadva, automatikusan kitalálja a runner temp útvonalából
if(NOT OBS_SDK_DIR OR OBS_SDK_DIR STREQUAL "")
    if(DEFINED ENV{OBS_SDK_DIR} AND NOT "$ENV{OBS_SDK_DIR}" STREQUAL "")
        set(OBS_SDK_DIR "$ENV{OBS_SDK_DIR}")
    elseif(DEFINED ENV{RUNNER_TEMP})
        set(OBS_SDK_DIR "$ENV{RUNNER_TEMP}/obs-sdk")
    else()
        set(OBS_SDK_DIR "D:/a/_temp/obs-sdk")
    endif()
endif()

file(TO_CMAKE_PATH "${OBS_SDK_DIR}" OBS_SDK_DIR)
message(STATUS "--> OBS_SDK_DIR: ${OBS_SDK_DIR}")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OBS_SDK_DIR}/include
)

link_directories(
    ${OBS_SDK_DIR}/lib
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
    NOMINMAX
)
