#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include "deck-engine.hpp"

extern "C" {

struct DeckSourceContext {
    obs_source_t *source = nullptr;
    DeckPlayer *deck = nullptr;
    bool audio_captured = false;
};

static const char *deck_get_name(void *)
{
    return "OBS VJ Deck";
}

static void deck_audio_capture(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
    UNUSED_PARAMETER(source);
    if (muted || !audio_data)
        return;

    auto *context = static_cast<DeckSourceContext *>(param);
    if (!context || !context->source)
        return;

    obs_source_output_audio(context->source, audio_data);
}

static void *deck_create(obs_data_t *settings, obs_source_t *source)
{
    auto *context = new DeckSourceContext();
    context->source = source;
    context->deck = vj_active_deck();

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
        if (context->audio_captured && context->deck && context->deck->source()) {
            obs_source_remove_audio_capture_callback(context->deck->source(), deck_audio_capture, context);
            context->audio_captured = false;
        }
        delete context;
    }
}

static void deck_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(seconds);
    auto *context = static_cast<DeckSourceContext *>(data);
    if (context && context->deck) {
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

    const uint32_t sw = obs_source_get_width(media);
    const uint32_t sh = obs_source_get_height(media);

    if (sw > 0 && sh > 0 && (sw != 1920 || sh != 1080)) {
        gs_matrix_push();
        gs_matrix_scale3f(1920.0f / static_cast<float>(sw),
                          1080.0f / static_cast<float>(sh),
                          1.0f);
        obs_source_video_render(media);
        gs_matrix_pop();
    } else {
        obs_source_video_render(media);
    }
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
    obs_properties_add_path(props, "local_file", "Médiafájl (Videó / Audió)",
                            OBS_PATH_FILE,
                            "Media Files (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav *.flac *.aac);;All Files (*.*)",
                            nullptr);
    return props;
}

static void deck_update(void *data, obs_data_t *settings)
{
    auto *context = static_cast<DeckSourceContext *>(data);
    if (!context || !context->deck || !settings)
        return;

    const char *file = obs_data_get_string(settings, "local_file");
    if (file && *file) {
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
}

} // extern "C"
