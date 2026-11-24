---
name: obs-audio-plugin-writing
description: Develop OBS Studio audio plugins including audio sources (generators, capture) and audio filters (gain, EQ, compression). Covers real-time audio processing patterns, the filter_audio callback, lockless ringbuffer, and audio settings.
---

# OBS Audio Plugin Development

Specialized skill for developing high-performance, real-time audio filters and audio sources for OBS Studio using `libobs`.

## Purpose

Develop OBS Studio audio plugins including audio sources (generators, capture) and audio filters (gain, EQ, compression). Covers real-time audio processing patterns, the `filter_audio` callback, and audio-specific settings.

## When to Use

- Developing audio filters (gain, compressor, EQ, noise suppression, DSP).
- Developing audio sources (microphones, audio capture devices, tone generators).
- Interfacing with raw PCM audio data buffers via `obs_audio_data` or `obs_source_audio`.
- Implementing thread-safe audio parameter updates between UI and audio threads.

## When NOT to Use

- Video plugins &rarr; Use `obs-plugin-developing`
- Output/encoder plugins &rarr; Use `obs-plugin-developing`
- Code review &rarr; Use `obs-plugin-reviewing`
- General architecture guidance &rarr; Use `obs-plugin-expert` agent

---

## Quick Start: Audio Filter in 6 Steps

### Step 1: Create Plugin Structure & Module Exports

```c
#include <obs-module.h>
#include <media-io/audio-math.h>  /* For db_to_mul, mul_to_db */

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("my-audio-filter", "en-US")

/* Forward declaration */
extern struct obs_source_info my_audio_filter;

bool obs_module_load(void)
{
    obs_register_source(&my_audio_filter);
    return true;
}
```

### Step 2: Define Context Structure

```c
struct filter_data {
    obs_source_t *context;    /* Required: Reference to this filter */
    size_t channels;          /* Number of audio channels */
    float gain;               /* Gain multiplier (linear, not dB) */
};
```

### Step 3: Implement Core Callbacks

```c
static const char *filter_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("MyAudioFilter");
}

static void *filter_create(obs_data_t *settings, obs_source_t *filter)
{
    struct filter_data *ctx = bzalloc(sizeof(*ctx));
    ctx->context = filter;
    filter_update(ctx, settings);  /* Apply initial settings */
    return ctx;
}

static void filter_destroy(void *data)
{
    struct filter_data *ctx = data;
    bfree(ctx);  /* CRITICAL: Always free allocated memory */
}

static void filter_update(void *data, obs_data_t *settings)
{
    struct filter_data *ctx = data;
    double db = obs_data_get_double(settings, "gain_db");
    ctx->gain = db_to_mul((float)db);
    ctx->channels = audio_output_get_channels(obs_get_audio());
}
```

### Step 4: Implement `filter_audio` Callback

```c
static struct obs_audio_data *filter_audio(void *data,
                                           struct obs_audio_data *audio)
{
    struct filter_data *ctx = data;
    float **adata = (float **)audio->data;

    for (size_t c = 0; c < ctx->channels; c++) {
        if (audio->data[c]) {  /* CRITICAL: Check channel exists */
            for (size_t i = 0; i < audio->frames; i++) {
                adata[c][i] *= ctx->gain;
            }
        }
    }
    return audio;
}
```

### Step 5: Define Settings & Properties

```c
static void filter_defaults(obs_data_t *settings)
{
    obs_data_set_default_double(settings, "gain_db", 0.0);
}

/* OBS 28+ supports get_properties2 with access to current settings */
static obs_properties_t *filter_properties2(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(settings);
    obs_properties_t *props = obs_properties_create();

    obs_property_t *p = obs_properties_add_float_slider(
        props, "gain_db", obs_module_text("Gain"),
        -30.0, 30.0, 0.1);
    obs_property_float_set_suffix(p, " dB");

    return props;
}
```

### Step 6: Register the Filter

