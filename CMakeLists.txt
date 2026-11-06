cmake_minimum_required(VERSION 3.24)
project(obs-vj-mixer VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "obs-vj-mixer requires Windows x64")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

set(OBS_ROOT "" CACHE PATH "OBS SDK or matching OBS build root")

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
    HINTS
        "${OBS_ROOT}/libobs"
        "${OBS_ROOT}/include"
        "${OBS_ROOT}/include/obs"
)

find_path(OBS_FRONTEND_INCLUDE_DIR
    NAMES obs-frontend-api.h
    HINTS
        "${OBS_ROOT}/UI/obs-frontend-api"
        "${OBS_ROOT}/frontend/api"
        "${OBS_ROOT}/include"
)

find_library(LIBOBS_LIBRARY
    NAMES obs libobs
    HINTS
        "${OBS_ROOT}/lib"
        "${OBS_ROOT}/libobs"
        "${OBS_ROOT}/bin/64bit"
)

find_library(OBS_FRONTEND_LIBRARY
    NAMES obs-frontend-api
    HINTS
        "${OBS_ROOT}/lib"
        "${OBS_ROOT}/UI/obs-frontend-api"
        "${OBS_ROOT}/bin/64bit"
)

if(NOT LIBOBS_INCLUDE_DIR OR
   NOT OBS_FRONTEND_INCLUDE_DIR OR
   NOT LIBOBS_LIBRARY OR
   NOT OBS_FRONTEND_LIBRARY)
    message(FATAL_ERROR
        "OBS development files were not found. OBS_ROOT must contain "
        "libobs and obs-frontend-api headers and Windows x64 import libraries.")
endif()

add_library(obs-vj-mixer MODULE
    src/plugin-main.cpp
    src/deck-engine.cpp
    src/mixer-source.cpp
    src/mixer-dock.cpp
    src/deck-engine.hpp
    src/mixer-dock.hpp
)

target_include_directories(obs-vj-mixer PRIVATE
    "${LIBOBS_INCLUDE_DIR}"
    "${OBS_FRONTEND_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(obs-vj-mixer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    "${LIBOBS_LIBRARY}"
    "${OBS_FRONTEND_LIBRARY}"
)

target_compile_definitions(obs-vj-mixer PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_options(obs-vj-mixer PRIVATE
    /W4
    /permissive-
)

set_target_properties(obs-vj-mixer PROPERTIES
    PREFIX ""
    OUTPUT_NAME "obs-vj-mixer"
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS obs-vj-mixer
    RUNTIME DESTINATION "obs-plugins/64bit"
    LIBRARY DESTINATION "obs-plugins/64bit"
)

install(FILES
    src/shaders/crossfade.effect
    DESTINATION "data/obs-plugins/obs-vj-mixer/shaders"
)
