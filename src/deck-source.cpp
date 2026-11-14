#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <media-io/audio-io.h>
#include "deck-engine.hpp"

extern "C" {

struct DeckSourceContext {
    obs_source_t *source = nullptr;
    DeckPlayer *deck = nullptr;
    bool audio_captured = false;
    gs_texrender_t *freeze_tex = nullptr;
    bool has_frozen_frame = false;
    int deck_channel = 0;
};

static const char *deck_get_name(void *)
{
    return "OBS VJ Deck";
}

static void deck_audio_capture(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
    UNUSED_PARAMETER(source);
    if (muted || !audio_data || audio_data->frames == 0)
        return;

    auto *context = static_cast<DeckSourceContext *>(param);
    if (!context || !context->source)
        return;

    uint32_t sample_rate = 48000;
    audio_t *audio = obs_get_audio();
    if (audio) {
        sample_rate = audio_output_get_sample_rate(audio);
    }

    struct obs_source_audio out_audio = {};
    for (size_t i = 0; i < MAX_AV_PLANES; i++) {
        out_audio.data[i] = audio_data->data[i];
    }
    out_audio.frames = audio_data->frames;
    out_audio.speakers = SPEAKERS_STEREO;
    out_audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
    out_audio.samples_per_sec = sample_rate;
    out_audio.timestamp = audio_data->timestamp;

    obs_source_output_audio(context->source, &out_audio);
}

static obs_source_t *g_obs_source_deck_a = nullptr;
static obs_source_t *g_obs_source_deck_b = nullptr;

obs_source_t *vdj_get_obs_source_deck(int channel)
{
    return (channel == 1) ? g_obs_source_deck_b : g_obs_source_deck_a;
}

static void *deck_create(obs_data_t *settings, obs_source_t *source)
{
    auto *context = new DeckSourceContext();
    context->source = source;
    context->deck_channel = settings ? static_cast<int>(obs_data_get_int(settings, "deck_channel")) : 0;
    context->deck = (context->deck_channel == 1) ? vj_deck_b() : vj_deck_a();
    if (!context->deck) {
        context->deck = vj_active_deck();
    }

    if (context->deck_channel == 1) {
        g_obs_source_deck_b = source;
    } else {
        g_obs_source_deck_a = source;
    }

    obs_source_set_audio_mixers(source, 0x3F);
    obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);

    if (context->deck && context->deck->source()) {
        obs_source_add_audio_capture_callback(context->deck->source(), deck_audio_capture, context);
        context->audio_captured = true;
    }

    if (settings && context->deck) {
        const char *file = obs_data_get_string(settings, "local_file");
        if (file && *file) {
            context->deck->loadFile(QString::fromUtf8(file));
        }
    }
    return context;
}

static void deck_destroy(void *data)
{
    auto *context = static_cast<DeckSourceContext *>(data);
    if (context) {
        if (g_obs_source_deck_a == context->source)
            g_obs_source_deck_a = nullptr;
        if (g_obs_source_deck_b == context->source)
            g_obs_source_deck_b = nullptr;

        if (context->audio_captured && context->deck && context->deck->source()) {
            obs_source_remove_audio_capture_callback(context->deck->source(), deck_audio_capture, context);
            context->audio_captured = false;
        }
        if (context->freeze_tex) {
            obs_enter_graphics();
            gs_texrender_destroy(context->freeze_tex);
            context->freeze_tex = nullptr;
            obs_leave_graphics();
        }
        delete context;
    }
}

static void deck_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(seconds);
    auto *context = static_cast<DeckSourceContext *>(data);
    if (!context)
        return;

    if (context->source) {
        // Enforce all 6 audio tracks and monitor+output if OBS loaded 0 from JSON
        const uint32_t mixers = obs_source_get_audio_mixers(context->source);
        if (mixers == 0) {
            obs_source_set_audio_mixers(context->source, 0x3F);
            obs_source_set_monitoring_type(context->source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
        }

        // Ensure audio capture callback is connected to player source
        if (!context->audio_captured && context->deck && context->deck->source()) {
            obs_source_add_audio_capture_callback(context->deck->source(), deck_audio_capture, context);
            context->audio_captured = true;
        }
    }

    if (context->deck) {
        context->deck->updateLoop();
    }
}

static void deck_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    auto *context = static_cast<DeckSourceContext *>(data);
    if (!context || !context->deck)
        return;

    obs_source_t *media = context->deck->source();
    if (!media)
        return;

    if (context->deck->hasFileChanged()) {
        context->has_frozen_frame = false;
        context->deck->clearFileChanged();
    }

    if (!context->freeze_tex) {
        context->freeze_tex = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    }

    const bool restarting = context->deck->isSpeedRestarting();

    // When NOT restarting, capture the latest frame into freeze_tex
    if (!restarting && context->freeze_tex) {
        if (gs_texrender_begin(context->freeze_tex, 1920, 1080)) {
            struct vec4 clear_color;
            vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
            gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

            const uint32_t sw = obs_source_get_width(media);
            const uint32_t sh = obs_source_get_height(media);

            if (sw > 0 && sh > 0) {
                gs_matrix_push();
                gs_matrix_scale3f(1920.0f / static_cast<float>(sw),
                                  1080.0f / static_cast<float>(sh),
                                  1.0f);
                obs_source_video_render(media);
                gs_matrix_pop();
            } else {
                obs_source_video_render(media);
            }
            gs_texrender_end(context->freeze_tex);
            context->has_frozen_frame = true;
        }
    }

    // Render the texture to screen (frozen frame during speed changes, fresh frame normally)
    if (context->freeze_tex && context->has_frozen_frame) {
        gs_texture_t *tex = gs_texrender_get_texture(context->freeze_tex);
        if (tex) {
            obs_source_draw(tex, 0, 0, 1920, 1080, false);
            return;
        }
    }

    // Direct fallback
    obs_source_video_render(media);
}

