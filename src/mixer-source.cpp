#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/graphics.h>
#include <graphics/effect.h>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <media-io/audio-io.h>
#include <util/platform.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "deck-engine.hpp"

extern "C" {

// =============================================================================
// Mester Mixer forrás struktúra (Master Mixer Source Structure & Audio Thread)
// =============================================================================
struct MixerSource {
    obs_source_t *source = nullptr;
    int channel = 0;

    gs_effect_t *effect = nullptr;
    gs_texrender_t *deck_a = nullptr;
    gs_texrender_t *deck_b = nullptr;

    uint64_t next_audio_ts = 0;
    std::thread audio_thread;
    std::atomic<bool> audio_thread_running{false};
};

static const char *MIXER_EFFECT = R"(
uniform float4x4 ViewProj;
uniform texture2d image_a;
uniform texture2d image_b;
uniform float4 gains_a;
uniform float4 gains_b;
uniform float saturation_a;
uniform float saturation_b;
uniform float crossfade;
uniform float transition_mode;

sampler_state def_sampler {
    Filter = Linear;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct VertData {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VertData VSDefault(VertData v)
{
    VertData v_out;
    v_out.pos = mul(float4(v.pos.xyz, 1.0), ViewProj);
    v_out.uv  = v.uv;
    return v_out;
}

float4 colorize(float4 color, float4 gains, float saturation)
{
    color.rgb *= gains.rgb;
    float luma = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    color.rgb = lerp(luma.xxx, color.rgb, saturation);
    color.a = 1.0;
    return saturate(color);
}

float4 PS(VertData v) : TARGET
{
    float4 a = colorize(image_a.Sample(def_sampler, v.uv), gains_a, saturation_a);
    float4 b = colorize(image_b.Sample(def_sampler, v.uv), gains_b, saturation_b);

    float4 res;
    if (transition_mode > 2.5) {
        // Hard Cut
        res = crossfade < 0.5 ? a : b;
    } else if (transition_mode > 0.5 && transition_mode < 1.5) {
        // Horizontal Wipe
        res = v.uv.x < (1.0 - crossfade) ? a : b;
    } else if (transition_mode > 1.5 && transition_mode < 2.5) {
        // Additive Blend
        res = saturate(a * (1.0 - crossfade) + b * crossfade);
    } else {
        // Linear Fade / Crossfade
        res = lerp(a, b, crossfade);
    }

    res.a = 1.0;
    return res;
}

technique Draw {
    pass {
        vertex_shader = VSDefault(v);
        pixel_shader = PS(v);
    }
}
)";

static void ensure_resources(MixerSource *context)
{
    if (!context->effect) {
        char *errors = nullptr;
        context->effect = gs_effect_create(MIXER_EFFECT, "mixer_effect", &errors);
        if (!context->effect) {
            blog(LOG_ERROR, "[obs-vj-mixer] Failed to compile MIXER_EFFECT: %s", errors ? errors : "unknown error");
            bfree(errors);
        }
    }

    if (!context->deck_a)
        context->deck_a = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

    if (!context->deck_b)
        context->deck_b = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
}

static bool render_to_texture(gs_texrender_t *target,
                              obs_source_t *source,
                              uint32_t width,
                              uint32_t height)
{
    if (!target || width == 0 || height == 0)
        return false;

    gs_texrender_reset(target);
    if (!gs_texrender_begin(target, width, height))
        return false;

    vec4 clear = {0.0f, 0.0f, 0.0f, 1.0f};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);

    if (source) {
        const uint32_t src_w = obs_source_get_width(source);
        const uint32_t src_h = obs_source_get_height(source);

        gs_blend_state_push();
        gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

        if (src_w > 0 && src_h > 0) {
            gs_matrix_push();
            if (src_w != width || src_h != height) {
                const float sx = static_cast<float>(width) / static_cast<float>(src_w);
                const float sy = static_cast<float>(height) / static_cast<float>(src_h);
                gs_matrix_scale3f(sx, sy, 1.0f);
            }
            obs_source_video_render(source);
            gs_matrix_pop();
        } else {
            obs_source_video_render(source);
        }

        gs_blend_state_pop();
    }

    gs_texrender_end(target);
    return true;
}

static void set_color_params(MixerSource *context,
                             const DeckPlayer &deck_a,
                             const DeckPlayer &deck_b,
                             float crossfade,
                             int transition_mode)
{
    gs_eparam_t *param = nullptr;

    vec4 gains_a;
    vec4 gains_b;

    vec4_set(&gains_a, deck_a.red(), deck_a.green(), deck_a.blue(), 1.0f);
    vec4_set(&gains_b, deck_b.red(), deck_b.green(), deck_b.blue(), 1.0f);

    if ((param = gs_effect_get_param_by_name(context->effect, "gains_a")))
        gs_effect_set_vec4(param, &gains_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "gains_b")))
        gs_effect_set_vec4(param, &gains_b);

    if ((param = gs_effect_get_param_by_name(context->effect, "saturation_a")))
        gs_effect_set_float(param, deck_a.saturation());

    if ((param = gs_effect_get_param_by_name(context->effect, "saturation_b")))
        gs_effect_set_float(param, deck_b.saturation());

    if ((param = gs_effect_get_param_by_name(context->effect, "crossfade")))
        gs_effect_set_float(param, crossfade);

    if ((param = gs_effect_get_param_by_name(context->effect, "transition_mode")))
        gs_effect_set_float(param, static_cast<float>(transition_mode));
}

