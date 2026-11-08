#include "deck-engine.hpp"

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>

#include <cstdint>

// ==========================================================================
// Beagyazott HLSL shader (nem kell hozza kulso .effect fajl vagy data mappa)
// ==========================================================================
static const char *CROSSFADE_EFFECT_SRC = R"(
uniform float4x4 ViewProj;

uniform texture2d image_a;
uniform texture2d image_b;

uniform float transition;
uniform float transition_mode;

uniform float3 tint_a;
uniform float3 tint_b;
uniform float saturation_a;
uniform float saturation_b;

sampler_state image_a_sampler {
    Filter   = Linear;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler_state image_b_sampler {
    Filter   = Linear;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct VertData {
    float4 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

VertData VSDefault(VertData v_in)
{
    VertData v_out;
    v_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
    v_out.uv  = v_in.uv;
    return v_out;
}

float4 apply_color_grade(float4 color, float3 tint, float saturation)
{
    color.rgb *= tint;
    float luminance = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    float3 gray = float3(luminance, luminance, luminance);
    color.rgb = lerp(gray, color.rgb, saturation);
    return color;
}

float4 PSBlend(VertData v_in) : TARGET
{
    float4 a = image_a.Sample(image_a_sampler, v_in.uv);
    float4 b = image_b.Sample(image_b_sampler, v_in.uv);

    a = apply_color_grade(a, tint_a, saturation_a);
    b = apply_color_grade(b, tint_b, saturation_b);

    float t = saturate(transition);

    if (transition_mode < 0.5)
        return lerp(a, b, t);

    if (transition_mode < 1.5) {
        float edge = smoothstep(t - 0.005, t + 0.005, v_in.uv.x);
        return lerp(b, a, edge);
    }

    if (transition_mode < 2.5)
        return float4(saturate(a.rgb + b.rgb * t), lerp(a.a, b.a, t));

    return t < 0.5 ? a : b;
}

technique Draw {
    pass {
        vertex_shader = VSDefault(v_in);
        pixel_shader  = PSBlend(v_in);
    }
}
)";

// ==========================================================================
// 1. Deck Monitor Forrasok (A es B kulon megjelenitesehez a jelenetekben)
// ==========================================================================

struct DeckMonitorData {
    int deck_index = 0;
};

static const char *deck_monitor_get_name(void *type_data)
{
    return reinterpret_cast<intptr_t>(type_data) == 0 ? "VDJ Deck A Monitor"
                                                      : "VDJ Deck B Monitor";
}

static void *deck_monitor_create_a(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    UNUSED_PARAMETER(source);
    auto *data = new DeckMonitorData();
    data->deck_index = 0;
    return data;
}

static void *deck_monitor_create_b(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    UNUSED_PARAMETER(source);
    auto *data = new DeckMonitorData();
    data->deck_index = 1;
    return data;
}

static void deck_monitor_destroy(void *data)
{
    delete static_cast<DeckMonitorData *>(data);
}

static void deck_monitor_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    auto *monitor = static_cast<DeckMonitorData *>(data);
    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    obs_source_t *src = (monitor->deck_index == 0) ? decks->deckA().source()
                                                    : decks->deckB().source();
    if (src) {
        obs_source_video_render(src);
    }
}

static uint32_t deck_monitor_get_width(void *data)
{
    auto *monitor = static_cast<DeckMonitorData *>(data);
    DeckPair *decks = vdj_decks();
    if (!decks)
        return 1920;

    obs_source_t *src = (monitor->deck_index == 0) ? decks->deckA().source()
                                                    : decks->deckB().source();
    return src ? obs_source_get_width(src) : 1920;
}

static uint32_t deck_monitor_get_height(void *data)
{
    auto *monitor = static_cast<DeckMonitorData *>(data);
    DeckPair *decks = vdj_decks();
    if (!decks)
        return 1080;

    obs_source_t *src = (monitor->deck_index == 0) ? decks->deckA().source()
                                                    : decks->deckB().source();
    return src ? obs_source_get_height(src) : 1080;
}

// ==========================================================================
// 2. Master Mixer Forras (Beagyazott HLSL shaderrel kevert kimenet)
// ==========================================================================

struct MixerSourceData {
    obs_source_t *source = nullptr;
    gs_effect_t *effect = nullptr;

    gs_texrender_t *texrender_a = nullptr;
    gs_texrender_t *texrender_b = nullptr;

    gs_eparam_t *param_image_a = nullptr;
    gs_eparam_t *param_image_b = nullptr;
    gs_eparam_t *param_transition = nullptr;
    gs_eparam_t *param_transition_mode = nullptr;
    gs_eparam_t *param_tint_a = nullptr;
    gs_eparam_t *param_tint_b = nullptr;
    gs_eparam_t *param_saturation_a = nullptr;
    gs_eparam_t *param_saturation_b = nullptr;
};

static const char *mixer_get_name(void *type_data)
{
    UNUSED_PARAMETER(type_data);
    return "VDJ Master Output";
}

static void *mixer_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    auto *data = new MixerSourceData();
    data->source = source;

    obs_enter_graphics();

    char *errors = nullptr;
    data->effect = gs_effect_create(CROSSFADE_EFFECT_SRC, "crossfade_embedded", &errors);
    if (!data->effect) {
        blog(LOG_ERROR, "[obs-vj-mixer] Failed to create embedded shader: %s", errors ? errors : "unknown");
        bfree(errors);
    } else {
        data->param_image_a = gs_effect_get_param_by_name(data->effect, "image_a");
        data->param_image_b = gs_effect_get_param_by_name(data->effect, "image_b");
        data->param_transition = gs_effect_get_param_by_name(data->effect, "transition");
        data->param_transition_mode = gs_effect_get_param_by_name(data->effect, "transition_mode");
        data->param_tint_a = gs_effect_get_param_by_name(data->effect, "tint_a");
        data->param_tint_b = gs_effect_get_param_by_name(data->effect, "tint_b");
        data->param_saturation_a = gs_effect_get_param_by_name(data->effect, "saturation_a");
        data->param_saturation_b = gs_effect_get_param_by_name(data->effect, "saturation_b");
    }

    data->texrender_a = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    data->texrender_b = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

    obs_leave_graphics();
    return data;
}

