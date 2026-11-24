---
name: obs-windows-building
description: Build OBS Studio plugins for Windows using MSVC (Visual Studio) or MinGW. Covers symbol exports, .def files, Windows-specific linking, platform source files, and DLL verification.
---

# OBS Windows Building

Native build and symbol export workflows for compiling OBS Studio plugins on Windows.

## Purpose

Build OBS Studio plugins for Windows using MSVC (Visual Studio) or MinGW. Covers symbol exports, Windows-specific linking, platform source files, and DLL verification.

## When to Use

- Building and packaging native Windows DLLs (`my-plugin.dll`).
- Setting up Visual Studio (MSVC) or MinGW compiler toolchains.
- Creating module definition files (`.def`) to export OBS runtime entry points.
- Linking Windows system libraries (`ws2_32`, `comctl32`, `user32`, `ole32`).
- Resolving unresolved symbol errors during DLL loading.

## When NOT to Use

- Cross-compiling from Linux &rarr; Use `obs-cross-compiling`
- Qt/C++ frontend development &rarr; Use `obs-cpp-qt-patterns`
- Audio plugin implementation &rarr; Use `obs-audio-plugin-writing`
- Code review &rarr; Use `obs-plugin-reviewing`
- General architecture coordination &rarr; Use `obs-plugin-expert` agent

---

## Quick Start: Windows Build in 4 Steps

### Step 1: Install Prerequisites

- **Visual Studio 2022**: Workload *"Desktop development with C++"*, CMake tools, Windows SDK.
- **Or MinGW (via MSYS2)**:
  ```bash
  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake ninja
  ```

### Step 2: Create Windows CMake Preset (`CMakePresets.json`)

```json
{
  "version": 8,
  "configurePresets": [
    {
      "name": "windows-x64",
      "displayName": "Windows x64",
      "description": "Build for Windows x64 with Visual Studio",
      "binaryDir": "${sourceDir}/build_x64",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo"
      }
    }
  ]
}
```

### Step 3: Create `.def` Export File (`src/plugin.def`)

OBS dynamically loads plugins at runtime via `LoadLibrary` and queries functions by exact name. A `.def` file ensures names are not mangled:

```def
LIBRARY my-plugin
EXPORTS
    obs_module_load
    obs_module_unload
    obs_module_post_load
    obs_module_ver
    obs_module_set_pointer
    obs_current_module
    obs_module_description
    obs_module_set_locale
    obs_module_free_locale
    obs_module_get_string
    obs_module_text
```

### Step 4: Configure `CMakeLists.txt` for Windows

```cmake
if(WIN32)
    # Link essential Windows system libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 comctl32)

    # Export module functions via .def file
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # MinGW GCC
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.def -Wl,--unresolved-symbols=ignore-all"
        )
    else()
        # MSVC
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.def"
        )
    endif()
endif()
```

---

## MSVC vs MinGW Comparison

| Aspect | MSVC (Visual Studio) | MinGW (MSYS2 / GCC) |
| :--- | :--- | :--- |
| **Primary IDE** | Visual Studio 2022 | VS Code, CLion |
| **Debugging** | Visual Studio Native Debugger | GDB |
| **Build Speed** | Slower | Faster |
| **ABI Compatibility**| Native OBS Windows ABI | GCC-based runtime |
| **CI/CD** | Windows Runner | Windows Runner or Linux cross-compile |
| **Recommendation** | **Primary choice for native dev** | Good for CLI automation |

---

## Windows System Libraries & Include Order

### Common System Libraries

| Library | Purpose | Header |
| :--- | :--- | :--- |
| `ws2_32` | Windows Sockets 2 (Networking) | `<winsock2.h>` |
| `comctl32`| Common Controls (GUI widgets) | `<commctrl.h>` |
| `user32` | Windows User API | `<windows.h>` |
| `kernel32`| Core Windows API | `<windows.h>` |
| `ole32` | COM subsystem | `<objbase.h>` |

> [!CAUTION]
> **Include Order Rule:** `<winsock2.h>` MUST always be included BEFORE `<windows.h>`. Otherwise, `<windows.h>` includes an older `<winsock.h>` resulting in macro collisions and compiler errors.

```c
/* CORRECT */
#include <winsock2.h>
#include <windows.h>
```

---

## Platform-Specific Source Separation

```cmake
# Cross-platform common sources
target_sources(${PROJECT_NAME} PRIVATE
    src/plugin-main.c
    src/my-source.c
)

# Platform-specific implementation
if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE src/platform/socket-win32.c)
else()
    target_sources(${PROJECT_NAME} PRIVATE src/platform/socket-posix.c)
endif()
```

---

## References

- **Microsoft C/C++ .def files**: [https://learn.microsoft.com/cpp/build/reference/module-definition-dot-def-files](https://learn.microsoft.com/cpp/build/reference/module-definition-dot-def-files)
- **OBS Build Documentation**: [https://docs.obsproject.com/wiki/install-instructions#windows](https://docs.obsproject.com/wiki/install-instructions#windows)
- **Related Skills**: `obs-cross-compiling`, `obs-plugin-developing`
- **Related Agent**: `obs-plugin-expert`