```c
struct obs_source_info my_audio_filter = {
    .id = "my_audio_filter",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_AUDIO,
    .get_name = filter_name,
    .create = filter_create,
    .destroy = filter_destroy,
    .update = filter_update,
    .filter_audio = filter_audio,
    .get_defaults = filter_defaults,
    .get_properties2 = filter_properties2,  /* Modern OBS 28+ callback */
    // .get_properties = filter_properties, /* Fallback for OBS < 28 */
};
```

---

## Audio Source Development

Audio sources generate or capture audio and push it to OBS.

### Audio Source Context Structure

```c
struct source_data {
    obs_source_t *source;
    pthread_t thread;
    os_event_t *event;
    bool active;
    uint64_t timestamp;
};
```

### Audio Source Registration

```c
struct obs_source_info my_audio_source = {
    .id = "my_audio_source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_AUDIO,
    .get_name = source_name,
    .create = source_create,
    .destroy = source_destroy,
    /* Audio sources push audio via obs_source_output_audio() */
};
```

### Pushing Audio Data

```c
static void push_audio(struct source_data *ctx, uint8_t *buffer, size_t frames)
{
    struct obs_source_audio audio = {
        .data[0] = buffer,
        .frames = frames,
        .speakers = SPEAKERS_MONO,        /* Or SPEAKERS_STEREO, etc. */
        .samples_per_sec = 48000,
        .timestamp = ctx->timestamp,
        .format = AUDIO_FORMAT_FLOAT_PLANAR,
    };

    obs_source_output_audio(ctx->source, &audio);
    ctx->timestamp += (uint64_t)frames * 1000000000ULL / 48000;
}
```

---

## Audio Data Structures

### `obs_audio_data` (Filter Input)

```c
struct obs_audio_data {
    uint8_t *data[MAX_AV_PLANES];  /* Audio channel data */
    uint32_t frames;               /* Number of audio frames */
    uint64_t timestamp;            /* Timestamp in nanoseconds */
};
```

### `obs_source_audio` (Source Output)

```c
struct obs_source_audio {
    const uint8_t *data[MAX_AV_PLANES];
    uint32_t frames;
    enum speaker_layout speakers;
    enum audio_format format;
    uint32_t samples_per_sec;
    uint64_t timestamp;
};
```

### Speaker Layouts

| Constant | Channels | Description |
| :--- | :---: | :--- |
| `SPEAKERS_MONO` | 1 | Single channel |
| `SPEAKERS_STEREO` | 2 | Left, Right |
| `SPEAKERS_2POINT1` | 3 | L, R, LFE |
| `SPEAKERS_4POINT0` | 4 | L, R, C, S |
| `SPEAKERS_4POINT1` | 5 | L, R, C, LFE, S |
| `SPEAKERS_5POINT1` | 6 | L, R, C, LFE, SL, SR |
| `SPEAKERS_7POINT1` | 8 | L, R, C, LFE, SL, SR, BL, BR |

### Audio Formats

| Constant | Type | Description |
| :--- | :--- | :--- |
| `AUDIO_FORMAT_U8BIT` | `uint8_t` | Unsigned 8-bit |
| `AUDIO_FORMAT_16BIT` | `int16_t` | Signed 16-bit |
| `AUDIO_FORMAT_32BIT` | `int32_t` | Signed 32-bit |
| `AUDIO_FORMAT_FLOAT` | `float` | 32-bit float interleaved |
| `AUDIO_FORMAT_FLOAT_PLANAR` | `float` | 32-bit float planar |

### Audio Math Functions (`media-io/audio-math.h`)

```c
/* Convert decibels to linear multiplier */
float db_to_mul(float db);
/* Example: db_to_mul(-6.0) ≈ 0.5 (half volume) */

/* Convert linear multiplier to decibels */
float mul_to_db(float mul);
/* Example: mul_to_db(0.5) ≈ -6.0 dB */
```

---

## Audio Properties UI Helpers

