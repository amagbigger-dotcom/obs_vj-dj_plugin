#include "deck-engine.hpp"

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>

#include <cstdint>
#include <string>

// ==========================================================================
// Beágyazott HLSL shader (HI=Red, MID=Green, LOW=Blue, FILTER=Saturation)
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
// Segédfüggvények: Vászonméret-illesztés és Hang + Crossfader szinkron
// ==========================================================================

static void get_canvas_size(uint32_t *out_w, uint32_t *out_h)
{
    struct obs_video_info ovi;
    if (obs_get_video_info(&ovi) && ovi.base_width > 0 && ovi.base_height > 0) {
        *out_w = ovi.base_width;
        *out_h = ovi.base_height;
    } else {
        *out_w = 1920;
        *out_h = 1080;
    }
}

static void render_source_fitted(obs_source_t *src, uint32_t target_w, uint32_t target_h)
{
    if (!src)
        return;

    const uint32_t src_w = obs_source_get_width(src);
    const uint32_t src_h = obs_source_get_height(src);
    if (src_w == 0 || src_h == 0 || target_w == 0 || target_h == 0) {
        obs_source_video_render(src);
        return;
    }

    const float scale_x = static_cast<float>(target_w) / static_cast<float>(src_w);
    const float scale_y = static_cast<float>(target_h) / static_cast<float>(src_h);
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;

    const float draw_w = static_cast<float>(src_w) * scale;
    const float draw_h = static_cast<float>(src_h) * scale;
    const float off_x = (static_cast<float>(target_w) - draw_w) * 0.5f;
    const float off_y = (static_cast<float>(target_h) - draw_h) * 0.5f;

    gs_matrix_push();
    gs_matrix_translate3f(off_x, off_y, 0.0f);
    gs_matrix_scale3f(scale, scale, 1.0f);
    obs_source_video_render(src);
    gs_matrix_pop();
}

static void sync_decks_audio_state()
{
    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    obs_source_t *src_a = decks->deckA().source();
    obs_source_t *src_b = decks->deckB().source();

    // Crossfader hanggörbe (0.0 = Deck A, 0.5 = Mindkettő 100%, 1.0 = Deck B)
    const double xfade = decks->crossfader();
    const float gain_a = static_cast<float>(xfade <= 0.5 ? 1.0 : (1.0 - xfade) * 2.0);
    const float gain_b = static_cast<float>(xfade >= 0.5 ? 1.0 : xfade * 2.0);

    if (src_a) {
        if (obs_source_get_monitoring_type(src_a) != OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT) {
            obs_source_set_monitoring_type(src_a, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
        }
        obs_source_set_volume(src_a, decks->deckA().volume() * gain_a);
    }

    if (src_b) {
        if (obs_source_get_monitoring_type(src_b) != OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT) {
            obs_source_set_monitoring_type(src_b, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
        }
        obs_source_set_volume(src_b, decks->deckB().volume() * gain_b);
    }
}

// ==========================================================================
// 1. Deck Monitor Források (VDJ Deck A Monitor & VDJ Deck B Monitor)
// ==========================================================================

struct DeckMonitorData {
    int deck_index = 0;
    std::string loaded_file;
};

static const char *deck_monitor_get_name(void *type_data)
{
    return reinterpret_cast<intptr_t>(type_data) == 0 ? "VDJ Deck A Monitor"
                                                      : "VDJ Deck B Monitor";
}

static void deck_monitor_update(void *data, obs_data_t *settings)
{
    auto *monitor = static_cast<DeckMonitorData *>(data);
    DeckPair *decks = vdj_decks();
    if (!monitor || !decks)
        return;

    DeckPlayer &deck = (monitor->deck_index == 0) ? decks->deckA() : decks->deckB();

    const char *file_path = obs_data_get_string(settings, "video_file");
    if (file_path && *file_path && monitor->loaded_file != file_path) {
        monitor->loaded_file = file_path;
        deck.loadFile(QString::fromUtf8(file_path));
    }

    const bool loop = obs_data_get_bool(settings, "looping");
    if (deck.isLooping() != loop) {
        deck.setLooping(loop);
    }

    const int speed = static_cast<int>(obs_data_get_int(settings, "speed"));
    if (speed >= 50 && speed <= 150 && deck.speed() != speed) {
        deck.setSpeed(speed);
    }

    deck.setRed(static_cast<float>(obs_data_get_double(settings, "hi_red")));
    deck.setGreen(static_cast<float>(obs_data_get_double(settings, "mid_green")));
    deck.setBlue(static_cast<float>(obs_data_get_double(settings, "low_blue")));
    deck.setSaturation(static_cast<float>(obs_data_get_double(settings, "filter_sat")));
    deck.setVolume(static_cast<float>(obs_data_get_double(settings, "volume")));

    sync_decks_audio_state();
}

static void deck_monitor_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, "looping", true);
    obs_data_set_default_int(settings, "speed", 100);
    obs_data_set_default_double(settings, "hi_red", 1.0);
    obs_data_set_default_double(settings, "mid_green", 1.0);
    obs_data_set_default_double(settings, "low_blue", 1.0);
    obs_data_set_default_double(settings, "filter_sat", 1.0);
    obs_data_set_default_double(settings, "volume", 1.0);
}

