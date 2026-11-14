#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/effect.h>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <media-io/audio-io.h>

#include <cstring>
#include <cmath>
#include <algorithm>

#include "deck-engine.hpp"

extern "C" {

struct FilterData {
    obs_source_t *source = nullptr;
    int deck_idx = 0;
};

static const char *filter_get_name(void *)
{
    return "OBS VJ/DJ Deck Audio DSP Filter";
}

static void *filter_create(obs_data_t *settings, obs_source_t *source)
{
    auto *filter = new FilterData();
    filter->source = source;
    filter->deck_idx = settings ? static_cast<int>(obs_data_get_int(settings, "deck_idx")) : 0;
    return filter;
}

static void filter_destroy(void *data)
{
    delete static_cast<FilterData *>(data);
}

static struct obs_audio_data *filter_audio(void *data, struct obs_audio_data *audio)
{
    auto *filter = static_cast<FilterData *>(data);
    DeckPair *decks = vdj_decks();
    if (!filter || !decks || !audio)
        return audio;

    DeckPlayer &deck = (filter->deck_idx == 0) ? decks->deckA() : decks->deckB();
    const size_t frames = audio->frames;
    for (size_t ch = 0; ch < 2; ch++) {
        if (audio->data[ch]) {
            deck.processAudioDsp(reinterpret_cast<float *>(audio->data[ch]),
                                  frames, ch, 48000);
        }
    }

    decks->queueCueAudio(filter->deck_idx,
                         reinterpret_cast<const float *const *>(audio->data),
                         frames);

    return audio;
}

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
uniform float time_a;
uniform float time_b;
uniform float is_playing_a;
uniform float is_playing_b;
uniform float bpm_a;
uniform float bpm_b;
uniform float has_video_a;
uniform float has_video_b;

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

// Procedural DJ VJ Studio Visualizer for audio files (.wav, .mp3)
float4 render_dj_visualizer(float2 uv, float time_sec, float is_playing, float bpm, float3 theme_color, float4 gains, float saturation)
{
    float2 center = float2(0.5, 0.5);
    float2 p = (uv - center) * float2(1.77777, 1.0);
    float dist = length(p);
    float angle = atan2(p.y, p.x);

    // Rotating vinyl turntable
    float eff_bpm = max(bpm, 60.0);
    float rot_speed = is_playing * (eff_bpm / 60.0) * 1.8;
    float a_rot = angle + time_sec * rot_speed;

    // Turntable vinyl body (radius 0.38)
    float vinyl_mask = smoothstep(0.40, 0.395, dist) * smoothstep(0.06, 0.065, dist);
    float vinyl_grooves = sin(dist * 220.0) * 0.08 * vinyl_mask;
    float vinyl_sheen = pow(abs(cos(a_rot * 2.0)), 8.0) * 0.28 * vinyl_mask;

    // Center vinyl label (radius 0.12)
    float label_mask = smoothstep(0.12, 0.118, dist) * smoothstep(0.03, 0.032, dist);
    float3 label_color = theme_color * 0.9;

    // Spindle hole
    float spindle = smoothstep(0.032, 0.03, dist);

    // Neon horizon grid background
    float2 grid_uv = float2(uv.x - 0.5, 1.0 - uv.y);
    float grid_persp = 0.12 / max(grid_uv.y, 0.02);
    float2 grid_pos = frac(float2(grid_uv.x * grid_persp * 8.0, grid_persp * 12.0 - time_sec * is_playing * 2.5));
    float grid_val = step(0.92, grid_pos.x) + step(0.92, grid_pos.y);
    float grid_fade = smoothstep(0.0, 0.65, grid_uv.y) * 0.25;

    // Frequency spectrum bars along the bottom
    float bar_x = frac(uv.x * 28.0);
    float bar_idx = floor(uv.x * 28.0);
    float bar_height = 0.08 + 0.18 * abs(sin(bar_idx * 1.35 + time_sec * 6.0 * is_playing));
    float bar_mask = step(bar_x, 0.75) * step(1.0 - uv.y, bar_height) * step(0.02, 1.0 - uv.y);

    // Composite
    float3 col = float3(0.04, 0.05, 0.08); // Dark studio background
    col += theme_color * grid_val * grid_fade;
    col += float3(0.09, 0.09, 0.11) * vinyl_mask + vinyl_grooves + vinyl_sheen;
    col = lerp(col, label_color, label_mask);
    col = lerp(col, float3(0.85, 0.85, 0.85), spindle);
    col += theme_color * bar_mask * 0.85;

    // Outer glow ring
    float ring = smoothstep(0.015, 0.0, abs(dist - 0.40));
    col += theme_color * ring * 0.7;

    return colorize(float4(col, 1.0), gains, saturation);
}

float4 PS(VertData v) : TARGET
{
    float4 a;
    if (has_video_a > 0.5) {
        a = colorize(image_a.Sample(def_sampler, v.uv), gains_a, saturation_a);
    } else {
        a = render_dj_visualizer(v.uv, time_a, is_playing_a, bpm_a, float3(0.0, 0.82, 1.0), gains_a, saturation_a);
    }

    float4 b;
    if (has_video_b > 0.5) {
        b = colorize(image_b.Sample(def_sampler, v.uv), gains_b, saturation_b);
    } else {
        b = render_dj_visualizer(v.uv, time_b, is_playing_b, bpm_b, float3(1.0, 0.5, 0.0), gains_b, saturation_b);
    }

    float4 res;
    if (transition_mode > 2.5) {
        res = crossfade < 0.5 ? a : b;
    } else if (transition_mode > 0.5 && transition_mode < 1.5) {
        res = v.uv.x < crossfade ? b : a;
    } else if (transition_mode > 1.5 && transition_mode < 2.5) {
        res = saturate(a * (1.0 - crossfade) + b * crossfade);
    } else {
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
                             int transition_mode,
                             float has_vid_a,
                             float has_vid_b)
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

    if ((param = gs_effect_get_param_by_name(context->effect, "time_a")))
        gs_effect_set_float(param, static_cast<float>(deck_a.currentTimeMs()) / 1000.0f);

    if ((param = gs_effect_get_param_by_name(context->effect, "time_b")))
        gs_effect_set_float(param, static_cast<float>(deck_b.currentTimeMs()) / 1000.0f);

    if ((param = gs_effect_get_param_by_name(context->effect, "is_playing_a")))
        gs_effect_set_float(param, deck_a.isPlaying() ? 1.0f : 0.0f);

    if ((param = gs_effect_get_param_by_name(context->effect, "is_playing_b")))
        gs_effect_set_float(param, deck_b.isPlaying() ? 1.0f : 0.0f);

    if ((param = gs_effect_get_param_by_name(context->effect, "bpm_a")))
        gs_effect_set_float(param, static_cast<float>(deck_a.effectiveBpm()));

    if ((param = gs_effect_get_param_by_name(context->effect, "bpm_b")))
        gs_effect_set_float(param, static_cast<float>(deck_b.effectiveBpm()));

    if ((param = gs_effect_get_param_by_name(context->effect, "has_video_a")))
        gs_effect_set_float(param, has_vid_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "has_video_b")))
        gs_effect_set_float(param, has_vid_b);
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
    if (!src_a) src_a = deck_a.source();
    obs_source_t *src_b = vdj_get_obs_source_deck(1);
    if (!src_b) src_b = deck_b.source();

    const bool rendered_a =
        render_to_texture(context->deck_a, src_a, width, height);
    const bool rendered_b =
        render_to_texture(context->deck_b, src_b, width, height);

    gs_texture_t *texture_a =
        rendered_a ? gs_texrender_get_texture(context->deck_a) : nullptr;
    gs_texture_t *texture_b =
        rendered_b ? gs_texrender_get_texture(context->deck_b) : nullptr;

    const float has_vid_a = deck_a.hasVideo() ? 1.0f : 0.0f;
    const float has_vid_b = deck_b.hasVideo() ? 1.0f : 0.0f;

    const int output_mode = decks->obsOutputMode();
    const DeckPlayer *color_a = &deck_a;
    const DeckPlayer *color_b = &deck_b;
    float xfade = static_cast<float>(decks->crossfader());
    if (output_mode == 1) {
        xfade = 0.0f;
    } else if (output_mode == 2) {
        xfade = 1.0f;
    } else {
        const bool loaded_a = !deck_a.filePath().isEmpty();
        const bool loaded_b = !deck_b.filePath().isEmpty();
        if (loaded_a && !loaded_b) {
            xfade = 0.0f;
        } else if (!loaded_a && loaded_b) {
            xfade = 1.0f;
        }
    }

    if (!texture_a && !texture_b)
        return;

    if (!texture_a) {
        texture_a = texture_b;
        color_a = &deck_b;
        color_b = &deck_b;
        xfade = 1.0f;
    } else if (!texture_b) {
        texture_b = texture_a;
        color_a = &deck_a;
        color_b = &deck_a;
        xfade = 0.0f;
    }

    gs_eparam_t *param = nullptr;
    if ((param = gs_effect_get_param_by_name(context->effect, "image_a")))
        gs_effect_set_texture(param, texture_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "image_b")))
        gs_effect_set_texture(param, texture_b);

    set_color_params(context, *color_a, *color_b, xfade,
                     decks->transitionMode(), has_vid_a, has_vid_b);

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

