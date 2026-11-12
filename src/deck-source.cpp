#include <obs-module.h>
#include "deck-engine.hpp"

extern "C" {

struct DeckSourceContext {
    obs_source_t *source = nullptr;
    DeckPlayer *deck = nullptr;
};

static const char *deck_get_name(void *)
{
    return "OBS VJ Deck";
}

static void *deck_create(obs_data_t *settings, obs_source_t *source)
{
    auto *context = new DeckSourceContext();
    context->source = source;
    context->deck = vj_active_deck();

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
    delete static_cast<DeckSourceContext *>(data);
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
    if (media) {
        obs_source_video_render(media);
    }
}

static uint32_t deck_get_width(void *data)
{
    auto *context = static_cast<DeckSourceContext *>(data);
    if (context && context->deck && context->deck->source()) {
        const uint32_t w = obs_source_get_width(context->deck->source());
        if (w > 0)
            return w;
    }
    return 1920;
}

static uint32_t deck_get_height(void *data)
{
    auto *context = static_cast<DeckSourceContext *>(data);
    if (context && context->deck && context->deck->source()) {
        const uint32_t h = obs_source_get_height(context->deck->source());
        if (h > 0)
            return h;
    }
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
    deck_info.output_flags = OBS_SOURCE_VIDEO;
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
