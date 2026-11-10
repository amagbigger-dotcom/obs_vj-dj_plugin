#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/effect.h>

#include <cstring>

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
uniform texture2d image_a;
uniform texture2d image_b;
uniform float4 gains_a;
uniform float4 gains_b;
uniform float saturation_a;
uniform float saturation_b;
uniform float crossfade;
uniform float transition_mode;

sampler_state sampler_a { Filter = Linear; AddressU = Clamp; AddressV = Clamp; };
sampler_state sampler_b { Filter = Linear; AddressU = Clamp; AddressV = Clamp; };

struct VertData { float4 pos : POSITION; float2 uv : TEXCOORD0; };
VertData VSDefault(VertData v) { return v; }

float4 colorize(float4 color, float4 gains, float saturation)
{
    color.rgb *= gains.rgb;
    float luma = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    color.rgb = lerp(luma.xxx, color.rgb, saturation);
    return saturate(color);
}

float4 PS(VertData v) : TARGET
{
    float4 a = colorize(image_a.Sample(sampler_a, v.uv), gains_a, saturation_a);
    float4 b = colorize(image_b.Sample(sampler_b, v.uv), gains_b, saturation_b);
    if (transition_mode > 2.5) return crossfade < 0.5 ? a : b;
    if (transition_mode > 0.5 && transition_mode < 1.5) return v.uv.x < crossfade ? b : a;
    if (transition_mode > 1.5 && transition_mode < 2.5) return saturate(a * (1.0 - crossfade) + b * crossfade);
    return lerp(a, b, crossfade);
}

technique Draw { pass { vertex_shader = VSDefault(v); pixel_shader = PS(v); } }
)";

static void ensure_resources(MixerSource *context)
{
    if (!context->effect)
        context->effect = gs_effect_create(MIXER_EFFECT, nullptr, nullptr);
    if (!context->deck_a)
        context->deck_a = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    if (!context->deck_b)
        context->deck_b = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
}

static bool render_to_texture(gs_texrender_t *target, obs_source_t *source,
                              uint32_t width, uint32_t height)
{
    if (!target || !source || !width || !height)
        return false;
    if (!gs_texrender_begin(target, width, height))
        return false;
    gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
    vec4 clear = {};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
    obs_source_video_render(source);
    gs_texrender_end(target);
    return true;
}

static void set_color_params(MixerSource *context, const DeckPlayer &deck_a,
                             const DeckPlayer &deck_b, float crossfade,
                             int transition_mode)
{
    vec4 gains_a, gains_b;
    vec4_set(&gains_a, deck_a.red(), deck_a.green(), deck_a.blue(), 1.0f);
    vec4_set(&gains_b, deck_b.red(), deck_b.green(), deck_b.blue(), 1.0f);
    gs_eparam_t *param = gs_effect_get_param_by_name(context->effect, "gains_a");
    if (param) gs_effect_set_vec4(param, &gains_a);
    param = gs_effect_get_param_by_name(context->effect, "gains_b");
    if (param) gs_effect_set_vec4(param, &gains_b);
    param = gs_effect_get_param_by_name(context->effect, "saturation_a");
    if (param) gs_effect_set_float(param, deck_a.saturation());
    param = gs_effect_get_param_by_name(context->effect, "saturation_b");
    if (param) gs_effect_set_float(param, deck_b.saturation());
    param = gs_effect_get_param_by_name(context->effect, "crossfade");
    if (param) gs_effect_set_float(param, crossfade);
    param = gs_effect_get_param_by_name(context->effect, "transition_mode");
    if (param) gs_effect_set_float(param, static_cast<float>(transition_mode));
}

static const char *mixer_get_name(void *) { return "Traktor S2 VJ Mixer Source"; }
static void *mixer_create(obs_data_t *, obs_source_t *source)
{
    auto *context = new MixerSource();
    context->source = source;
    return context;
}

static void mixer_destroy(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context) return;
    obs_enter_graphics();
    if (context->deck_a) gs_texrender_destroy(context->deck_a);
    if (context->deck_b) gs_texrender_destroy(context->deck_b);
    if (context->effect) gs_effect_destroy(context->effect);
    obs_leave_graphics();
    delete context;
}

static void clear_output()
{
    vec4 clear = {};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
}

