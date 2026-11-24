---
name: obs-plugin-developing
description: Entry point skill for the OBS Studio plugin development ecosystem. Routes to specialized skills based on plugin type and guides initial project setup. Covers plugin types (sources, filters, transitions, outputs, encoders, services), module registration boilerplate (libobs), CMake configuration, and obs-plugintemplate scaffolding.
---

# OBS Plugin Development

Entry point skill for the OBS Studio plugin development ecosystem. Routes to specialized skills based on plugin type and guides initial project setup.

## Purpose

Provides foundational guidance, architectural definitions, and scaffolding patterns for building native C/C++ plugins for OBS Studio.

## When to Use

- Starting a new OBS Studio plugin project from scratch.
- Setting up the CMake build environment and module registration boilerplate.
- Determining the proper plugin component types (`obs_source_info`, `obs_output_info`, `obs_encoder_info`, `obs_service_info`).
- Bootstrapping using the official `obsproject/obs-plugintemplate`.

## When NOT to Use

- Developing audio-specific plugins (filters, DSP, generators) &rarr; Use `obs-audio-plugin-writing`
- Auditing or reviewing existing plugin code for memory/thread safety &rarr; Use `obs-plugin-reviewing`
- Requiring end-to-end multi-domain guidance &rarr; Use `obs-plugin-expert` agent

---

## Skill Routing

| Task                         | Use Skill                    |
| :--------------------------- | :--------------------------- |
| **Create audio filter**      | `obs-audio-plugin-writing`   |
| **Create audio source**      | `obs-audio-plugin-writing`   |
| **Review plugin code**       | `obs-plugin-reviewing`       |
| **Video plugins (future)**   | Check this skill for updates |
| **Output plugins (future)**  | Check this skill for updates |
| **Encoder plugins (future)** | Check this skill for updates |

---

## OBS Plugin Types Overview

### 1. Sources (`obs_source_info`)

Sources render video and/or audio content. Three main categories:

| Type           | Flag                         | Purpose                                     |
| :------------- | :--------------------------- | :------------------------------------------ |
| **INPUT**      | `OBS_SOURCE_TYPE_INPUT`      | Capture devices, media playback, generators |
| **FILTER**     | `OBS_SOURCE_TYPE_FILTER`     | Process video/audio from a parent source    |
| **TRANSITION** | `OBS_SOURCE_TYPE_TRANSITION` | Animate between scenes or sources           |

#### Output Capability Flags
- `OBS_SOURCE_VIDEO`: Renders visual video output.
- `OBS_SOURCE_AUDIO`: Outputs audio samples.
- `OBS_SOURCE_ASYNC_VIDEO`: Provides asynchronous raw frames (RAM-based).
- `OBS_SOURCE_COMPOSITE`: Contains and renders child sources.

### 2. Outputs (`obs_output_info`)

Handle streaming and recording by receiving raw or encoded streams.
- **Examples**: RTMP streaming, MP4/MKV recording, FFmpeg muxing.

### 3. Encoders (`obs_encoder_info`)

Wrap codec implementations for video and audio compression.
- **Examples**: x264, NVENC, Intel QuickSync, Apple VideoToolbox, AAC.

### 4. Services (`obs_service_info`)

Integrate with streaming platforms and broadcast service endpoints.
- **Examples**: Twitch, YouTube, custom RTMP/SRT servers.

---

## Module Registration (Required)

Every OBS plugin requires standard module export and lifecycle boilerplate:

```c
#include <obs-module.h>
#include <plugin-support.h>  /* Provides obs_log and PLUGIN_VERSION macros */

/* Required: Exports common module functions */
OBS_DECLARE_MODULE()

/* Optional: Load locale from data/locale/ */
OBS_MODULE_USE_DEFAULT_LOCALE("my-plugin", "en-US")

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

    /* Register your plugin components here */
    obs_register_source(&my_source);
    // obs_register_output(&my_output);
    // obs_register_encoder(&my_encoder);
    // obs_register_service(&my_service);

    return true;  /* Must return true on success */
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "plugin unloaded");
    /* Optional: Cleanup allocated resources on unload */
}
```

> [!TIP]
> **Properties Callback (OBS 28+):**
> When defining `obs_source_info`, prefer `get_properties2` over `get_properties` if your UI controls depend dynamically on existing source settings:
> ```c
> static obs_properties_t *my_properties2(void *data, obs_data_t *settings) { ... }
> /* In obs_source_info: */
> .get_properties2 = my_properties2,
> ```

---

## Build System Setup

### Using `obs-plugintemplate` (Recommended)

Clone and customize the official boilerplate template:

```bash
# Clone template
git clone https://github.com/obsproject/obs-plugintemplate my-plugin
cd my-plugin

# Edit buildspec.json with your plugin metadata
# Implement your module in src/plugin-main.c
```

### `buildspec.json` Configuration

```json
{
  "name": "my-plugin",
  "displayName": "My Plugin",
  "version": "1.0.0",
  "author": "Your Name",
  "website": "https://example.com",
  "email": "you@example.com"
}
```

### `CMakeLists.txt` Structure

```cmake
cmake_minimum_required(VERSION 3.28...3.30)

project(my-plugin VERSION 1.0.0)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)
```

### Build Commands

```bash
# Configure build directory
cmake -B build -S .

# Compile
cmake --build build

# Install (copies to OBS plugins directory)
cmake --install build
```

---

## Standard Project Structure

```text
my-plugin/
├── CMakeLists.txt
├── buildspec.json
├── src/
│   ├── plugin-main.c     # Module registration and lifecycle callbacks
│   ├── my-source.c       # Source implementation
│   └── plugin-support.h  # Helper macros and definitions
└── data/
    └── locale/
        └── en-US.ini     # Localization strings
```

---

## External Documentation

### MCP / Context Tools
- **Context7**: `/obsproject/obs-studio` (topic: `plugin development`)

### Official Documentation
- **Plugin Guide**: [https://docs.obsproject.com/plugins](https://docs.obsproject.com/plugins)
- **Plugin Template**: [https://github.com/obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
- **Module Reference**: [https://docs.obsproject.com/reference-modules](https://docs.obsproject.com/reference-modules)
- **Source Reference**: [https://docs.obsproject.com/reference-sources](https://docs.obsproject.com/reference-sources)

---

## Related Skills & Agents

- **`obs-audio-plugin-writing`**: Audio sources and filters (real-time DSP, callbacks).
- **`obs-plugin-reviewing`**: Code review, memory safety, and thread-safety audit.
- **`obs-plugin-expert` agent**: Coordinated guidance across all OBS plugin skills, multi-domain workflows, and troubleshooting.