static void mixer_destroy(void *data)
{
    auto *mixer = static_cast<MixerSourceData *>(data);
    if (!mixer)
        return;

    obs_enter_graphics();
    if (mixer->texrender_a)
        gs_texrender_destroy(mixer->texrender_a);
    if (mixer->texrender_b)
        gs_texrender_destroy(mixer->texrender_b);
    if (mixer->effect)
        gs_effect_destroy(mixer->effect);
    obs_leave_graphics();

    delete mixer;
}

static void mixer_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    auto *mixer = static_cast<MixerSourceData *>(data);
    DeckPair *decks = vdj_decks();
    if (!decks || !mixer->effect)
        return;

    obs_source_t *src_a = decks->deckA().source();
    obs_source_t *src_b = decks->deckB().source();

    const uint32_t cx = 1920;
    const uint32_t cy = 1080;

    // 1. Deck A lerenderelese texturaba
    gs_texrender_reset(mixer->texrender_a);
    if (gs_texrender_begin(mixer->texrender_a, cx, cy)) {
        struct vec4 clear_color;
        vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
        gs_ortho(0.0f, static_cast<float>(cx), 0.0f, static_cast<float>(cy), -100.0f, 100.0f);

        if (src_a) {
            obs_source_video_render(src_a);
        }
        gs_texrender_end(mixer->texrender_a);
    }

    // 2. Deck B lerenderelese texturaba
    gs_texrender_reset(mixer->texrender_b);
    if (gs_texrender_begin(mixer->texrender_b, cx, cy)) {
        struct vec4 clear_color;
        vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
        gs_ortho(0.0f, static_cast<float>(cx), 0.0f, static_cast<float>(cy), -100.0f, 100.0f);

        if (src_b) {
            obs_source_video_render(src_b);
        }
        gs_texrender_end(mixer->texrender_b);
    }

    gs_texture_t *tex_a = gs_texrender_get_texture(mixer->texrender_a);
    gs_texture_t *tex_b = gs_texrender_get_texture(mixer->texrender_b);
    if (!tex_a || !tex_b)
        return;

    // 3. Shader parameterek atadasa
    gs_effect_set_texture(mixer->param_image_a, tex_a);
    gs_effect_set_texture(mixer->param_image_b, tex_b);
    gs_effect_set_float(mixer->param_transition, static_cast<float>(decks->crossfader()));
    gs_effect_set_float(mixer->param_transition_mode, static_cast<float>(decks->transitionMode()));

    struct vec3 tint_a, tint_b;
    vec3_set(&tint_a, decks->deckA().red(), decks->deckA().green(), decks->deckA().blue());
    vec3_set(&tint_b, decks->deckB().red(), decks->deckB().green(), decks->deckB().blue());

    gs_effect_set_vec3(mixer->param_tint_a, &tint_a);
    gs_effect_set_vec3(mixer->param_tint_b, &tint_b);
    gs_effect_set_float(mixer->param_saturation_a, decks->deckA().saturation());
    gs_effect_set_float(mixer->param_saturation_b, decks->deckB().saturation());

    // 4. Kevert vegeredmeny kirajzolasa
    while (gs_effect_loop(mixer->effect, "Draw")) {
        gs_draw_sprite(nullptr, 0, cx, cy);
    }
}