static obs_properties_t *mixer_properties(void *)
{
    obs_properties_t *props = obs_properties_create();

    obs_property_t *transition = obs_properties_add_list(
        props, "transition", "FX Transition",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(transition, "Linear Fade", 0);
    obs_property_list_add_int(transition, "Horizontal Wipe", 1);
    obs_property_list_add_int(transition, "Additive Blend", 2);
    obs_property_list_add_int(transition, "Hard Cut", 3);

    return props;
}

static void mixer_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "transition", 0);
}

static const char *cue_get_name(void *)
{
    return "OBS VJ/DJ Headphone CUE";
}

static void mixer_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(seconds);
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks)
        return;

    decks->deckA().updateLoop();
    decks->deckB().updateLoop();
}

static void *cue_create(obs_data_t *, obs_source_t *source)
{
    return source;
}

static void cue_destroy(void *)
{
}

void vdj_register_sources(void)
{
    obs_source_info filter_info = {};
    filter_info.id = "vdj_eq_filter";
    filter_info.type = OBS_SOURCE_TYPE_FILTER;
    filter_info.output_flags = OBS_SOURCE_AUDIO;
    filter_info.get_name = filter_get_name;
    filter_info.create = filter_create;
    filter_info.destroy = filter_destroy;
    filter_info.filter_audio = filter_audio;
    obs_register_source_s(&filter_info, sizeof(filter_info));

    obs_source_info cue_info = {};
    cue_info.id = "vdj_headphone_cue";
    cue_info.type = OBS_SOURCE_TYPE_INPUT;
    cue_info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
    cue_info.get_name = cue_get_name;
    cue_info.create = cue_create;
    cue_info.destroy = cue_destroy;
    obs_register_source_s(&cue_info, sizeof(cue_info));

    obs_source_info master = {};
    master.id = "vdj_master_mixer";
    master.type = OBS_SOURCE_TYPE_INPUT;
    master.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE | OBS_SOURCE_DO_NOT_DUPLICATE;
    master.get_name = mixer_get_name;
    master.create = mixer_create;
    master.destroy = mixer_destroy;
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
