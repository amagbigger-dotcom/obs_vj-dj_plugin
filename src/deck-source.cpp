#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <media-io/audio-io.h>
#include <util/platform.h>
#include "deck-engine.hpp"

#include <vector>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cmath>

extern "C" {

static const char *DECK_COLOR_EFFECT = R"(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform float4 gains;
uniform float saturation;
uniform float alpha;
uniform float wipe_cutoff;
uniform float is_deck_b;

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

float4 PSColorize(VertData v) : TARGET
{
    float4 col = image.Sample(def_sampler, v.uv);
    col.rgb *= gains.rgb;
    float luma = dot(col.rgb, float3(0.2126, 0.7152, 0.0722));
    col.rgb = lerp(luma.xxx, col.rgb, saturation);
    
    float final_a = alpha;
    if (wipe_cutoff >= 0.0) {
        if (is_deck_b > 0.5) {
            final_a *= smoothstep(wipe_cutoff - 0.005, wipe_cutoff + 0.005, v.uv.x);
        } else {
            final_a *= smoothstep(wipe_cutoff + 0.005, wipe_cutoff - 0.005, v.uv.x);
        }
    }
    col.a = saturate(final_a);
    col.rgb *= col.a;
    return col;
}

technique Draw
{
    pass
    {
        vertex_shader = VSDefault(v);
        pixel_shader  = PSColorize(v);
    }
}
)";

// =============================================================================
// Deck forrás kontextus és szálkezelés (Deck Source Context & Thread Management)
// =============================================================================
struct DeckSourceContext {
    obs_source_t *source = nullptr;
    DeckPlayer *deck = nullptr;
    bool audio_captured = false;
    gs_texrender_t *freeze_tex = nullptr;
    bool has_frozen_frame = false;
    int deck_channel = 0;
    gs_effect_t *color_effect = nullptr;

    uint64_t next_audio_ts = 0;

    std::thread audio_thread;
    std::atomic<bool> audio_thread_running{false};
};

static const char *deck_get_name(void *)
{
    return "OBS VJ Deck";
}