static bool deck_play_pause_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    auto *monitor = static_cast<DeckMonitorData *>(data);
    DeckPair *decks = vdj_decks();
    if (monitor && decks) {
        DeckPlayer &deck = (monitor->deck_index == 0) ? decks->deckA() : decks->deckB();
        deck.togglePlayback();
    }
    return false;
}

static bool deck_cue_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    auto *monitor = static_cast<DeckMonitorData *>(data);
    DeckPair *decks = vdj_decks();
    if (monitor && decks) {
        DeckPlayer &deck = (monitor->deck_index == 0) ? decks->deckA() : decks->deckB();
        deck.cue();
    }
    return false;
}

static obs_properties_t *deck_monitor_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();

    obs_properties_add_path(
        props, "video_file", "Videófájl (LOAD)", OBS_PATH_FILE,
        "Video/Audio (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All (*.*)", nullptr);

    obs_properties_add_button2(props, "play_pause_btn", "PLAY / PAUSE", deck_play_pause_clicked, data);
    obs_properties_add_button2(props, "cue_btn", "CUE", deck_cue_clicked, data);
    obs_properties_add_bool(props, "looping", "AUTO LOOP (Végtelenítés)");
    obs_properties_add_int_slider(props, "speed", "TEMPO / PITCH (%)", 50, 150, 1);

    obs_properties_add_float_slider(props, "hi_red", "HI EQ -> Piros (Red)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "mid_green", "MID EQ -> Zöld (Green)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "low_blue", "LOW EQ -> Kék (Blue)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "filter_sat", "FILTER -> Szaturáció", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "volume", "Csatorna Hangerő (VOL)", 0.0, 1.0, 0.01);

    return props;
}

static void *deck_monitor_create_a(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(source);
    auto *data = new DeckMonitorData();
    data->deck_index = 0;
    deck_monitor_update(data, settings);
    return data;
}

static void *deck_monitor_create_b(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(source);
    auto *data = new DeckMonitorData();
    data->deck_index = 1;
    deck_monitor_update(data, settings);
    return data;
}

static void deck_monitor_destroy(void *data)
{
    delete static_cast<DeckMonitorData *>(data);
}

static void deck_monitor_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(seconds);
    sync_decks_audio_state();
}

static void deck_monitor_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    auto *monitor = static_cast<DeckMonitorData *>(data);
    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    uint32_t cx = 1920, cy = 1080;
    get_canvas_size(&cx, &cy);

    obs_source_t *src = (monitor->deck_index == 0) ? decks->deckA().source()
                                                    : decks->deckB().source();
    render_source_fitted(src, cx, cy);
}

static uint32_t deck_monitor_get_width(void *data)
{
    UNUSED_PARAMETER(data);
    uint32_t cx = 1920, cy = 1080;
    get_canvas_size(&cx, &cy);
    return cx;
}

static uint32_t deck_monitor_get_height(void *data)
{
    UNUSED_PARAMETER(data);
    uint32_t cx = 1920, cy = 1080;
    get_canvas_size(&cx, &cy);
    return cy;
}

// ==========================================================================
// 2. Master Mixer Forrás (Kép + Hang + Crossfader + Shader)
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

    std::string loaded_file_a;
    std::string loaded_file_b;
};

static const char *mixer_get_name(void *type_data)
{
    UNUSED_PARAMETER(type_data);
    return "VDJ Master Output";
}

