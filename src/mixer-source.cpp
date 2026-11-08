#include <obs-module.h>
#include <graphics/graphics.h>
#include "deck-engine.hpp"

extern "C" {

struct MixerSource {
    obs_source_t *source;
    bool is_master;
    int channel;
};

static const char *mixer_get_name(void *type_data)
{
    UNUSED_PARAMETER(type_data);
    return "Traktor S2 VJ Mixer Source";
}

static void *mixer_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    MixerSource *context = static_cast<MixerSource *>(bmalloc(sizeof(MixerSource)));
    if (!context)
        return nullptr;
        
    context->source = source;
    context->is_master = false;
    context->channel = 0;

    obs_enter_graphics();
    obs_leave_graphics();

    return context;
}

static void mixer_destroy(void *data)
{
    MixerSource *context = static_cast<MixerSource *>(data);
    if (context) {
        bfree(context);
    }
}

static void mixer_update(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(settings);
}

static void mixer_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    MixerSource *context = static_cast<MixerSource *>(data);
    if (!context || !context->source)
        return;
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

static obs_properties_t *mixer_properties(void *data)
{
    UNUSED_PARAMETER(data);
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_list(props, "transition", "FX Transition", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    return props;
}

static void mixer_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "transition", 0);
}

struct obs_source_info get_mixer_source_info()
{
    struct obs_source_info info = {};
    info.id = "vdj_master_mixer";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE;
    info.get_name = mixer_get_name;
    info.create = mixer_create;
    info.destroy = mixer_destroy;
    info.update = mixer_update;
    info.video_render = mixer_video_render;
    info.get_width = mixer_get_width;
    info.get_height = mixer_get_height;
    info.get_properties = mixer_properties;
    info.get_defaults = mixer_defaults;
    return info;
}

static const char *deck_monitor_get_name(void *type_data)
{
    UNUSED_PARAMETER(type_data);
    return "Traktor S2 Deck Monitor";
}

static void *deck_monitor_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    MixerSource *context = static_cast<MixerSource *>(bmalloc(sizeof(MixerSource)));
    if (!context)
        return nullptr;
        
    context->source = source;
    context->is_master = false;
    return context;
}

static void deck_monitor_destroy(void *data)
{
    MixerSource *context = static_cast<MixerSource *>(data);
    if (context) {
        bfree(context);
    }
}

struct obs_source_info get_deck_a_source_info()
{
    struct obs_source_info info = {};
    info.id = "vdj_deck_a_source";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
    info.get_name = deck_monitor_get_name;
    info.create = deck_monitor_create;
    info.destroy = deck_monitor_destroy;
    info.get_width = mixer_get_width;
    info.get_height = mixer_get_height;
    return info;
}

struct obs_source_info get_deck_b_source_info()
{
    struct obs_source_info info = {};
    info.id = "vdj_deck_b_source";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
    info.get_name = deck_monitor_get_name;
    info.create = deck_monitor_create;
    info.destroy = deck_monitor_destroy;
    info.get_width = mixer_get_width;
    info.get_height = mixer_get_height;
    return info;
}

void vdj_register_sources(void)
{
    struct obs_source_info mixer_info = get_mixer_source_info();
    obs_register_source_s(&mixer_info, sizeof(mixer_info));

    struct obs_source_info deck_a_info = get_deck_a_source_info();
    obs_register_source_s(&deck_a_info, sizeof(deck_a_info));

    struct obs_source_info deck_b_info = get_deck_b_source_info();
    obs_register_source_s(&deck_b_info, sizeof(deck_b_info));
}

} // extern "C"
