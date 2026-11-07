#include "deck-engine.hpp"

#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <obs-module.h>

#include <algorithm>
#include <string>

// A shader kód közvetlenül a C++ kódban lakik, nem kell hozzá külön .effect fájl és data mappa!
static const char *CROSSFADE_EFFECT_STRING = R"(
uniform float4x4 ViewProj;
uniform texture2d image_a;
uniform texture2d image_b;
uniform float crossfader;
uniform int transition_mode;
uniform float4 color_a;
uniform float4 color_b;

sampler_state def_sampler {
    Filter    = Linear;
    AddressU  = Clamp;
    AddressV  = Clamp;
};

struct VertInOut {
    float4 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

VertInOut VSDefault(VertInOut vert_in)
{
    VertInOut vert_out;
    vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
    vert_out.uv  = vert_in.uv;
    return vert_out;
}

float3 apply_color(float3 rgb, float4 col)
{
    rgb *= col.rgb;
    float gray = dot(rgb, float3(0.299, 0.587, 0.114));
    return lerp(float3(gray, gray, gray), rgb, col.a);
}

float4 PSDraw(VertInOut vert_in) : TARGET
{
    float4 a = image_a.Sample(def_sampler, vert_in.uv);
    float4 b = image_b.Sample(def_sampler, vert_in.uv);

    a.rgb = apply_color(a.rgb, color_a);
    b.rgb = apply_color(b.rgb, color_b);

    float t = clamp(crossfader, 0.0, 1.0);

    if (transition_mode == 1) {
        float edge = smoothstep(t - 0.01, t + 0.01, vert_in.uv.x);
        return lerp(b, a, edge);
    } else if (transition_mode == 2) {
        return saturate(a * (1.0 - t) + b * t + (a * b * 0.5));
    } else if (transition_mode == 3) {
        return (t < 0.5) ? a : b;
    }

    return lerp(a, b, t);
}

technique Draw
{
    pass
    {
        vertex_shader = VSDefault(vert_in);
        pixel_shader  = PSDraw(vert_in);
    }
}
)";

struct VdjMasterSource {
    obs_source_t *source = nullptr;
    gs_effect_t *effect = nullptr;
    gs_eparam_t *param_image_a = nullptr;
    gs_eparam_t *param_image_b = nullptr;
    gs_eparam_t *param_crossfader = nullptr;
    gs_eparam_t *param_transition_mode = nullptr;
    gs_eparam_t *param_color_a = nullptr;
    gs_eparam_t *param_color_b = nullptr;
};

static const char *vdj_master_get_name(void *)
{
    return "VDJ Master Output";
}

static void *vdj_master_create(obs_data_t *, obs_source_t *source)
{
    auto *context = new VdjMasterSource();
    context->source = source;

    obs_enter_graphics();
    char *errors = nullptr;
    context->effect = gs_effect_create(CROSSFADE_EFFECT_STRING, "crossfade_internal", &errors);
    if (!context->effect) {
        blog(LOG_ERROR, "[obs-vj-mixer] Failed to compile internal shader: %s", errors ? errors : "unknown");
        bfree(errors);
    } else {
        context->param_image_a = gs_effect_get_param_by_name(context->effect, "image_a");
        context->param_image_b = gs_effect_get_param_by_name(context->effect, "image_b");
        context->param_crossfader = gs_effect_get_param_by_name(context->effect, "crossfader");
        context->param_transition_mode = gs_effect_get_param_by_name(context->effect, "transition_mode");
        context->param_color_a = gs_effect_get_param_by_name(context->effect, "color_a");
        context->param_color_b = gs_effect_get_param_by_name(context->effect, "color_b");
    }
    obs_leave_graphics();

    return context;
}

static void vdj_master_destroy(void *data)
{
    auto *context = static_cast<VdjMasterSource *>(data);
    if (!context) return;

    if (context->effect) {
        obs_enter_graphics();
        gs_effect_destroy(context->effect);
        obs_leave_graphics();
    }
    delete context;
}

static void vdj_master_video_render(void *data, gs_effect_t *)
{
    auto *context = static_cast<VdjMasterSource *>(data);
    if (!context || !context->effect) return;

    DeckPair *decks = vdj_decks();
    if (!decks) return;

    obs_source_t *src_a = decks->deckA().source();
    obs_source_t *src_b = decks->deckB().source();

    gs_texture_t *tex_a = src_a ? obs_source_get_texture(src_a) : nullptr;
    gs_texture_t *tex_b = src_b ? obs_source_get_texture(src_b) : nullptr;

    const float xfade = static_cast<float>(decks->crossfader());
    const int mode = decks->transitionMode();

    struct vec4 col_a = {decks->deckA().red(), decks->deckA().green(), decks->deckA().blue(), decks->deckA().saturation()};
    struct vec4 col_b = {decks->deckB().red(), decks->deckB().green(), decks->deckB().blue(), decks->deckB().saturation()};

    gs_effect_set_texture(context->param_image_a, tex_a);
    gs_effect_set_texture(context->param_image_b, tex_b);
    gs_effect_set_float(context->param_crossfader, xfade);
    gs_effect_set_int(context->param_transition_mode, mode);
    gs_effect_set_vec4(context->param_color_a, &col_a);
    gs_effect_set_vec4(context->param_color_b, &col_b);

    while (gs_effect_loop(context->effect, "Draw")) {
        gs_draw_sprite(nullptr, 0, 1920, 1080);
    }
}

static uint32_t vdj_master_get_width(void *) { return 1920; }
static uint32_t vdj_master_get_height(void *) { return 1080; }

struct obs_source_info vdj_master_source_info = {
    .id             = "vdj_master_output",
    .type           = OBS_SOURCE_TYPE_INPUT,
    .output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
    .get_name       = vdj_master_get_name,
    .create         = vdj_master_create,
    .destroy        = vdj_master_destroy,
    .video_render   = vdj_master_video_render,
    .get_width      = vdj_master_get_width,
    .get_height     = vdj_master_get_height,
};

void vdj_register_sources()
{
    obs_register_source(&vdj_master_source_info);
}
