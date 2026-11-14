#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/graphics.h>
#include <graphics/effect.h>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <media-io/audio-io.h>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>

#include "deck-engine.hpp"

extern "C" {

struct MixerSource {
    obs_source_t *source = nullptr;
    int channel = 0;

    gs_effect_t *effect = nullptr;
    gs_texrender_t *deck_a = nullptr;
    gs_texrender_t *deck_b = nullptr;
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

    if (!gs_texrender_begin(target, width, height))
        return false;

    gs_ortho(0.0f, static_cast<float>(width),
             0.0f, static_cast<float>(height),
             -100.0f, 100.0f);

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
    return context;
}

static void mixer_destroy(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context)
        return;

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

    obs_source_t *src_a = vdj_get_obs_source_deck(0);
    if (!src_a)
        src_a = deck_a.source();

    obs_source_t *src_b = vdj_get_obs_source_deck(1);
    if (!src_b)
        src_b = deck_b.source();

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

static void mixer_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(seconds);
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->deckA().updateLoop();
        decks->deckB().updateLoop();
    }

    if (!context || !context->source)
        return;

    // Periodically (every ~1 sec) ensure child sources exist under master mixer in active scene
    static auto last_check_tp = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_tp).count() < 1000)
        return;
    last_check_tp = now;

    obs_source_t *scene_source = obs_frontend_get_current_scene();
    if (!scene_source)
        return;

    obs_scene_t *scene = obs_scene_from_source(scene_source);
    if (scene) {
        const char *master_name = obs_source_get_name(context->source);
        obs_sceneitem_t *master_item = obs_scene_find_source(scene, master_name);
        if (master_item) {
            // Master mixer is in this scene! Ensure Deck A, Deck B, and Headphone CUE are present underneath
            obs_sceneitem_t *item_a = obs_scene_find_source(scene, "OBS VJ Deck A");
            if (!item_a) {
                obs_source_t *src_a = obs_get_source_by_name("OBS VJ Deck A");
                if (!src_a) {
                    obs_data_t *settings = obs_data_create();
                    obs_data_set_int(settings, "deck_channel", 0);
                    src_a = obs_source_create("vj_deck_a", "OBS VJ Deck A", settings, nullptr);
                    obs_data_release(settings);
                }
                if (src_a) {
                    obs_sceneitem_t *it = obs_scene_add(scene, src_a);
                    if (it) {
                        obs_sceneitem_set_order(it, OBS_ORDER_MOVE_BOTTOM);
                        obs_sceneitem_set_visible(it, true);
                    }
                    obs_source_release(src_a);
                }
            } else {
                obs_sceneitem_set_visible(item_a, true);
            }

            obs_sceneitem_t *item_b = obs_scene_find_source(scene, "OBS VJ Deck B");
            if (!item_b) {
                obs_source_t *src_b = obs_get_source_by_name("OBS VJ Deck B");
                if (!src_b) {
                    obs_data_t *settings = obs_data_create();
                    obs_data_set_int(settings, "deck_channel", 1);
                    src_b = obs_source_create("vj_deck_b", "OBS VJ Deck B", settings, nullptr);
                    obs_data_release(settings);
                }
                if (src_b) {
                    obs_sceneitem_t *it = obs_scene_add(scene, src_b);
                    if (it) {
                        obs_sceneitem_set_order(it, OBS_ORDER_MOVE_BOTTOM);
                        obs_sceneitem_set_visible(it, true);
                    }
                    obs_source_release(src_b);
                }
            } else {
                obs_sceneitem_set_visible(item_b, true);
            }

            obs_sceneitem_t *item_cue = obs_scene_find_source(scene, "OBS VJ Headphone CUE");
            if (!item_cue) {
                obs_source_t *src_cue = obs_get_source_by_name("OBS VJ Headphone CUE");
                if (!src_cue) {
                    src_cue = obs_source_create("vdj_headphone_cue", "OBS VJ Headphone CUE", nullptr, nullptr);
                }
                if (src_cue) {
                    obs_source_set_monitoring_type(src_cue, OBS_MONITORING_TYPE_MONITOR_ONLY);
                    obs_source_set_audio_mixers(src_cue, 0);
                    obs_sceneitem_t *it = obs_scene_add(scene, src_cue);
                    if (it) obs_sceneitem_set_order(it, OBS_ORDER_MOVE_BOTTOM);
                    obs_source_release(src_cue);
                }
            }

            // Always ensure Master Mixer is visible and stays on top of the visual stack
            obs_sceneitem_set_visible(master_item, true);
            obs_sceneitem_set_order(master_item, OBS_ORDER_MOVE_TOP);
        }
    }
    obs_source_release(scene_source);
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
    master.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;
    master.get_name = mixer_get_name;
    master.create = mixer_create;
    master.destroy = mixer_destroy;
    master.update = mixer_update;
    master.video_tick = mixer_video_tick;
    master.video_render = mixer_video_render;
    master.enum_active_sources = mixer_enum_active_sources;
    master.enum_all_sources = mixer_enum_all_sources;
    master.get_width = mixer_get_width;
    master.get_height = mixer_get_height;
    master.get_properties = mixer_properties;
    master.get_defaults = mixer_defaults;
    obs_register_source_s(&master, sizeof(master));
}

} // extern "C"