static void mixer_update(void *data, obs_data_t *settings)
{
    auto *mixer = static_cast<MixerSourceData *>(data);
    DeckPair *decks = vdj_decks();
    if (!mixer || !decks)
        return;

    const char *file_a = obs_data_get_string(settings, "file_a");
    if (file_a && *file_a && mixer->loaded_file_a != file_a) {
        mixer->loaded_file_a = file_a;
        decks->deckA().loadFile(QString::fromUtf8(file_a));
    }

    const char *file_b = obs_data_get_string(settings, "file_b");
    if (file_b && *file_b && mixer->loaded_file_b != file_b) {
        mixer->loaded_file_b = file_b;
        decks->deckB().loadFile(QString::fromUtf8(file_b));
    }

    decks->setCrossfader(obs_data_get_double(settings, "crossfader"));
    decks->setTransitionMode(static_cast<int>(obs_data_get_int(settings, "transition_mode")));

    decks->deckA().setRed(static_cast<float>(obs_data_get_double(settings, "a_hi_red")));
    decks->deckA().setGreen(static_cast<float>(obs_data_get_double(settings, "a_mid_green")));
    decks->deckA().setBlue(static_cast<float>(obs_data_get_double(settings, "a_low_blue")));
    decks->deckA().setSaturation(static_cast<float>(obs_data_get_double(settings, "a_filter_sat")));

    decks->deckB().setRed(static_cast<float>(obs_data_get_double(settings, "b_hi_red")));
    decks->deckB().setGreen(static_cast<float>(obs_data_get_double(settings, "b_mid_green")));
    decks->deckB().setBlue(static_cast<float>(obs_data_get_double(settings, "b_low_blue")));
    decks->deckB().setSaturation(static_cast<float>(obs_data_get_double(settings, "b_filter_sat")));

    sync_decks_audio_state();
}

static void mixer_defaults(obs_data_t *settings)
{
    obs_data_set_default_double(settings, "crossfader", 0.5);
    obs_data_set_default_int(settings, "transition_mode", 0);

    obs_data_set_default_double(settings, "a_hi_red", 1.0);
    obs_data_set_default_double(settings, "a_mid_green", 1.0);
    obs_data_set_default_double(settings, "a_low_blue", 1.0);
    obs_data_set_default_double(settings, "a_filter_sat", 1.0);

    obs_data_set_default_double(settings, "b_hi_red", 1.0);
    obs_data_set_default_double(settings, "b_mid_green", 1.0);
    obs_data_set_default_double(settings, "b_low_blue", 1.0);
    obs_data_set_default_double(settings, "b_filter_sat", 1.0);
}

static bool master_play_a_clicked(obs_properties_t *p, obs_property_t *prop, void *data)
{
    UNUSED_PARAMETER(p); UNUSED_PARAMETER(prop); UNUSED_PARAMETER(data);
    if (vdj_decks()) vdj_decks()->deckA().togglePlayback();
    return false;
}

static bool master_play_b_clicked(obs_properties_t *p, obs_property_t *prop, void *data)
{
    UNUSED_PARAMETER(p); UNUSED_PARAMETER(prop); UNUSED_PARAMETER(data);
    if (vdj_decks()) vdj_decks()->deckB().togglePlayback();
    return false;
}