static void draw_mixer(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks || !context->effect) return;

    const uint32_t width = obs_source_get_base_width(context->source);
    const uint32_t height = obs_source_get_base_height(context->source);
    if (!width || !height) return;

    DeckPlayer &deck_a = decks->deckA();
    DeckPlayer &deck_b = decks->deckB();
    deck_a.updateLoop();
    deck_b.updateLoop();

    // An unloaded ffmpeg_source still exists, but renders a black frame.
    // Treat it as absent so a single loaded deck fills the master output.
    const bool has_a = deck_a.hasMedia();
    const bool has_b = deck_b.hasMedia();
    const bool rendered_a = has_a && render_to_texture(context->deck_a, deck_a.source(), width, height);
    const bool rendered_b = has_b && render_to_texture(context->deck_b, deck_b.source(), width, height);
    gs_texture_t *texture_a = rendered_a ? gs_texrender_get_texture(context->deck_a) : nullptr;
    gs_texture_t *texture_b = rendered_b ? gs_texrender_get_texture(context->deck_b) : nullptr;

    if (!texture_a && !texture_b) {
        clear_output();
        return;
    }

    const DeckPlayer *color_a = &deck_a;
    const DeckPlayer *color_b = &deck_b;
    float xfade = static_cast<float>(decks->crossfader());
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

    gs_eparam_t *param = gs_effect_get_param_by_name(context->effect, "image_a");
    if (param) gs_effect_set_texture(param, texture_a);
    param = gs_effect_get_param_by_name(context->effect, "image_b");
    if (param) gs_effect_set_texture(param, texture_b);
    set_color_params(context, *color_a, *color_b, xfade, decks->transitionMode());

    while (gs_effect_loop(context->effect, "Draw"))
        gs_draw_sprite(texture_a, 0, width, height);
}

static void mixer_video_render(void *data, gs_effect_t *)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context || !vdj_decks()) return;
    ensure_resources(context);
    if (!context->effect || !context->deck_a || !context->deck_b) return;
    draw_mixer(context);
}

static uint32_t mixer_get_width(void *) { return 1920; }
static uint32_t mixer_get_height(void *) { return 1080; }
static const char *monitor_get_name(void *) { return "Traktor S2 Deck Monitor"; }

static void *monitor_create(obs_data_t *, obs_source_t *source)
{
    auto *context = new MixerSource();
    context->source = source;
    const char *id = obs_source_get_id(source);
    context->channel = (id && std::strcmp(id, "vdj_deck_b_source") == 0) ? 1 : 0;
    return context;
}

static void monitor_video_render(void *data, gs_effect_t *)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks) return;
    ensure_resources(context);
    if (!context->effect || !context->deck_a) return;

    DeckPlayer &deck = context->channel == 0 ? decks->deckA() : decks->deckB();
    if (!deck.hasMedia()) { clear_output(); return; }
    deck.updateLoop();
    const uint32_t width = obs_source_get_base_width(context->source);
    const uint32_t height = obs_source_get_base_height(context->source);
    if (!render_to_texture(context->deck_a, deck.source(), width, height)) return;
    gs_texture_t *texture = gs_texrender_get_texture(context->deck_a);
    if (!texture) return;
    gs_eparam_t *param = gs_effect_get_param_by_name(context->effect, "image_a");
    if (param) gs_effect_set_texture(param, texture);
    param = gs_effect_get_param_by_name(context->effect, "image_b");
    if (param) gs_effect_set_texture(param, texture);
    set_color_params(context, deck, deck, 0.0f, 0);
    while (gs_effect_loop(context->effect, "Draw"))
        gs_draw_sprite(texture, 0, width, height);
}

static obs_properties_t *mixer_properties(void *)
{
    obs_properties_t *props = obs_properties_create();
    obs_property_t *transition = obs_properties_add_list(props, "transition", "FX Transition", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(transition, "Linear Fade", 0);
    obs_property_list_add_int(transition, "Horizontal Wipe", 1);
    obs_property_list_add_int(transition, "Additive Blend", 2);
    obs_property_list_add_int(transition, "Hard Cut", 3);
    return props;
}
static void mixer_defaults(obs_data_t *settings) { obs_data_set_default_int(settings, "transition", 0); }

void vdj_register_sources(void)
{
    obs_source_info master = {};
    master.id = "vdj_master_mixer";
    master.type = OBS_SOURCE_TYPE_INPUT;
    master.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE;
    master.get_name = mixer_get_name;
    master.create = mixer_create;
    master.destroy = mixer_destroy;
    master.video_render = mixer_video_render;
    master.get_width = mixer_get_width;
    master.get_height = mixer_get_height;
    master.get_properties = mixer_properties;
    master.get_defaults = mixer_defaults;
    obs_register_source_s(&master, sizeof(master));

    obs_source_info deck_a = {};
    deck_a.id = "vdj_deck_a_source";
    deck_a.type = OBS_SOURCE_TYPE_INPUT;
    deck_a.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
    deck_a.get_name = monitor_get_name;
    deck_a.create = monitor_create;
    deck_a.destroy = mixer_destroy;
    deck_a.video_render = monitor_video_render;
    deck_a.get_width = mixer_get_width;
    deck_a.get_height = mixer_get_height;
    obs_register_source_s(&deck_a, sizeof(deck_a));
    obs_source_info deck_b = deck_a;
    deck_b.id = "vdj_deck_b_source";
    obs_register_source_s(&deck_b, sizeof(deck_b));
}

} // extern "C"
