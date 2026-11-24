---
name: obs-cross-compiling
description: Cross-compile OBS Studio plugins from Linux to Windows using MinGW-w64. Covers CMake presets, toolchain configuration, headers-only linking, CI/CD workflows, and artifact packaging.
---

# OBS Cross-Compilation (Linux &rarr; Windows)

Guide for cross-compiling OBS Studio Windows plugins on Linux hosts using MinGW-w64.

## Purpose

Cross-compile OBS Studio plugins from Linux to Windows using MinGW-w64. Covers CMake presets, toolchain configuration, headers-only linking, CI/CD workflows, and artifact packaging.

## When to Use

- Building Windows `.dll` binaries from a Linux machine, Docker container, or CI/CD runner.
- Creating automated GitHub Actions / GitLab CI workflows on Linux runners that publish Windows plugin releases.
- Setting up headers-only linking where OBS runtime symbols are resolved dynamically on Windows.

## When NOT to Use

- Native Windows builds with MSVC &rarr; Use `obs-windows-building`
- Qt/C++ frontend development &rarr; Use `obs-cpp-qt-patterns`
- Audio plugin implementation &rarr; Use `obs-audio-plugin-writing`
- Code review &rarr; Use `obs-plugin-reviewing`
- General architecture coordination &rarr; Use `obs-plugin-expert` agent

---

## Quick Start: Cross-Compile in 5 Steps

### Step 1: Install MinGW-w64 on Linux

```bash
# Ubuntu / Debian
sudo apt update && sudo apt install -y mingw-w64 gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 ninja-build cmake

# Verify compiler
x86_64-w64-mingw32-gcc --version
```

### Step 2: Create Toolchain File (`cmake/mingw-w64-toolchain.cmake`)

```cmake
# Target Windows from Linux
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cross-compilers
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Target environment search root
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform flags
set(WIN32 TRUE)
set(MINGW TRUE)
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
set(CMAKE_EXECUTABLE_SUFFIX ".exe")

# Statically link C/C++ runtime to avoid DLL missing errors on Windows
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# Allow unresolved OBS symbols (OBS provides them at runtime when loading the plugin)
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--unresolved-symbols=ignore-all,--warn-unresolved-symbols,--noinhibit-exec")
```

### Step 3: Create `CMakePresets.json`

```json
{
  "version": 8,
  "configurePresets": [
    {
      "name": "linux-cross-windows-x64",
      "displayName": "Cross-compile Windows x64 (from Linux)",
      "description": "Cross-compile for Windows x64 using MinGW-w64 on Linux",
      "binaryDir": "${sourceDir}/build_windows_x64",
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      },
      "generator": "Ninja",
      "toolchainFile": "${sourceDir}/cmake/mingw-w64-toolchain.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "CROSS_COMPILE_WINDOWS": true
      }
    }
  ],
  "buildPresets": [
    {
      "name": "linux-cross-windows-x64",
      "configurePreset": "linux-cross-windows-x64"
    }
  ]
}
```

### Step 4: Configure Headers-Only Linking in `CMakeLists.txt`

```cmake
option(CROSS_COMPILE_WINDOWS "Cross-compile for Windows from Linux" OFF)

if(CROSS_COMPILE_WINDOWS)
    # Headers only - OBS loads and provides symbols at runtime
    add_library(obs-headers INTERFACE)
    target_include_directories(obs-headers INTERFACE
        "${OBS_SOURCE_DIR}/libobs"
        "${OBS_SOURCE_DIR}/frontend/api"
    )
    target_compile_definitions(obs-headers INTERFACE
        UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS WIN32 _WIN32
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE obs-headers)

    # Windows system libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 comctl32)
endif()
```

### Step 5: Build and Verify

```bash
# Configure via preset
cmake --preset linux-cross-windows-x64

# Build .dll
cmake --build --preset linux-cross-windows-x64

# Verify that PE32+ executable DLL was generated
file build_windows_x64/my-plugin.dll
```

---

## GitHub Actions CI Workflow (`.github/workflows/build-windows.yml`)

```yaml
name: Cross-Compile Windows Plugin
on: [push, pull_request]

jobs:
  build-windows:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install MinGW and Ninja
        run: |
          sudo apt-get update
          sudo apt-get install -y mingw-w64 ninja-build
      - name: Configure CMake
        run: cmake --preset linux-cross-windows-x64
      - name: Build
        run: cmake --build --preset linux-cross-windows-x64
      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: windows-plugin-x64
          path: build_windows_x64/*.dll
```

---

## References

- **MinGW-w64 Project**: [https://www.mingw-w64.org/](https://www.mingw-w64.org/)
- **CMake Toolchains Guide**: [https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- **Related Skills**: `obs-windows-building`, `obs-plugin-developing`
- **Related Agent**: `obs-plugin-expert`