static const char *mixer_get_name(void *)
{
    return "OBS VJ Master Mixer";
}

static void *mixer_create(obs_data_t *, obs_source_t *source)
{
    auto *context = new MixerSource();
    context->source = source;

    // Minden hangsáv engedélyezése és monitorozás bekapcsolása (Enable all tracks & Monitor and Output)
    obs_source_set_audio_mixers(source, 0x3F);
    obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);

    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setMasterSource(source);
    }

    // =========================================================================
    // Mester RAM Audió Szál: Deck A + Deck B valós idejű keverése Crossfaderrel
    // (Master RAM Audio Thread: Mixing Deck A + Deck B in Real-Time with Crossfader)
    // =========================================================================
    context->audio_thread_running.store(true, std::memory_order_relaxed);
    context->audio_thread = std::thread([context]() {
#ifdef _WIN32
        timeBeginPeriod(1);
#endif
        const uint32_t sample_rate = 48000;
        const size_t frames_per_tick = 480; // 10ms precíz időszelet 48kHz-en

        std::vector<float> buf_a_l(frames_per_tick);
        std::vector<float> buf_a_r(frames_per_tick);
        std::vector<float> buf_b_l(frames_per_tick);
        std::vector<float> buf_b_r(frames_per_tick);
        std::vector<float> master_l(frames_per_tick);
        std::vector<float> master_r(frames_per_tick);

        auto next_wake = std::chrono::steady_clock::now();

        while (context->audio_thread_running.load(std::memory_order_relaxed)) {
            next_wake += std::chrono::milliseconds(10);

            DeckPair *dp = vdj_decks();
            if (!dp || !context->source) {
                std::this_thread::sleep_until(next_wake);
                continue;
            }

            DeckPlayer &da = dp->deckA();
            DeckPlayer &db = dp->deckB();

            bool have_a = false;
            if (da.hasAudioBuffer() && da.isPlaying()) {
                da.readAudioFrames(buf_a_l.data(), buf_a_r.data(), frames_per_tick, sample_rate);
                da.processAudioDsp(buf_a_l.data(), frames_per_tick, 0, sample_rate);
                da.processAudioDsp(buf_a_r.data(), frames_per_tick, 1, sample_rate);
                const float *channels_a[2] = {buf_a_l.data(), buf_a_r.data()};
                dp->queueCueAudio(0, channels_a, frames_per_tick);
                have_a = true;
            } else {
                std::fill(buf_a_l.begin(), buf_a_l.end(), 0.0f);
                std::fill(buf_a_r.begin(), buf_a_r.end(), 0.0f);
            }

            bool have_b = false;
            if (db.hasAudioBuffer() && db.isPlaying()) {
                db.readAudioFrames(buf_b_l.data(), buf_b_r.data(), frames_per_tick, sample_rate);
                db.processAudioDsp(buf_b_l.data(), frames_per_tick, 0, sample_rate);
                db.processAudioDsp(buf_b_r.data(), frames_per_tick, 1, sample_rate);
                const float *channels_b[2] = {buf_b_l.data(), buf_b_r.data()};
                dp->queueCueAudio(1, channels_b, frames_per_tick);
                have_b = true;
            } else {
                std::fill(buf_b_l.begin(), buf_b_l.end(), 0.0f);
                std::fill(buf_b_r.begin(), buf_b_r.end(), 0.0f);
            }

            // Crossfader, csatornahangerő és Gain súlyozás (Gain & Crossfader weighting)
            const float gain_a = da.gain() * da.volume() * da.mixGain();
            const float gain_b = db.gain() * db.volume() * db.mixGain();

            for (size_t i = 0; i < frames_per_tick; ++i) {
                master_l[i] = std::clamp(buf_a_l[i] * gain_a + buf_b_l[i] * gain_b, -1.2f, 1.2f);
                master_r[i] = std::clamp(buf_a_r[i] * gain_a + buf_b_r[i] * gain_b, -1.2f, 1.2f);
            }

            if (have_a || have_b) {
                const uint64_t cur_time = os_gettime_ns();
                if (context->next_audio_ts == 0) {
                    context->next_audio_ts = cur_time + 25000000ULL;
                } else if (cur_time > context->next_audio_ts + 80000000ULL ||
                           context->next_audio_ts > cur_time + 150000000ULL) {
                    context->next_audio_ts = cur_time + 25000000ULL;
                }

                struct obs_source_audio out_audio = {};
                out_audio.data[0] = reinterpret_cast<uint8_t *>(master_l.data());
                out_audio.data[1] = reinterpret_cast<uint8_t *>(master_r.data());
                out_audio.frames = static_cast<uint32_t>(frames_per_tick);
                out_audio.speakers = SPEAKERS_STEREO;
                out_audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
                out_audio.samples_per_sec = sample_rate;
                out_audio.timestamp = context->next_audio_ts;

                context->next_audio_ts += static_cast<uint64_t>(frames_per_tick) * 1000000000ULL / sample_rate;

                obs_source_output_audio(context->source, &out_audio);
            } else {
                context->next_audio_ts = 0;
            }

            const auto now = std::chrono::steady_clock::now();
            if (next_wake > now) {
                std::this_thread::sleep_until(next_wake);
            } else {
                next_wake = now;
            }
        }

#ifdef _WIN32
        timeEndPeriod(1);
#endif
    });

    return context;
}

