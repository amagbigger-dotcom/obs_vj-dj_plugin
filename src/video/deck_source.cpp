#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <media-io/audio-io.h>
#include <util/platform.h>
#include "engine/deck_engine.hpp"
#include "effects/fluid_flow.hpp"
#include "mixer_fx_shaders.hpp"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <vector>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cmath>

extern "C" {

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
    bool color_effect_failed = false;
    gs_effect_t *fluid_effect = nullptr;
    uint32_t loaded_vj_effect_version = 0;
    gs_texture_t *dummy_tex = nullptr;

    uint64_t next_audio_ts = 0;
    uint64_t last_deck_time_ns = 0;

    std::thread audio_thread;
    std::atomic<bool> audio_thread_running{false};
    int last_monitoring_mode = -1;
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

    // Ha a Master Mixer már végzi a master audio keverést, elkerüljük a dupla jelet
    DeckPair *decks = vdj_decks();
    if (decks && decks->masterSource() != nullptr)
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

    DeckPair *decks = vdj_decks();
    const bool has_master = (decks && decks->masterSource() != nullptr);
    if (!has_master) {
        obs_source_set_audio_mixers(source, 0x3F);
        obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
    } else {
        obs_source_set_audio_mixers(source, 0);
        obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
    }

    // RAM Audio Engine szál indítása (Zero-Stutter Dedicated RAM Audio Thread)
    context->audio_thread_running.store(true, std::memory_order_relaxed);
    context->audio_thread = std::thread([context]() {
#ifdef _WIN32
        timeBeginPeriod(1);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
        const uint32_t sample_rate = 48000;
        const size_t frames_per_tick = 480; // 10ms precíz időszelet 48kHz-en
        std::vector<float> buf_l(frames_per_tick);
        std::vector<float> buf_r(frames_per_tick);

        auto next_wake = std::chrono::steady_clock::now();

        while (context->audio_thread_running.load(std::memory_order_relaxed)) {
            next_wake += std::chrono::milliseconds(10);

            // Ha az OBS VJ Master Mixer aktív a jelenetben, a keverő végzi a master audio kimenetet,
            // így elkerüljük a dupla mintaleolvasást és fáziskioltást (Master Mixer handles audio stream)
            DeckPair *decks = vdj_decks();
            const bool master_active = (decks && decks->masterSource() && obs_source_active(decks->masterSource()));

            if (!master_active && context->deck && context->deck->hasAudioBuffer()) {
                if (context->deck->isPlaying()) {
                    context->deck->readAudioFrames(buf_l.data(), buf_r.data(), frames_per_tick, sample_rate);

                    // 3-sávos EQ és DJ Filter
                    context->deck->processAudioDsp(buf_l.data(), frames_per_tick, 0, sample_rate);
                    context->deck->processAudioDsp(buf_r.data(), frames_per_tick, 1, sample_rate);

                    // Fejhallgató CUE (PFL)
                    if (decks) {
                        const float *channels[2] = {buf_l.data(), buf_r.data()};
                        decks->queueCueAudio(context->deck_channel, channels, frames_per_tick);
                    }

                    // Vonal szintű hangerő és crossfader (Zipper-noise mentesített simítás)
                    static float s_last_net_gain = 1.0f;
                    const float ch_gain = context->deck->gain();
                    const float ch_vol = context->deck->volume();
                    const float mix_gain = context->deck->mixGain();
                    const float target_net_gain = ch_gain * ch_vol * mix_gain;

                    const float inv_tick = (frames_per_tick > 0) ? (1.0f / static_cast<float>(frames_per_tick)) : 1.0f;
                    const float step_gain = (target_net_gain - s_last_net_gain) * inv_tick;
                    float cur_g = s_last_net_gain;

                    for (size_t i = 0; i < frames_per_tick; ++i) {
                        cur_g += step_gain;
                        buf_l[i] = std::clamp(buf_l[i] * cur_g, -1.2f, 1.2f);
                        buf_r[i] = std::clamp(buf_r[i] * cur_g, -1.2f, 1.2f);
                    }
                    s_last_net_gain = target_net_gain;

                    const uint64_t cur_time = os_gettime_ns();
                    if (context->next_audio_ts == 0) {
                        // 25ms biztonsági előpuffer az OBS belső jitter-kivédésére
                        context->next_audio_ts = cur_time + 25000000ULL;
                    } else if (cur_time > context->next_audio_ts + 80000000ULL ||
                               context->next_audio_ts > cur_time + 150000000ULL) {
                        // Időcsúszás esetén újraszinkronizálás
                        context->next_audio_ts = cur_time + 25000000ULL;
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

            const auto now = std::chrono::steady_clock::now();
            if (next_wake > now) {
                std::this_thread::sleep_until(next_wake);
            } else {
                next_wake = now;
            }
        }
        timeEndPeriod(1);
    });

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

        if (context->color_effect) {
            obs_enter_graphics();
            gs_effect_destroy(context->color_effect);
            context->color_effect = nullptr;
            obs_leave_graphics();
        }
        if (context->fluid_effect) {
            obs_enter_graphics();
            gs_effect_destroy(context->fluid_effect);
            context->fluid_effect = nullptr;
            obs_leave_graphics();
        }
        if (context->dummy_tex) {
            obs_enter_graphics();
            gs_texture_destroy(context->dummy_tex);
            context->dummy_tex = nullptr;
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

// =============================================================================
// Deck videó tick ciklus (Állapotfrissítés és belső ffmpeg képkocka-pumpálás)
// =============================================================================
static void deck_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(seconds);
    auto *context = static_cast<DeckSourceContext *>(data);
    if (!context)
        return;

    if (context->source) {
        DeckPair *decks = vdj_decks();
        const bool has_master = (decks && decks->masterSource() != nullptr);
        const int target_mode = has_master ? 0 : 1;
        if (context->last_monitoring_mode != target_mode) {
            context->last_monitoring_mode = target_mode;
            if (has_master) {
                obs_source_set_monitoring_type(context->source, OBS_MONITORING_TYPE_NONE);
                obs_source_set_audio_mixers(context->source, 0);
            } else {
                obs_source_set_audio_mixers(context->source, 0x3F);
                obs_source_set_monitoring_type(context->source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
            }
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

    const uint64_t now_deck_ns = os_gettime_ns();
    if (context->last_deck_time_ns > 0) {
        float dt = static_cast<float>(now_deck_ns - context->last_deck_time_ns) * 1e-9f;
        if (dt > 0.1f) dt = 0.1f;
        if (dt > 0.0f) {
            context->deck->updateVjShaderTransition(dt);
        }
    }
    context->last_deck_time_ns = now_deck_ns;

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

            if (sw > 0 && sh > 0 && context->deck->hasVideo()) {
                gs_matrix_push();
                DeckPair *dp = vdj_decks();
                const int scale_mode = dp ? dp->videoScaleMode() : 1; // 0 = Fit, 1 = Fill, 2 = Stretch

                if (scale_mode == 0) {
                    // FIT (Aránytartó teljes kép, fekete sávokkal)
                    const float s = std::min(1920.0f / static_cast<float>(sw), 1080.0f / static_cast<float>(sh));
                    const float draw_w = static_cast<float>(sw) * s;
                    const float draw_h = static_cast<float>(sh) * s;
                    const float off_x = (1920.0f - draw_w) * 0.5f;
                    const float off_y = (1080.0f - draw_h) * 0.5f;
                    gs_matrix_translate3f(off_x, off_y, 0.0f);
                    gs_matrix_scale3f(s, s, 1.0f);
                } else if (scale_mode == 1) {
                    // FILL (Aránytartó teljes vászonkitöltés fekete sávok nélkül)
                    const float s = std::max(1920.0f / static_cast<float>(sw), 1080.0f / static_cast<float>(sh));
                    const float draw_w = static_cast<float>(sw) * s;
                    const float draw_h = static_cast<float>(sh) * s;
                    const float off_x = (1920.0f - draw_w) * 0.5f;
                    const float off_y = (1080.0f - draw_h) * 0.5f;
                    gs_matrix_translate3f(off_x, off_y, 0.0f);
                    gs_matrix_scale3f(s, s, 1.0f);
                } else {
                    // STRETCH
                    gs_matrix_scale3f(1920.0f / static_cast<float>(sw),
                                      1080.0f / static_cast<float>(sh),
                                      1.0f);
                }
                obs_source_video_render(media);
                gs_matrix_pop();
            } else if (context->deck && context->deck->hasAudioOnlyTrack() && context->deck->isPlaying() && !context->deck->isAtEnd()) {
                // Procedurális Audio-Reaktív VJ Shader Suite (audió fájlokhoz vagy üres deckhez)
                const uint32_t cur_vj_ver = CustomShaderManager::instance().version();
                if (context->fluid_effect && context->loaded_vj_effect_version != cur_vj_ver) {
                    gs_effect_destroy(context->fluid_effect);
                    context->fluid_effect = nullptr;
                }
                if (!context->fluid_effect) {
                    char *err = nullptr;
                    context->fluid_effect = gs_effect_create(FLUID_FLOW_EFFECT, "fluid_flow_effect", &err);
                    if (!context->fluid_effect) {
                        blog(LOG_ERROR, "[vj_deck] Failed to compile FLUID_FLOW_EFFECT: %s", err ? err : "unknown");
                        bfree(err);
                    } else {
                        context->loaded_vj_effect_version = cur_vj_ver;
                    }
                }
                if (!context->dummy_tex) {
                    const uint32_t white_px = 0xFFFFFFFF;
                    const uint8_t *data_ptr = reinterpret_cast<const uint8_t *>(&white_px);
                    context->dummy_tex = gs_texture_create(1, 1, GS_RGBA, 1, &data_ptr, 0);
                }
                if (context->fluid_effect && context->dummy_tex) {
                    const float time_sec = static_cast<float>(os_gettime_ns() / 1000000ULL) * 0.001f;
                    const float bpm_val = static_cast<float>(context->deck->bpm());
                    const float amp_val = context->deck->currentAmplitude();
                    const float bass_val = context->deck->currentBassEnergy();
                    const float mid_val = context->deck->currentMidEnergy();
                    const float high_val = context->deck->currentHighEnergy();
                    const bool is_b = (context->deck_channel == 1 || (context->deck && context->deck->side() == DeckPlayer::DeckSide::B));

                    VjDeckConfig v_cfg = context->deck->vjConfig();
                    const float react = std::clamp(v_cfg.audio_reactivity, 0.0f, 2.5f);
                    const float eff_bass = std::clamp(bass_val * react, 0.0f, 1.5f);
                    const float eff_mid = std::clamp(mid_val * react, 0.0f, 1.5f);
                    const float eff_high = std::clamp(high_val * react, 0.0f, 1.5f);
                    const float eff_amp = std::clamp(amp_val * react, 0.0f, 1.5f);

                    const float viz_mode = static_cast<float>(context->deck->vjShaderIdx());
                    const float viz_mode_prev = static_cast<float>(context->deck->vjShaderIdxPrev());
                    const float viz_trans_t = context->deck->vjShaderTransProgress();
                    const float macro_tonus = context->deck->currentMacroTonus();

                    const auto &defs = getVjShaderDefinitions();
                    float p_vals[8] = {0.0f};
                    const float tone_balance = (eff_high + eff_mid * 0.5f) / (eff_bass + eff_mid + eff_high + 0.001f);

                    DeckPair *d_pair = vdj_decks();
                    const float lfo1 = d_pair ? d_pair->currentLfo1Value() : 0.0f;
                    const float lfo2 = d_pair ? d_pair->currentLfo2Value() : 0.0f;

                    if (v_cfg.shader_idx >= 0 && v_cfg.shader_idx < static_cast<int>(defs.size())) {
                        const auto &p_defs = defs[v_cfg.shader_idx].params;
                        for (size_t i = 0; i < p_defs.size() && i < 8; ++i) {
                            p_vals[i] = computeEffectiveParam(p_defs[i], v_cfg.params[i].value,
                                                              v_cfg.params[i].mod_depth,
                                                              v_cfg.params[i].mod_mode,
                                                              eff_bass, eff_mid, eff_high,
                                                              macro_tonus, lfo1, lfo2);
                        }
                    }

                    const float audio_time = time_sec + eff_bass * 0.15f;
                    gs_eparam_t *p = nullptr;
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "visualizer_mode")))
                        gs_effect_set_float(p, viz_mode);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "visualizer_mode_prev")))
                        gs_effect_set_float(p, viz_mode_prev);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "visualizer_trans_t")))
                        gs_effect_set_float(p, viz_trans_t);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "anim_time")))
                        gs_effect_set_float(p, audio_time);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "bpm")))
                        gs_effect_set_float(p, bpm_val);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "audio_amp")))
                        gs_effect_set_float(p, eff_amp);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "bass_energy")))
                        gs_effect_set_float(p, eff_bass);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "mid_energy")))
                        gs_effect_set_float(p, eff_mid);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "high_energy")))
                        gs_effect_set_float(p, eff_high);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "audio_reactivity")))
                        gs_effect_set_float(p, react);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "is_deck_b")))
                        gs_effect_set_float(p, is_b ? 1.0f : 0.0f);

                    float theme_r = 0.0f, theme_g = 0.0f, theme_b = 0.0f, theme_act = 0.0f;
                    computeThemeColor(v_cfg, is_b, time_sec, bpm_val, eff_bass, macro_tonus,
                                      theme_r, theme_g, theme_b, theme_act);
                    if ((p = gs_effect_get_param_by_name(context->fluid_effect, "custom_theme_color"))) {
                        struct vec4 c_vec;
                        vec4_set(&c_vec, theme_r, theme_g, theme_b, theme_act);
                        gs_effect_set_vec4(p, &c_vec);
                    }

                    char p_name[4] = "p0";
                    for (int i = 0; i < 8; ++i) {
                        p_name[1] = static_cast<char>('0' + i);
                        if ((p = gs_effect_get_param_by_name(context->fluid_effect, p_name)))
                            gs_effect_set_float(p, p_vals[i]);
                    }

                    while (gs_effect_loop(context->fluid_effect, "Draw")) {
                        gs_draw_sprite(context->dummy_tex, 0, 1920, 1080);
                    }
                }
            }
            gs_texrender_end(context->freeze_tex);
            context->has_frozen_frame = true;
        }
    }

    // Render the texture to screen with color effect
    if (context->freeze_tex && context->has_frozen_frame) {
        gs_texture_t *tex = gs_texrender_get_texture(context->freeze_tex);
        if (tex) {
            if (!context->color_effect && !context->color_effect_failed) {
                char *errors = nullptr;
                context->color_effect = gs_effect_create(getDeckColorEffectCode(), "deck_color_effect", &errors);
                if (!context->color_effect) {
                    blog(LOG_ERROR, "[vj_deck] Failed to compile deck_color_effect: %s", errors ? errors : "unknown error");
                    bfree(errors);
                    context->color_effect_failed = true;
                }
            }
            if (context->color_effect) {
                gs_eparam_t *param = nullptr;

                if ((param = gs_effect_get_param_by_name(context->color_effect, "image")))
                    gs_effect_set_texture(param, tex);

                const bool vis_eq = context->deck ? context->deck->isVisualEqEnabled() : true;

                vec4 gains;
                if (vis_eq && context->deck) {
                    vec4_set(&gains, context->deck->red(), context->deck->green(), context->deck->blue(), 1.0f);
                } else {
                    vec4_set(&gains, 1.0f, 1.0f, 1.0f, 1.0f);
                }
                if ((param = gs_effect_get_param_by_name(context->color_effect, "gains")))
                    gs_effect_set_vec4(param, &gains);

                const float sat = (vis_eq && context->deck) ? context->deck->saturation() : 1.0f;
                if ((param = gs_effect_get_param_by_name(context->color_effect, "saturation")))
                    gs_effect_set_float(param, sat);

                const float fx_val = (vis_eq && context->deck) ? context->deck->filterVal() : 0.0f;
                const float fx_type = context->deck ? static_cast<float>(context->deck->mixerFxType()) : 0.0f;
                const float bpm_val = context->deck ? static_cast<float>(context->deck->bpm()) : 128.0f;
                const float anim_time = static_cast<float>(os_gettime_ns() / 1000000ULL) * 0.001f;

                if ((param = gs_effect_get_param_by_name(context->color_effect, "mixer_fx_type")))
                    gs_effect_set_float(param, fx_type);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "mixer_fx_val")))
                    gs_effect_set_float(param, fx_val);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "bpm")))
                    gs_effect_set_float(param, bpm_val);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "anim_time")))
                    gs_effect_set_float(param, anim_time);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "visual_eq_enabled")))
                    gs_effect_set_float(param, vis_eq ? 1.0f : 0.0f);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "bass_energy")))
                    gs_effect_set_float(param, context->deck ? context->deck->currentBassEnergy() : 0.0f);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "mid_energy")))
                    gs_effect_set_float(param, context->deck ? context->deck->currentMidEnergy() : 0.0f);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "high_energy")))
                    gs_effect_set_float(param, context->deck ? context->deck->currentHighEnergy() : 0.0f);
                if ((param = gs_effect_get_param_by_name(context->color_effect, "audio_amp")))
                    gs_effect_set_float(param, context->deck ? context->deck->currentAmplitude() : 0.0f);

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
    // Az aktív belső forrás átadása az OBS-nek az automatikus dekódolási tickhez
    deck_info.enum_active_sources = deck_enum_active_sources;
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