static uint32_t deck_get_width(void *data)
{
    UNUSED_PARAMETER(data);
    return 1920;
}

static uint32_t deck_get_height(void *data)
{
    UNUSED_PARAMETER(data);
    return 1080;
}

static obs_properties_t *deck_properties(void *data)
{
    UNUSED_PARAMETER(data);
    obs_properties_t *props = obs_properties_create();

    obs_property_t *p_channel = obs_properties_add_list(props, "deck_channel", "Deck Csatorna (A / B)",
                                                        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p_channel, "Deck A", 0);
    obs_property_list_add_int(p_channel, "Deck B", 1);

    obs_properties_add_path(props, "local_file", "Médiafájl (Videó / Audió)",
                            OBS_PATH_FILE,
                            "Media Files (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav *.flac *.aac);;All Files (*.*)",
                            nullptr);
    return props;
}

static void deck_update(void *data, obs_data_t *settings)
{
    auto *context = static_cast<DeckSourceContext *>(data);
    if (!context || !settings)
        return;

    const int channel = static_cast<int>(obs_data_get_int(settings, "deck_channel"));
    if (channel != context->deck_channel || !context->deck) {
        context->deck_channel = channel;
        if (context->audio_captured && context->deck && context->deck->source()) {
            obs_source_remove_audio_capture_callback(context->deck->source(), deck_audio_capture, context);
            context->audio_captured = false;
        }
        context->deck = (channel == 1) ? vj_deck_b() : vj_deck_a();
        if (!context->deck) context->deck = vj_active_deck();
        context->has_frozen_frame = false;
    }

    const char *file = obs_data_get_string(settings, "local_file");
    if (file && *file && context->deck) {
        context->deck->loadFile(QString::fromUtf8(file));
    }
}

static void deck_enum_active_sources(void *data, obs_source_enum_proc_t enum_callback, void *param)
{
    auto *context = static_cast<DeckSourceContext *>(data);
    if (context && context->deck && context->deck->source()) {
        enum_callback(context->source, context->deck->source(), param);
    }
}

static void deck_enum_all_sources(void *data, obs_source_enum_proc_t enum_callback, void *param)
{
    auto *context = static_cast<DeckSourceContext *>(data);
    if (context && context->deck && context->deck->source()) {
        enum_callback(context->source, context->deck->source(), param);
    }
}

static const char *deck_a_get_name(void *)
{
    return "OBS VJ Deck A";
}

static const char *deck_b_get_name(void *)
{
    return "OBS VJ Deck B";
}

static void *deck_a_create(obs_data_t *settings, obs_source_t *source)
{
    if (settings) {
        obs_data_set_int(settings, "deck_channel", 0);
    }
    return deck_create(settings, source);
}

static void *deck_b_create(obs_data_t *settings, obs_source_t *source)
{
    if (settings) {
        obs_data_set_int(settings, "deck_channel", 1);
    }
    return deck_create(settings, source);
}

void vdj_register_deck_source(void)
{
    obs_source_info deck_info = {};
    deck_info.id = "vj_deck";
    deck_info.type = OBS_SOURCE_TYPE_INPUT;
    deck_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
    deck_info.get_name = deck_get_name;
    deck_info.create = deck_create;
    deck_info.destroy = deck_destroy;
    deck_info.video_tick = deck_video_tick;
    deck_info.video_render = deck_video_render;
    deck_info.get_width = deck_get_width;
    deck_info.get_height = deck_get_height;
    deck_info.get_properties = deck_properties;
    deck_info.update = deck_update;
    deck_info.enum_active_sources = deck_enum_active_sources;
    deck_info.enum_all_sources = deck_enum_all_sources;
    obs_register_source_s(&deck_info, sizeof(deck_info));

    obs_source_info deck_a_info = deck_info;
    deck_a_info.id = "vj_deck_a";
    deck_a_info.get_name = deck_a_get_name;
    deck_a_info.create = deck_a_create;
    obs_register_source_s(&deck_a_info, sizeof(deck_a_info));

    obs_source_info deck_b_info = deck_info;
    deck_b_info.id = "vj_deck_b";
    deck_b_info.get_name = deck_b_get_name;
    deck_b_info.create = deck_b_create;
    obs_register_source_s(&deck_b_info, sizeof(deck_b_info));
}

} // extern "C"