static obs_properties_t *mixer_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();

    obs_properties_add_float_slider(props, "crossfader", "MASTER CROSSFADER (A <-> B)", 0.0, 1.0, 0.01);

    obs_property_t *trans = obs_properties_add_list(
        props, "transition_mode", "Átmenet (FX Transition)",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(trans, "Linear Fade", 0);
    obs_property_list_add_int(trans, "Horizontal Wipe", 1);
    obs_property_list_add_int(trans, "Additive Blend", 2);
    obs_property_list_add_int(trans, "Hard Cut", 3);

    obs_properties_add_path(
        props, "file_a", "DECK A - Videófájl", OBS_PATH_FILE,
        "Video/Audio (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All (*.*)", nullptr);
    obs_properties_add_button2(props, "play_a", "DECK A - PLAY / PAUSE", master_play_a_clicked, data);
    obs_properties_add_float_slider(props, "a_hi_red", "DECK A - HI (Red / Piros)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "a_mid_green", "DECK A - MID (Green / Zöld)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "a_low_blue", "DECK A - LOW (Blue / Kék)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "a_filter_sat", "DECK A - FILTER (Szaturáció)", 0.0, 2.0, 0.01);

    obs_properties_add_path(
        props, "file_b", "DECK B - Videófájl", OBS_PATH_FILE,
        "Video/Audio (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All (*.*)", nullptr);
    obs_properties_add_button2(props, "play_b", "DECK B - PLAY / PAUSE", master_play_b_clicked, data);
    obs_properties_add_float_slider(props, "b_hi_red", "DECK B - HI (Red / Piros)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "b_mid_green", "DECK B - MID (Green / Zöld)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "b_low_blue", "DECK B - LOW (Blue / Kék)", 0.0, 2.0, 0.01);
    obs_properties_add_float_slider(props, "b_filter_sat", "DECK B - FILTER (Szaturáció)", 0.0, 2.0, 0.01);

    return props;
}

static void *mixer_create(obs_data_t *settings, obs_source_t *source)
{
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

    DeckPair *decks = vdj_decks();
    if (decks) {
        if (decks->deckA().source())
            obs_source_add_active_child(source, decks->deckA().source());
        if (decks->deckB().source())
            obs_source_add_active_child(source, decks->deckB().source());
    }

    obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
    mixer_update(data, settings);
    return data;
}

static void mixer_destroy(void *data)
{
    auto *mixer = static_cast<MixerSourceData *>(data);
    if (!mixer)
        return;

    DeckPair *decks = vdj_decks();
    if (decks && mixer->source) {
        if (decks->deckA().source())
            obs_source_remove_active_child(mixer->source, decks->deckA().source());
        if (decks->deckB().source())
            obs_source_remove_active_child(mixer->source, decks->deckB().source());
    }

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

static void mixer_enum_active_sources(void *data, obs_source_enum_proc_t enum_callback, void *param)
{
    auto *mixer = static_cast<MixerSourceData *>(data);
    DeckPair *decks = vdj_decks();
    if (!mixer || !decks)
        return;

    if (decks->deckA().source())
        enum_callback(mixer->source, decks->deckA().source(), param);
    if (decks->deckB().source())
        enum_callback(mixer->source, decks->deckB().source(), param);
}

static bool mixer_audio_render(void *data, uint64_t *ts_out,
                               struct obs_source_audio_mix *audio_output,
                               uint32_t mixers, size_t channels,
                               size_t sample_rate)
{
    UNUSED_PARAMETER(data);
    DeckPair *decks = vdj_decks();
    if (!decks)
        return false;

    obs_source_t *src_a = decks->deckA().source();
    obs_source_t *src_b = decks->deckB().source();

    uint64_t ts_a = 0;
    uint64_t ts_b = 0;

    if (src_a && !obs_source_audio_pending(src_a))
        ts_a = obs_source_get_audio_timestamp(src_a);
    if (src_b && !obs_source_audio_pending(src_b))
        ts_b = obs_source_get_audio_timestamp(src_b);

    if (!ts_a && !ts_b)
        return false;

    uint64_t min_ts = 0;
    if (ts_a && ts_b)
        min_ts = (ts_a < ts_b) ? ts_a : ts_b;
    else
        min_ts = ts_a ? ts_a : ts_b;

    if (ts_a) {
        struct obs_source_audio_mix child_audio;
        obs_source_get_audio_mix(src_a, &child_audio);
        const size_t diff = static_cast<size_t>((ts_a - min_ts) * static_cast<uint64_t>(sample_rate) / 1000000000ULL);
        if (diff < AUDIO_OUTPUT_FRAMES) {
            const size_t count = AUDIO_OUTPUT_FRAMES - diff;
            for (size_t mix = 0; mix < MAX_AUDIO_MIXES; ++mix) {
                if ((mixers & (1 << mix)) == 0)
                    continue;
                for (size_t ch = 0; ch < channels; ++ch) {
                    float *out = audio_output->output[mix].data[ch] + diff;
                    const float *in = child_audio.output[mix].data[ch];
                    if (!out || !in)
                        continue;
                    for (size_t i = 0; i < count; ++i) {
                        out[i] += in[i];
                    }
                }
            }
        }
    }

    if (ts_b) {
        struct obs_source_audio_mix child_audio;
        obs_source_get_audio_mix(src_b, &child_audio);
        const size_t diff = static_cast<size_t>((ts_b - min_ts) * static_cast<uint64_t>(sample_rate) / 1000000000ULL);
        if (diff < AUDIO_OUTPUT_FRAMES) {
            const size_t count = AUDIO_OUTPUT_FRAMES - diff;
            for (size_t mix = 0; mix < MAX_AUDIO_MIXES; ++mix) {
                if ((mixers & (1 << mix)) == 0)
                    continue;
                for (size_t ch = 0; ch < channels; ++ch) {
                    float *out = audio_output->output[mix].data[ch] + diff;
                    const float *in = child_audio.output[mix].data[ch];
                    if (!out || !in)
                        continue;
                    for (size_t i = 0; i < count; ++i) {
                        out[i] += in[i];
                    }
                }
            }
        }
    }

    *ts_out = min_ts;
    return true;
}

static void mixer_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(seconds);
    sync_decks_audio_state();
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

    uint32_t cx = 1920, cy = 1080;
    get_canvas_size(&cx, &cy);

    // 1. Deck A lerenderelése textúrába (automatikus képaránytartó illesztéssel)
    gs_texrender_reset(mixer->texrender_a);
    if (gs_texrender_begin(mixer->texrender_a, cx, cy)) {
        struct vec4 clear_color;
        vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
        gs_ortho(0.0f, static_cast<float>(cx), 0.0f, static_cast<float>(cy), -100.0f, 100.0f);

        render_source_fitted(src_a, cx, cy);
        gs_texrender_end(mixer->texrender_a);
    }

    // 2. Deck B lerenderelése textúrába (automatikus képaránytartó illesztéssel)
    gs_texrender_reset(mixer->texrender_b);
    if (gs_texrender_begin(mixer->texrender_b, cx, cy)) {
        struct vec4 clear_color;
        vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
        gs_ortho(0.0f, static_cast<float>(cx), 0.0f, static_cast<float>(cy), -100.0f, 100.0f);

        render_source_fitted(src_b, cx, cy);
        gs_texrender_end(mixer->texrender_b);
    }

    gs_texture_t *tex_a = gs_texrender_get_texture(mixer->texrender_a);
    gs_texture_t *tex_b = gs_texrender_get_texture(mixer->texrender_b);
    if (!tex_a || !tex_b)
        return;

    // 3. Shader paraméterek átadása
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

    // 4. Kevert végeredmény kirajzolása
    while (gs_effect_loop(mixer->effect, "Draw")) {
        gs_draw_sprite(nullptr, 0, cx, cy);
    }
}

static uint32_t mixer_get_width(void *data)
{
    UNUSED_PARAMETER(data);
    uint32_t cx = 1920, cy = 1080;
    get_canvas_size(&cx, &cy);
    return cx;
}

static uint32_t mixer_get_height(void *data)
{
    UNUSED_PARAMETER(data);
    uint32_t cx = 1920, cy = 1080;
    get_canvas_size(&cx, &cy);
    return cy;
}

// ==========================================================================
// 3. OBS Források Regisztrációja
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
    vdj_deck_a_info.update = deck_monitor_update;
    vdj_deck_a_info.get_defaults = deck_monitor_defaults;
    vdj_deck_a_info.get_properties = deck_monitor_properties;
    vdj_deck_a_info.video_tick = deck_monitor_video_tick;
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
    vdj_deck_b_info.update = deck_monitor_update;
    vdj_deck_b_info.get_defaults = deck_monitor_defaults;
    vdj_deck_b_info.get_properties = deck_monitor_properties;
    vdj_deck_b_info.video_tick = deck_monitor_video_tick;
    vdj_deck_b_info.video_render = deck_monitor_video_render;
    vdj_deck_b_info.get_width = deck_monitor_get_width;
    vdj_deck_b_info.get_height = deck_monitor_get_height;
    vdj_deck_b_info.type_data = reinterpret_cast<void *>(static_cast<intptr_t>(1));
    obs_register_source(&vdj_deck_b_info);

    // Master Output Mixer Source (Video + Audio + Composite)
    vdj_master_mixer_info.id = "vdj_master_mixer";
    vdj_master_mixer_info.type = OBS_SOURCE_TYPE_INPUT;
    vdj_master_mixer_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_COMPOSITE;
    vdj_master_mixer_info.get_name = mixer_get_name;
    vdj_master_mixer_info.create = mixer_create;
    vdj_master_mixer_info.destroy = mixer_destroy;
    vdj_master_mixer_info.update = mixer_update;
    vdj_master_mixer_info.get_defaults = mixer_defaults;
    vdj_master_mixer_info.get_properties = mixer_properties;
    vdj_master_mixer_info.enum_active_sources = mixer_enum_active_sources;
    vdj_master_mixer_info.audio_render = mixer_audio_render;
    vdj_master_mixer_info.video_tick = mixer_video_tick;
    vdj_master_mixer_info.video_render = mixer_video_render;
    vdj_master_mixer_info.get_width = mixer_get_width;
    vdj_master_mixer_info.get_height = mixer_get_height;
    obs_register_source(&vdj_master_mixer_info);
}