static void mixer_destroy(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context)
        return;

    // Audió szál biztonságos leállítása a grafika felszabadítása előtt
    context->audio_thread_running.store(false, std::memory_order_relaxed);
    if (context->audio_thread.joinable()) {
        context->audio_thread.join();
    }

    DeckPair *decks = vdj_decks();
    if (decks && decks->masterSource() == context->source) {
        decks->setMasterSource(nullptr);
    }

    obs_enter_graphics();

    if (context->deck_a)
        gs_texrender_destroy(context->deck_a);

    if (context->deck_b)
        gs_texrender_destroy(context->deck_b);

    if (context->effect)
        gs_effect_destroy(context->effect);

    obs_leave_graphics();
    delete context;
}

static void draw_mixer(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();

    if (!context || !decks || !context->effect)
        return;

    uint32_t width = obs_source_get_width(context->source);
    uint32_t height = obs_source_get_height(context->source);

    if (width == 0)
        width = 1920;
    if (height == 0)
        height = 1080;

    DeckPlayer &deck_a = decks->deckA();
    DeckPlayer &deck_b = decks->deckB();

    deck_a.updateLoop();
    deck_b.updateLoop();

    // Közvetlenül a két Deck privát belső forrását rendereljük textúrára,
    // így az OBS VJ Master Mixer BÁRMELY jelenetben működik, Stúdió Mód nélkül, valós időben!
    // (Directly render Deck A and B decoder sources so Master Mixer works in ANY scene without Studio Mode)
    obs_source_t *src_a = deck_a.source();
    obs_source_t *src_b = deck_b.source();

    const bool rendered_a =
        render_to_texture(context->deck_a, src_a, width, height);
    const bool rendered_b =
        render_to_texture(context->deck_b, src_b, width, height);

    gs_texture_t *texture_a =
        rendered_a ? gs_texrender_get_texture(context->deck_a) : nullptr;
    gs_texture_t *texture_b =
        rendered_b ? gs_texrender_get_texture(context->deck_b) : nullptr;

    const int output_mode = decks->obsOutputMode();
    const DeckPlayer *color_a = &deck_a;
    const DeckPlayer *color_b = &deck_b;
    float xfade = static_cast<float>(decks->crossfader());
    if (output_mode == 1) {
        xfade = 0.0f;
    } else if (output_mode == 2) {
        xfade = 1.0f;
    }

    if (!texture_a && !texture_b)
        return;

    if (!texture_a) {
        texture_a = texture_b;
        color_a = &deck_b;
    } else if (!texture_b) {
        texture_b = texture_a;
        color_b = &deck_a;
    }

    gs_eparam_t *param = nullptr;

    if ((param = gs_effect_get_param_by_name(context->effect, "image_a")))
        gs_effect_set_texture(param, texture_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "image_b")))
        gs_effect_set_texture(param, texture_b);

    set_color_params(context, *color_a, *color_b, xfade,
                     decks->transitionMode());

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

    while (gs_effect_loop(context->effect, "Draw")) {
        gs_draw_sprite(texture_a, 0, width, height);
    }

    gs_blend_state_pop();
}