// =============================================================================
// FFmpeg audió rögzítés fallback (Audio Capture Callback for FFmpeg Fallback)
// =============================================================================
static void deck_audio_capture(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
    UNUSED_PARAMETER(source);
    if (muted || !audio_data || audio_data->frames == 0)
        return;

    auto *context = static_cast<DeckSourceContext *>(param);
    if (!context || !context->source || !context->deck)
        return;

    // Ha a RAM audio motor már betöltődött, ignoráljuk az FFmpeg audiót (RAM Audio takes precedence)
    if (context->deck->hasAudioBuffer())
        return;

    // Ha a deck nincs lejátszásban, abszolút csend (No audio when paused / on media load)
    if (!context->deck->isPlaying())
        return;

    uint32_t sample_rate = 48000;
    audio_t *audio = obs_get_audio();
    if (audio) {
        sample_rate = audio_output_get_sample_rate(audio);
    }

    const size_t frames = audio_data->frames;
    const float ch_gain = context->deck->gain();
    const float ch_vol = context->deck->volume();
    const float mix_gain = context->deck->mixGain();

    static thread_local std::vector<float> buf_l;
    static thread_local std::vector<float> buf_r;
    buf_l.resize(frames);
    buf_r.resize(frames);

    const float *in_l = reinterpret_cast<const float *>(audio_data->data[0]);
    const float *in_r = audio_data->data[1] ? reinterpret_cast<const float *>(audio_data->data[1]) : in_l;

    for (size_t i = 0; i < frames; ++i) {
        buf_l[i] = in_l[i];
        buf_r[i] = in_r[i];
    }

    // 3-sávos EQ és DJ Filter
    context->deck->processAudioDsp(buf_l.data(), frames, 0, sample_rate);
    context->deck->processAudioDsp(buf_r.data(), frames, 1, sample_rate);

    // Fejhallgató CUE
    DeckPair *decks = vdj_decks();
    if (decks) {
        const float *channels[2] = {buf_l.data(), buf_r.data()};
        decks->queueCueAudio(context->deck_channel, channels, frames);
    }

    const float net_gain = ch_gain * ch_vol * mix_gain;
    for (size_t i = 0; i < frames; ++i) {
        buf_l[i] = std::clamp(buf_l[i] * net_gain, -1.2f, 1.2f);
        buf_r[i] = std::clamp(buf_r[i] * net_gain, -1.2f, 1.2f);
    }

    const uint64_t cur_time = os_gettime_ns();
    if (context->next_audio_ts == 0 ||
        std::abs(static_cast<int64_t>(cur_time - context->next_audio_ts)) > 100000000LL) {
        context->next_audio_ts = cur_time;
    }

    struct obs_source_audio out_audio = {};
    out_audio.data[0] = reinterpret_cast<uint8_t *>(buf_l.data());
    out_audio.data[1] = reinterpret_cast<uint8_t *>(buf_r.data());
    out_audio.frames = static_cast<uint32_t>(frames);
    out_audio.speakers = SPEAKERS_STEREO;
    out_audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
    out_audio.samples_per_sec = sample_rate;
    out_audio.timestamp = context->next_audio_ts;

    context->next_audio_ts += static_cast<uint64_t>(frames) * 1000000000ULL / sample_rate;

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

    // RAM Audio Engine szál indítása (Zero-Stutter Dedicated RAM Audio Thread)
    context->audio_thread_running.store(true, std::memory_order_relaxed);
    context->audio_thread = std::thread([context]() {
        timeBeginPeriod(1);
        const uint32_t sample_rate = 48000;
        const size_t frames_per_tick = 480; // 10ms precíz időszelet 48kHz-en
        std::vector<float> buf_l(frames_per_tick);
        std::vector<float> buf_r(frames_per_tick);

        while (context->audio_thread_running.load(std::memory_order_relaxed)) {
            const auto tick_start = std::chrono::steady_clock::now();

            if (context->deck && context->deck->hasAudioBuffer()) {
                if (context->deck->isPlaying()) {
                    context->deck->readAudioFrames(buf_l.data(), buf_r.data(), frames_per_tick, sample_rate);

                    // 3-sávos EQ és DJ Filter
                    context->deck->processAudioDsp(buf_l.data(), frames_per_tick, 0, sample_rate);
                    context->deck->processAudioDsp(buf_r.data(), frames_per_tick, 1, sample_rate);

                    // Fejhallgató CUE (PFL)
                    DeckPair *decks = vdj_decks();
                    if (decks) {
                        const float *channels[2] = {buf_l.data(), buf_r.data()};
                        decks->queueCueAudio(context->deck_channel, channels, frames_per_tick);
                    }

                    // Vonal szintű hangerő és crossfader
                    const float ch_gain = context->deck->gain();
                    const float ch_vol = context->deck->volume();
                    const float mix_gain = context->deck->mixGain();
                    const float net_gain = ch_gain * ch_vol * mix_gain;

                    for (size_t i = 0; i < frames_per_tick; ++i) {
                        buf_l[i] = std::clamp(buf_l[i] * net_gain, -1.2f, 1.2f);
                        buf_r[i] = std::clamp(buf_r[i] * net_gain, -1.2f, 1.2f);
                    }

                    const uint64_t cur_time = os_gettime_ns();
                    if (context->next_audio_ts == 0 ||
                        std::abs(static_cast<int64_t>(cur_time - context->next_audio_ts)) > 100000000LL) {
                        context->next_audio_ts = cur_time;
                    }

                    struct obs_source_audio out_audio = {};
                    out_audio.data[0] = reinterpret_cast<uint8_t *>(buf_l.data());
                    out_audio.data[1] = reinterpret_cast<uint8_t *>(buf_r.data());
                    out_audio.frames = static_cast<uint32_t>(frames_per_tick);
                    out_audio.speakers = SPEAKERS_STEREO;
                    out_audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
                    out_audio.samples_per_sec = sample_rate;
                    out_audio.timestamp = context->next_audio_ts;

                    context->next_audio_ts += static_cast<uint64_t>(frames_per_tick) * 1000000000ULL / sample_rate;

                    obs_source_output_audio(context->source, &out_audio);
                } else {
                    context->next_audio_ts = 0;
                }
            }

            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - tick_start).count();
            const int sleep_us = 10000 - static_cast<int>(elapsed);
            if (sleep_us > 1000) {
                std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
            }
        }
        timeEndPeriod(1);
    });

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
        context->audio_thread_running.store(false, std::memory_order_relaxed);
        if (context->audio_thread.joinable()) {
            context->audio_thread.join();
        }

        if (g_obs_source_deck_a == context->source)
            g_obs_source_deck_a = nullptr;
        if (g_obs_source_deck_b == context->source)
            g_obs_source_deck_b = nullptr;

        if (context->audio_captured && context->deck && context->deck->source()) {
            obs_source_remove_audio_capture_callback(context->deck->source(), deck_audio_capture, context);
            context->audio_captured = false;
        }
        if (context->color_effect) {
            obs_enter_graphics();
            gs_effect_destroy(context->color_effect);
            context->color_effect = nullptr;
            obs_leave_graphics();
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

        // Gapless loop audio bridging watchdog during FFmpeg seek stalls
        if (context->deck->isLooping() && context->source) {
            const auto now = std::chrono::steady_clock::now();
            const auto silence_gap_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - context->last_audio_tp).count();

            if (silence_gap_ms >= 20) {
                std::lock_guard<std::mutex> lock(context->bridge_mutex);
                const size_t b_size = context->loop_bridge_l.size();
                if (b_size > 1024) {
                    uint32_t sample_rate = 48000;
                    audio_t *audio = obs_get_audio();
                    if (audio) {
                        sample_rate = audio_output_get_sample_rate(audio);
                    }

                    const size_t chunk_frames = 512;
                    static thread_local std::vector<float> bridge_out_l(512);
                    static thread_local std::vector<float> bridge_out_r(512);

                    for (size_t i = 0; i < chunk_frames; ++i) {
                        const size_t idx = (context->bridge_play_idx + i) % b_size;
                        bridge_out_l[i] = context->loop_bridge_l[idx];
                        bridge_out_r[i] = context->loop_bridge_r[idx];
                    }
                    context->bridge_play_idx = (context->bridge_play_idx + chunk_frames) % b_size;

                    const uint64_t cur_time = os_gettime_ns();
                    if (context->next_audio_ts == 0 ||
                        std::abs(static_cast<int64_t>(cur_time - context->next_audio_ts)) > 100000000LL) {
                        context->next_audio_ts = cur_time;
                    }

                    struct obs_source_audio out_audio = {};
                    out_audio.data[0] = reinterpret_cast<uint8_t *>(bridge_out_l.data());
                    out_audio.data[1] = reinterpret_cast<uint8_t *>(bridge_out_r.data());
                    out_audio.frames = static_cast<uint32_t>(chunk_frames);
                    out_audio.speakers = SPEAKERS_STEREO;
                    out_audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
                    out_audio.samples_per_sec = sample_rate;
                    out_audio.timestamp = context->next_audio_ts;

                    context->next_audio_ts += static_cast<uint64_t>(chunk_frames) * 1000000000ULL / sample_rate;
                    context->last_audio_tp = now;

                    obs_source_output_audio(context->source, &out_audio);
                }
            }
        }
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

    // Render the texture to screen with color effect
    if (context->freeze_tex && context->has_frozen_frame) {
        gs_texture_t *tex = gs_texrender_get_texture(context->freeze_tex);
        if (tex) {
            if (!context->color_effect) {
                context->color_effect = gs_effect_create(DECK_COLOR_EFFECT, "deck_color_effect", nullptr);
            }
            if (context->color_effect) {
                gs_eparam_t *param = nullptr;
                if ((param = gs_effect_get_param_by_name(context->color_effect, "image")))
                    gs_effect_set_texture(param, tex);

                vec4 gains;
                vec4_set(&gains, context->deck->red(), context->deck->green(), context->deck->blue(), 1.0f);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "gains")))
                    gs_effect_set_vec4(param, &gains);

                if ((param = gs_effect_get_param_by_name(context->color_effect, "saturation")))
                    gs_effect_set_float(param, context->deck->saturation());

                const bool is_b = (context->deck_channel == 1 || (context->deck && context->deck->side() == DeckPlayer::DeckSide::B));

                if ((param = gs_effect_get_param_by_name(context->color_effect, "alpha")))
                    gs_effect_set_float(param, 1.0f);

                if ((param = gs_effect_get_param_by_name(context->color_effect, "wipe_cutoff")))
                    gs_effect_set_float(param, -1.0f);

                if ((param = gs_effect_get_param_by_name(context->color_effect, "is_deck_b")))
                    gs_effect_set_float(param, is_b ? 1.0f : 0.0f);

                gs_blend_state_push();
                gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

                while (gs_effect_loop(context->color_effect, "Draw")) {
                    gs_draw_sprite(tex, 0, 1920, 1080);
                }

                gs_blend_state_pop();
                return;
            } else {
                obs_source_draw(tex, 0, 0, 1920, 1080, false);
                return;
            }
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
    deck_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;
    deck_info.get_name = deck_get_name;
    deck_info.create = deck_create;
    deck_info.destroy = deck_destroy;
    deck_info.video_tick = deck_video_tick;
    deck_info.video_render = deck_video_render;
    deck_info.get_width = deck_get_width;
    deck_info.get_height = deck_get_height;
    deck_info.get_properties = deck_properties;
    deck_info.update = deck_update;
    // A belső privát forrásokat nem enumeráljuk a jelenetgráfba a kilépéskori kettős felszabadítás elkerülésére (Prevent Double-Free on Exit)
    deck_info.enum_active_sources = nullptr;
    deck_info.enum_all_sources = nullptr;
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