```c
/* Gain Slider (dB) */
obs_property_t *p = obs_properties_add_float_slider(
    props, "gain_db", "Gain", -30.0, 30.0, 0.1);
obs_property_float_set_suffix(p, " dB");

/* Channel Selector */
obs_property_t *ch = obs_properties_add_list(
    props, "channel", "Channel",
    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
obs_property_list_add_int(ch, "All", -1);
obs_property_list_add_int(ch, "Left", 0);
obs_property_list_add_int(ch, "Right", 1);

/* Frequency Selector */
obs_property_t *freq = obs_properties_add_int_slider(
    props, "frequency", "Frequency (Hz)", 20, 20000, 1);
obs_property_int_set_suffix(freq, " Hz");
```

---

## FORBIDDEN Patterns & Critical Violations

| Pattern | Problem | Solution |
| :--- | :--- | :--- |
| **Missing `OBS_DECLARE_MODULE()`** | Plugin won't load | Add macro at file start |
| **`return false` from `obs_module_load()`** | Plugin fails silently | Return `true` on success |
| **Missing `bfree()` in destroy** | Memory leak | Always free allocated context |
| **Global state variables** | Thread safety issues | Encapsulate state in context struct |
| **Blocking in `filter_audio`** | Audio glitches / crackling | Never block, lock mutexes, or wait |
| **Ignoring NULL channels** | Segmentation fault / crash | Check `if (audio->data[c])` |
| **Ignoring `audio->frames`** | Buffer overflow | Always respect incoming frame count |
| **`malloc()`/`bzalloc()` in `filter_audio`** | Real-time audio dropout | Pre-allocate all buffers in `create` |

### Anti-Pattern Examples

```c
/* BAD: Global state */
static float g_gain = 1.0;  /* WRONG - not thread safe */

/* GOOD: Context structure */
struct filter_data {
    float gain;  /* Per-instance state */
};

/* BAD: Ignoring NULL channels */
for (size_t c = 0; c < channels; c++) {
    adata[c][i] *= gain;  /* CRASH if channel doesn't exist */
}

/* GOOD: Check channel exists */
for (size_t c = 0; c < channels; c++) {
    if (audio->data[c]) {  /* Safe check first */
        adata[c][i] *= gain;
    }
}
```

---

## Thread Safety Guidelines

The `filter_audio` callback runs on the **real-time audio thread**, completely decoupled from the UI/main thread.

1. **Never block**: No mutexes, no file/network I/O, no memory allocations.
2. **Use atomics**: For shared scalar state between UI updates and the audio callback.
3. **Pre-allocate**: Allocate ringbuffers and scratch arrays in `create`.

### Safe Atomic Parameter Pattern

```c
#include <stdatomic.h>

struct filter_data {
    atomic_int gain_int;  /* Store gain * 1000 as scaled int */
};

static void filter_update(void *data, obs_data_t *settings)
{
    struct filter_data *ctx = data;
    double db = obs_data_get_double(settings, "gain_db");
    int gain_scaled = (int)(db_to_mul((float)db) * 1000);
    atomic_store(&ctx->gain_int, gain_scaled);
}

static struct obs_audio_data *filter_audio(void *data, struct obs_audio_data *audio)
{
    struct filter_data *ctx = data;
    float gain = atomic_load(&ctx->gain_int) / 1000.0f;
    /* Process audio samples safely */
    return audio;
}
```

---

## References

- **OBS Source Reference**: [https://docs.obsproject.com/reference-sources](https://docs.obsproject.com/reference-sources)
- **Settings Reference**: [https://docs.obsproject.com/reference-settings](https://docs.obsproject.com/reference-settings)
- **OBS Gain Filter Example**: [https://github.com/obsproject/obs-studio/blob/master/plugins/obs-filters/gain-filter.c](https://github.com/obsproject/obs-studio/blob/master/plugins/obs-filters/gain-filter.c)
- **Related Skills**: `obs-plugin-developing`, `obs-plugin-reviewing`
- **Related Agent**: `obs-plugin-expert`