static void mixer_video_render(void *data, gs_effect_t *)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context || !vdj_decks())
        return;

    ensure_resources(context);
    draw_mixer(context);
}

static uint32_t mixer_get_width(void *)
{
    return 1920;
}

static uint32_t mixer_get_height(void *)
{
    return 1080;
}

static void mixer_enum_active_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks)
        return;

    obs_source_t *parent = context->source;
    obs_source_t *source_a = decks->deckA().source();
    obs_source_t *source_b = decks->deckB().source();

    if (source_a)
        cb(parent, source_a, param);
    if (source_b)
        cb(parent, source_b, param);
}

static void mixer_enum_all_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks)
        return;

    obs_source_t *parent = context->source;
    obs_source_t *source_a = decks->deckA().source();
    obs_source_t *source_b = decks->deckB().source();

    if (source_a)
        cb(parent, source_a, param);
    if (source_b)
        cb(parent, source_b, param);
}

static bool enum_audio_devices_cb(void *param, const char *name, const char *id)
{
    obs_property_t *prop = static_cast<obs_property_t *>(param);
    obs_property_list_add_string(prop, name, id);
    return true;
}

static obs_properties_t *mixer_properties(void *)
{
    obs_properties_t *props = obs_properties_create();

    obs_property_t *dev_list = obs_properties_add_list(
        props, "master_device", "Master Audio Output Device (Soundcard)",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(dev_list, "Default Device (Behringer 1-2 Out / Windows Default)", "default");
    obs_enum_audio_monitoring_devices(enum_audio_devices_cb, dev_list);

    obs_property_t *transition = obs_properties_add_list(
        props, "transition", "FX Transition",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(transition, "Linear Fade", 0);
    obs_property_list_add_int(transition, "Horizontal Wipe", 1);
    obs_property_list_add_int(transition, "Additive Blend", 2);
    obs_property_list_add_int(transition, "Hard Cut", 3);

    return props;
}

static void mixer_update(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    if (!settings) return;

    const char *dev = obs_data_get_string(settings, "master_device");
    if (dev && *dev && strcmp(dev, "default") != 0 && obs_audio_monitoring_available()) {
        obs_set_audio_monitoring_device(dev, dev);
    }

    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setTransitionMode(static_cast<int>(obs_data_get_int(settings, "transition")));
    }
}

static void mixer_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "master_device", "default");
    obs_data_set_default_int(settings, "transition", 0);
}