static uint32_t mixer_get_width(void *data)
{
    UNUSED_PARAMETER(data);
    return 1920;
}

static uint32_t mixer_get_height(void *data)
{
    UNUSED_PARAMETER(data);
    return 1080;
}

// ==========================================================================
// 3. OBS Forrasok Regisztracioja
// ==========================================================================

static struct obs_source_info vdj_deck_a_info = {};
static struct obs_source_info vdj_deck_b_info = {};
static struct obs_source_info vdj_master_mixer_info = {};

void vdj_register_sources()
{
    // Deck A Monitor Source
    vdj_deck_a_info.id = "vdj_deck_a_source";
    vdj_deck_a_info.type = OBS_SOURCE_TYPE_INPUT;
    vdj_deck_a_info.output_flags = OBS_SOURCE_VIDEO;
    vdj_deck_a_info.get_name = deck_monitor_get_name;
    vdj_deck_a_info.create = deck_monitor_create_a;
    vdj_deck_a_info.destroy = deck_monitor_destroy;
    vdj_deck_a_info.video_render = deck_monitor_video_render;
    vdj_deck_a_info.get_width = deck_monitor_get_width;
    vdj_deck_a_info.get_height = deck_monitor_get_height;
    vdj_deck_a_info.type_data = reinterpret_cast<void *>(static_cast<intptr_t>(0));
    obs_register_source(&vdj_deck_a_info);

    // Deck B Monitor Source
    vdj_deck_b_info.id = "vdj_deck_b_source";
    vdj_deck_b_info.type = OBS_SOURCE_TYPE_INPUT;
    vdj_deck_b_info.output_flags = OBS_SOURCE_VIDEO;
    vdj_deck_b_info.get_name = deck_monitor_get_name;
    vdj_deck_b_info.create = deck_monitor_create_b;
    vdj_deck_b_info.destroy = deck_monitor_destroy;
    vdj_deck_b_info.video_render = deck_monitor_video_render;
    vdj_deck_b_info.get_width = deck_monitor_get_width;
    vdj_deck_b_info.get_height = deck_monitor_get_height;
    vdj_deck_b_info.type_data = reinterpret_cast<void *>(static_cast<intptr_t>(1));
    obs_register_source(&vdj_deck_b_info);

    // Master Output Mixer Source
    vdj_master_mixer_info.id = "vdj_master_mixer";
    vdj_master_mixer_info.type = OBS_SOURCE_TYPE_INPUT;
    vdj_master_mixer_info.output_flags = OBS_SOURCE_VIDEO;
    vdj_master_mixer_info.get_name = mixer_get_name;
    vdj_master_mixer_info.create = mixer_create;
    vdj_master_mixer_info.destroy = mixer_destroy;
    vdj_master_mixer_info.video_render = mixer_video_render;
    vdj_master_mixer_info.get_width = mixer_get_width;
    vdj_master_mixer_info.get_height = mixer_get_height;
    obs_register_source(&vdj_master_mixer_info);
}