static const char *cue_get_name(void *)
{
    return "OBS VJ Headphone CUE";
}

// =============================================================================
// Videó tick ciklus (Renderelés előtti állapotfrissítés - Video Render Tick)
// =============================================================================
// Meghívja a belső ffmpeg_source privát források obs_source_video_tick metódusát,
// hogy a képkockák folyamatosan dekódolódjanak akkor is, ha nincs külön Deck a jelenetben.
static void mixer_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(data);
    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->deckA().updateLoop();
        decks->deckB().updateLoop();

        obs_source_t *src_a = decks->deckA().source();
        obs_source_t *src_b = decks->deckB().source();
        if (src_a)
            obs_source_video_tick(src_a, seconds);
        if (src_b)
            obs_source_video_tick(src_b, seconds);
    }
}

static obs_properties_t *cue_properties(void *)
{
    obs_properties_t *props = obs_properties_create();
    obs_property_t *dev_list = obs_properties_add_list(
        props, "monitoring_device", "CUE Headphone Soundcard / Device",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(dev_list, "Default Monitoring Device", "default");
    obs_enum_audio_monitoring_devices(enum_audio_devices_cb, dev_list);
    return props;
}

static void cue_update(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    if (!settings) return;
    const char *dev = obs_data_get_string(settings, "monitoring_device");
    if (dev && *dev && strcmp(dev, "default") != 0) {
        obs_set_audio_monitoring_device(dev, dev);
    }
}

static void *cue_create(obs_data_t *settings, obs_source_t *source)
{
    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setCueSource(source);
    }
    obs_source_set_audio_mixers(source, 0); // Mute on main stream / recording tracks
    obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY); // Send to headphones
    cue_update(source, settings);
    return source;
}

static void cue_destroy(void *)
{
    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setCueSource(nullptr);
    }
}

void vdj_register_sources(void)
{
    obs_source_info cue_info = {};
    cue_info.id = "vdj_headphone_cue";
    cue_info.type = OBS_SOURCE_TYPE_INPUT;
    cue_info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
    cue_info.get_name = cue_get_name;
    cue_info.create = cue_create;
    cue_info.destroy = cue_destroy;
    cue_info.get_properties = cue_properties;
    cue_info.update = cue_update;
    obs_register_source_s(&cue_info, sizeof(cue_info));

    obs_source_info master = {};
    master.id = "vdj_master_mixer";
    master.type = OBS_SOURCE_TYPE_INPUT;
    master.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;
    master.get_name = mixer_get_name;
    master.create = mixer_create;
    master.destroy = mixer_destroy;
    master.update = mixer_update;
    master.video_tick = mixer_video_tick;
    master.video_render = mixer_video_render;
    // Az aktív gyermek deck források átadása az OBS-nek az automatikus dekódolási tickhez
    master.enum_active_sources = mixer_enum_active_sources;
    master.enum_all_sources = nullptr;
    master.get_width = mixer_get_width;
    master.get_height = mixer_get_height;
    master.get_properties = mixer_properties;
    master.get_defaults = mixer_defaults;
    obs_register_source_s(&master, sizeof(master));
}

} // extern "C"
