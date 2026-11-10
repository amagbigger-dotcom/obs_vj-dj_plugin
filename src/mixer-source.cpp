#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/effect.h>
#include <graphics/matrix4.h>
#include <media-io/audio-io.h>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>

#include <QImage>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QString>

#include "deck-engine.hpp"

extern "C" {

struct MixerSource {
    obs_source_t *source = nullptr;
    int channel = 0;

    gs_effect_t *effect = nullptr;
    gs_texrender_t *deck_a = nullptr;
    gs_texrender_t *deck_b = nullptr;

    gs_texture_t *vis_tex_a = nullptr;
    gs_texture_t *vis_tex_b = nullptr;
};

static const char *MIXER_EFFECT = R"(
uniform float4x4 ViewProj;
uniform texture2d image_a;
uniform texture2d image_b;
uniform float4 gains_a;
uniform float4 gains_b;
uniform float saturation_a;
uniform float saturation_b;
uniform float crossfade;
uniform float transition_mode;

sampler_state sampler_a {
    Filter = Linear;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler_state sampler_b {
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

float4 colorize(float4 color, float4 gains, float saturation)
{
    color.rgb *= gains.rgb;

    float luma = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    color.rgb = lerp(luma.xxx, color.rgb, saturation);

    return saturate(color);
}

float4 PS(VertData v) : TARGET
{
    float4 a = colorize(image_a.Sample(sampler_a, v.uv),
                        gains_a, saturation_a);
    float4 b = colorize(image_b.Sample(sampler_b, v.uv),
                        gains_b, saturation_b);

    // 0: linear fade
    // 1: horizontal wipe
    // 2: additive blend
    // 3: hard cut
    if (transition_mode > 2.5)
        return crossfade < 0.5 ? a : b;

    if (transition_mode > 0.5 && transition_mode < 1.5)
        return v.uv.x < crossfade ? b : a;

    if (transition_mode > 1.5 && transition_mode < 2.5)
        return saturate(a * (1.0 - crossfade) + b * crossfade);

    return lerp(a, b, crossfade);
}

technique Draw {
    pass {
        vertex_shader = VSDefault(v);
        pixel_shader = PS(v);
    }
}
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

static void render_deck_visualizer(MixerSource *context,
                                   gs_texrender_t *target,
                                   DeckPlayer &deck,
                                   bool is_deck_a,
                                   uint32_t width,
                                   uint32_t height)
{
    constexpr int kW = 960;
    constexpr int kH = 540;

    gs_texture_t *&vis_tex = is_deck_a ? context->vis_tex_a : context->vis_tex_b;
    if (!vis_tex) {
        vis_tex = gs_texture_create(kW, kH, GS_RGBA, 1, nullptr, GS_DYNAMIC);
    }
    if (!vis_tex)
        return;

    QImage img(kW, kH, QImage::Format_RGBA8888);
    img.fill(QColor(8, 11, 16));

    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);

        // 1. Dark Tech Background Gradient
        QLinearGradient bg(0, 0, 0, kH);
        bg.setColorAt(0.0, QColor(6, 8, 12));
        bg.setColorAt(0.5, QColor(12, 16, 24));
        bg.setColorAt(1.0, QColor(6, 8, 12));
        p.fillRect(0, 0, kW, kH, bg);

        // Subtle Tech Grid
        p.setPen(QColor(24, 32, 46, 70));
        for (int x = 0; x < kW; x += 40)
            p.drawLine(x, 0, x, kH);
        for (int y = 0; y < kH; y += 40)
            p.drawLine(0, y, kW, y);

        const QColor theme = is_deck_a ? QColor(0, 210, 255) : QColor(255, 115, 0);

        // 2. Top Header (Deck Name + Track Title)
        p.setFont(QFont(QStringLiteral("Segoe UI"), 22, QFont::Bold));
        p.setPen(theme);
        p.drawText(40, 50, is_deck_a ? QStringLiteral("DECK A") : QStringLiteral("DECK B"));

        const QString track_name = deck.fileName().isEmpty()
            ? (is_deck_a ? QStringLiteral("Deck A - Ready") : QStringLiteral("Deck B - Ready"))
            : deck.fileName();

        p.setFont(QFont(QStringLiteral("Segoe UI"), 16, QFont::DemiBold));
        p.setPen(QColor(220, 230, 245));
        p.drawText(160, 48, track_name);

        // 3. Center Vinyl Record Turntable (Rotates smoothly while playing!)
        const int cx = kW / 2;
        const int cy = 250;
        const int r_outer = 135;

        // Outer turntable bezel & glow
        p.setPen(QPen(theme, 2));
        p.setBrush(QColor(14, 17, 24));
        p.drawEllipse(QPoint(cx, cy), r_outer + 8, r_outer + 8);

        // Vinyl body with concentric grooves
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(18, 20, 25));
        p.drawEllipse(QPoint(cx, cy), r_outer, r_outer);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(28, 33, 42), 1));
        for (int gr = 55; gr < r_outer - 5; gr += 12)
            p.drawEllipse(QPoint(cx, cy), gr, gr);

        // Center vinyl label
        p.setBrush(theme);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(cx, cy), 45, 45);

        p.setBrush(QColor(8, 10, 14));
        p.drawEllipse(QPoint(cx, cy), 12, 12);

        // Vinyl needle / rotation marker
        const int64_t cur_ms = deck.currentTimeMs();
        const double rot_deg = deck.isPlaying() ? (std::fmod(static_cast<double>(cur_ms), 1800.0) * 360.0 / 1800.0) : 0.0;
        const double rad = rot_deg * 3.14159265 / 180.0;
        const int mx = cx + static_cast<int>(std::cos(rad) * (r_outer - 15));
        const int my = cy + static_cast<int>(std::sin(rad) * (r_outer - 15));
        p.setPen(QPen(QColor(255, 255, 255, 230), 3));
        p.drawLine(cx, cy, mx, my);

        // Label Text
        p.setFont(QFont(QStringLiteral("Segoe UI"), 7, QFont::Bold));
        p.setPen(QColor(255, 255, 255));
        p.drawText(QRect(cx - 35, cy - 20, 70, 16), Qt::AlignCenter, QStringLiteral("TRAKTOR"));
        p.drawText(QRect(cx - 35, cy + 6, 70, 16), Qt::AlignCenter, is_deck_a ? QStringLiteral("DECK A") : QStringLiteral("DECK B"));

        // 4. Real-time Animated Audio Spectrum Bars at bottom
        constexpr int kNumBars = 48;
        const int bar_w = 12;
        const int bar_gap = 4;
        const int total_spec_w = kNumBars * (bar_w + bar_gap);
        const int spec_start_x = (kW - total_spec_w) / 2;
        const int spec_base_y = kH - 85;

        const std::vector<float> peaks = deck.detailedSamples();
        const size_t cur_idx = static_cast<size_t>(cur_ms / 10);
        const float cur_energy = (cur_idx < peaks.size()) ? peaks[cur_idx] : (deck.isPlaying() ? 0.5f : 0.05f);

        for (int b = 0; b < kNumBars; ++b) {
            const int bx = spec_start_x + b * (bar_w + bar_gap);
            const double dist_from_mid = std::abs(b - kNumBars / 2) / static_cast<double>(kNumBars / 2);
            const double freq_weight = 1.0 - dist_from_mid * 0.6;
            const double anim = std::sin(static_cast<double>(cur_ms) / 100.0 + b * 0.4) * 0.25 + 0.75;
            float bar_val = static_cast<float>(cur_energy * freq_weight * anim);
            if (!deck.isPlaying())
                bar_val = 0.03f;
            bar_val = std::clamp(bar_val, 0.03f, 1.0f);

            const int bar_h = static_cast<int>(bar_val * 70.0f);

            QLinearGradient bar_grad(bx, spec_base_y, bx, spec_base_y - bar_h);
            bar_grad.setColorAt(0.0, theme);
            bar_grad.setColorAt(0.7, QColor(255, 200, 0));
            bar_grad.setColorAt(1.0, QColor(255, 50, 50));

            p.fillRect(bx, spec_base_y - bar_h, bar_w, bar_h, bar_grad);
        }

        // 5. Timecode and BPM Info Bar
        const int64_t dur_ms = deck.durationMs();
        const int cur_sec = static_cast<int>(cur_ms / 1000);
        const int dur_sec = static_cast<int>(dur_ms / 1000);
        const int rem_sec = std::max(0, dur_sec - cur_sec);

        const QString time_str = QStringLiteral("%1:%2 / -%3:%4")
            .arg(cur_sec / 60, 2, 10, QChar('0'))
            .arg(cur_sec % 60, 2, 10, QChar('0'))
            .arg(rem_sec / 60, 2, 10, QChar('0'))
            .arg(rem_sec % 60, 2, 10, QChar('0'));

        const QString bpm_str = QStringLiteral("%1 BPM (%2%)")
            .arg(deck.effectiveBpm(), 0, 'f', 1)
            .arg(deck.speed());

        p.setFont(QFont(QStringLiteral("Segoe UI"), 16, QFont::Bold));
        p.setPen(theme);
        p.drawText(60, kH - 25, time_str);

        p.setPen(QColor(255, 204, 0));
        p.drawText(kW - 260, kH - 25, bpm_str);
    }

    gs_texture_set_image(vis_tex, img.constBits(), kW * 4, false);

    if (gs_texrender_begin(target, width, height)) {
        gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
        vec4 clear = {};
        gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
        gs_draw_sprite(vis_tex, 0, width, height);
        gs_texrender_end(target);
    }
}

static bool render_to_texture(MixerSource *context,
                              gs_texrender_t *target,
                              DeckPlayer &deck,
                              bool is_deck_a,
                              uint32_t width,
                              uint32_t height)
{
    if (!target || width == 0 || height == 0)
        return false;

    obs_source_t *source = deck.source();
    const uint32_t src_w = source ? obs_source_get_width(source) : 0;
    const uint32_t src_h = source ? obs_source_get_height(source) : 0;

    if (src_w > 0 && src_h > 0) {
        // Render video frame
        if (!gs_texrender_begin(target, width, height))
            return false;

        gs_ortho(0.0f, static_cast<float>(width),
                 0.0f, static_cast<float>(height),
                 -100.0f, 100.0f);

        vec4 clear = {};
        gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);

        gs_matrix_push();
        if (src_w != width || src_h != height) {
            const float sx = static_cast<float>(width) / static_cast<float>(src_w);
            const float sy = static_cast<float>(height) / static_cast<float>(src_h);
            gs_matrix_scale3f(sx, sy, 1.0f);
        }

        obs_source_video_render(source);
        gs_matrix_pop();
        gs_texrender_end(target);
        return true;
    } else {
        // Audio-only or no video stream: render dynamic DJ Console visualizer
        render_deck_visualizer(context, target, deck, is_deck_a, width, height);
        return true;
    }
}

static void set_color_params(MixerSource *context,
                             const DeckPlayer &deck_a,
                             const DeckPlayer &deck_b,
                             float crossfade,
                             int transition_mode)
{
    gs_eparam_t *param = nullptr;

    vec4 gains_a;
    vec4 gains_b;

    vec4_set(&gains_a, deck_a.red(), deck_a.green(), deck_a.blue(), 1.0f);
    vec4_set(&gains_b, deck_b.red(), deck_b.green(), deck_b.blue(), 1.0f);

    if ((param = gs_effect_get_param_by_name(context->effect, "gains_a")))
        gs_effect_set_vec4(param, &gains_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "gains_b")))
        gs_effect_set_vec4(param, &gains_b);

    if ((param = gs_effect_get_param_by_name(context->effect, "saturation_a")))
        gs_effect_set_float(param, deck_a.saturation());

    if ((param = gs_effect_get_param_by_name(context->effect, "saturation_b")))
        gs_effect_set_float(param, deck_b.saturation());

    if ((param = gs_effect_get_param_by_name(context->effect, "crossfade")))
        gs_effect_set_float(param, crossfade);

    if ((param = gs_effect_get_param_by_name(context->effect, "transition_mode")))
        gs_effect_set_float(param, static_cast<float>(transition_mode));
}

static const char *mixer_get_name(void *)
{
    return "Traktor S2 VJ Mixer Source";
}

static void *mixer_create(obs_data_t *, obs_source_t *source)
{
    auto *context = new MixerSource();
    context->source = source;
    return context;
}

static void mixer_destroy(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context)
        return;

    obs_enter_graphics();

    if (context->vis_tex_a)
        gs_texture_destroy(context->vis_tex_a);

    if (context->vis_tex_b)
        gs_texture_destroy(context->vis_tex_b);

    if (context->deck_a)
        gs_texrender_destroy(context->deck_a);

    if (context->deck_b)
        gs_texrender_destroy(context->deck_b);

    if (context->effect)
        gs_effect_destroy(context->effect);

    obs_leave_graphics();
    delete context;
}

static void draw_mixer(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();

    if (!context || !decks || !context->effect)
        return;

    uint32_t width = obs_source_get_width(context->source);
    uint32_t height = obs_source_get_height(context->source);

    if (width == 0)
        width = 1920;
    if (height == 0)
        height = 1080;

    DeckPlayer &deck_a = decks->deckA();
    DeckPlayer &deck_b = decks->deckB();

    deck_a.updateLoop();
    deck_b.updateLoop();

    const bool rendered_a =
        render_to_texture(context, context->deck_a, deck_a, true, width, height);
    const bool rendered_b =
        render_to_texture(context, context->deck_b, deck_b, false, width, height);

    gs_texture_t *texture_a =
        rendered_a ? gs_texrender_get_texture(context->deck_a) : nullptr;
    gs_texture_t *texture_b =
        rendered_b ? gs_texrender_get_texture(context->deck_b) : nullptr;

    if (!texture_a && !texture_b) {
        vec4 clear = {};
        gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
        return;
    }

    const int output_mode = decks->obsOutputMode();
    if (output_mode == 1 && texture_a) {
        gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
        while (gs_effect_loop(default_effect, "Draw")) {
            gs_draw_sprite(texture_a, 0, width, height);
        }
        return;
    } else if (output_mode == 2 && texture_b) {
        gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
        while (gs_effect_loop(default_effect, "Draw")) {
            gs_draw_sprite(texture_b, 0, width, height);
        }
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

    gs_eparam_t *param = nullptr;

    if ((param = gs_effect_get_param_by_name(context->effect, "image_a")))
        gs_effect_set_texture(param, texture_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "image_b")))
        gs_effect_set_texture(param, texture_b);

    set_color_params(context, *color_a, *color_b, xfade,
                     decks->transitionMode());

    // Bind current projection matrix to ViewProj
    matrix4 viewproj;
    gs_matrix_get(&viewproj);
    param = gs_effect_get_param_by_name(context->effect, "ViewProj");
    if (param)
        gs_effect_set_matrix4(param, &viewproj);

    while (gs_effect_loop(context->effect, "Draw")) {
        gs_draw_sprite(texture_a, 0, width, height);
    }
}

static void mixer_video_render(void *data, gs_effect_t *)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context || !vdj_decks())
        return;

    ensure_resources(context);
    draw_mixer(context);
}

static uint32_t mixer_get_width(void *)
{
    return 1920;
}

static uint32_t mixer_get_height(void *)
{
    return 1080;
}

static const char *monitor_get_name(void *)
{
    return "Traktor S2 Deck Monitor";
}

static void *monitor_create(obs_data_t *, obs_source_t *source)
{
    auto *context = new MixerSource();
    context->source = source;

    const char *id = obs_source_get_id(source);
    context->channel =
        (id && std::strcmp(id, "vdj_deck_b_source") == 0) ? 1 : 0;

    return context;
}

static void monitor_video_render(void *data, gs_effect_t *)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();

    if (!context || !decks)
        return;

    ensure_resources(context);
    if (!context->effect)
        return;

    const bool is_deck_a = (context->channel == 0);
    DeckPlayer &deck = is_deck_a ? decks->deckA() : decks->deckB();

    deck.updateLoop();

    uint32_t width = obs_source_get_width(context->source);
    uint32_t height = obs_source_get_height(context->source);

    if (width == 0)
        width = 1920;
    if (height == 0)
        height = 1080;

    if (!render_to_texture(context, context->deck_a, deck, is_deck_a, width, height))
        return;

    gs_texture_t *texture = gs_texrender_get_texture(context->deck_a);
    if (!texture)
        return;

    gs_eparam_t *param = nullptr;

    if ((param = gs_effect_get_param_by_name(context->effect, "image_a")))
        gs_effect_set_texture(param, texture);

    if ((param = gs_effect_get_param_by_name(context->effect, "image_b")))
        gs_effect_set_texture(param, texture);

    set_color_params(context, deck, deck, 0.0f, 0);

    matrix4 viewproj;
    gs_matrix_get(&viewproj);
    param = gs_effect_get_param_by_name(context->effect, "ViewProj");
    if (param)
        gs_effect_set_matrix4(param, &viewproj);

    while (gs_effect_loop(context->effect, "Draw")) {
        gs_draw_sprite(texture, 0, width, height);
    }
}

#ifndef AUDIO_OUTPUT_FRAMES
#define AUDIO_OUTPUT_FRAMES 1024
#endif

#ifndef MAX_AUDIO_MIXES
#define MAX_AUDIO_MIXES 6
#endif

static bool mixer_audio_render(void *data, uint64_t *ts_out,
                               struct obs_source_audio_mix *audio_output,
                               uint32_t mixers, size_t channels,
                               size_t sample_rate)
{
    UNUSED_PARAMETER(data);

    DeckPair *decks = vdj_decks();
    if (!decks || !ts_out || !audio_output)
        return false;

    DeckPlayer &deck_a = decks->deckA();
    DeckPlayer &deck_b = decks->deckB();
    obs_source_t *source_a = deck_a.source();
    obs_source_t *source_b = deck_b.source();

    bool has_a = source_a && deck_a.isPlaying();
    bool has_b = source_b && deck_b.isPlaying();

    if (!has_a && !has_b)
        return false;

    struct obs_source_audio_mix mix_a = {};
    struct obs_source_audio_mix mix_b = {};

    uint64_t ts_a = 0;
    uint64_t ts_b = 0;

    if (has_a) {
        ts_a = obs_source_get_audio_timestamp(source_a);
        if (ts_a)
            obs_source_get_audio_mix(source_a, &mix_a);
        else
            has_a = false;
    }

    if (has_b) {
        ts_b = obs_source_get_audio_timestamp(source_b);
        if (ts_b)
            obs_source_get_audio_mix(source_b, &mix_b);
        else
            has_b = false;
    }

    if (!has_a && !has_b)
        return false;

    *ts_out = (has_a && has_b) ? std::min(ts_a, ts_b) : (has_a ? ts_a : ts_b);

    // Process 3-Band EQ and DJ Filter DSP ONCE per channel into local temporary buffers:
    const size_t num_ch = std::min<size_t>(channels, 2);
    float proc_a[2][AUDIO_OUTPUT_FRAMES];
    float proc_b[2][AUDIO_OUTPUT_FRAMES];
    std::memset(proc_a, 0, sizeof(proc_a));
    std::memset(proc_b, 0, sizeof(proc_b));

    if (has_a && mix_a.output[0].data[0]) {
        for (size_t ch = 0; ch < num_ch; ch++) {
            if (mix_a.output[0].data[ch]) {
                std::memcpy(proc_a[ch], mix_a.output[0].data[ch], AUDIO_OUTPUT_FRAMES * sizeof(float));
                deck_a.processAudioDsp(proc_a[ch], AUDIO_OUTPUT_FRAMES, ch, sample_rate);
            }
        }
    }

    if (has_b && mix_b.output[0].data[0]) {
        for (size_t ch = 0; ch < num_ch; ch++) {
            if (mix_b.output[0].data[ch]) {
                std::memcpy(proc_b[ch], mix_b.output[0].data[ch], AUDIO_OUTPUT_FRAMES * sizeof(float));
                deck_b.processAudioDsp(proc_b[ch], AUDIO_OUTPUT_FRAMES, ch, sample_rate);
            }
        }
    }

    const int output_mode = decks->obsOutputMode();
    const float crossfade = static_cast<float>(decks->crossfader());
    const float vol_a = deck_a.volume() * deck_a.gain();
    const float vol_b = deck_b.volume() * deck_b.gain();

    const float gain_a = (output_mode == 2) ? 0.0f : ((output_mode == 1) ? vol_a : vol_a * (1.0f - crossfade));
    const float gain_b = (output_mode == 1) ? 0.0f : ((output_mode == 2) ? vol_b : vol_b * crossfade);

    for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
        if ((mixers & (1U << mix)) == 0)
            continue;

        for (size_t ch = 0; ch < channels; ch++) {
            float *out = audio_output->output[mix].data[ch];
            const size_t ch_idx = ch % 2;
            for (size_t f = 0; f < AUDIO_OUTPUT_FRAMES; f++) {
                out[f] = proc_a[ch_idx][f] * gain_a + proc_b[ch_idx][f] * gain_b;
            }
        }
    }

    return true;
}

static bool monitor_audio_render(void *data, uint64_t *ts_out,
                                 struct obs_source_audio_mix *audio_output,
                                 uint32_t mixers, size_t channels,
                                 size_t sample_rate)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks || !ts_out || !audio_output)
        return false;

    DeckPlayer &deck = context->channel == 0 ? decks->deckA() : decks->deckB();
    obs_source_t *source = deck.source();
    if (!source || !deck.isPlaying())
        return false;

    uint64_t ts = obs_source_get_audio_timestamp(source);
    if (!ts)
        return false;

    struct obs_source_audio_mix mix = {};
    obs_source_get_audio_mix(source, &mix);
    *ts_out = ts;

    const size_t num_ch = std::min<size_t>(channels, 2);
    float proc[2][AUDIO_OUTPUT_FRAMES];
    std::memset(proc, 0, sizeof(proc));

    if (mix.output[0].data[0]) {
        for (size_t ch = 0; ch < num_ch; ch++) {
            if (mix.output[0].data[ch]) {
                std::memcpy(proc[ch], mix.output[0].data[ch], AUDIO_OUTPUT_FRAMES * sizeof(float));
                deck.processAudioDsp(proc[ch], AUDIO_OUTPUT_FRAMES, ch, sample_rate);
            }
        }
    }

    const float vol = deck.volume() * deck.gain();

    for (size_t m = 0; m < MAX_AUDIO_MIXES; m++) {
        if ((mixers & (1U << m)) == 0)
            continue;

        for (size_t ch = 0; ch < channels; ch++) {
            float *out = audio_output->output[m].data[ch];
            const size_t ch_idx = ch % 2;
            for (size_t f = 0; f < AUDIO_OUTPUT_FRAMES; f++) {
                out[f] = proc[ch_idx][f] * vol;
            }
        }
    }

    return true;
}

static void mixer_enum_active_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks)
        return;

    obs_source_t *parent = context->source;
    obs_source_t *source_a = decks->deckA().source();
    obs_source_t *source_b = decks->deckB().source();

    if (source_a)
        cb(parent, source_a, param);
    if (source_b)
        cb(parent, source_b, param);
}

static void mixer_enum_all_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks)
        return;

    obs_source_t *parent = context->source;
    obs_source_t *source_a = decks->deckA().source();
    obs_source_t *source_b = decks->deckB().source();

    if (source_a)
        cb(parent, source_a, param);
    if (source_b)
        cb(parent, source_b, param);
}

static void monitor_enum_active_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks)
        return;

    obs_source_t *parent = context->source;
    DeckPlayer &deck = (context->channel == 0) ? decks->deckA() : decks->deckB();
    obs_source_t *child = deck.source();

    if (child)
        cb(parent, child, param);
}

static void monitor_enum_all_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (!context || !decks)
        return;

    obs_source_t *parent = context->source;
    DeckPlayer &deck = (context->channel == 0) ? decks->deckA() : decks->deckB();
    obs_source_t *child = deck.source();

    if (child)
        cb(parent, child, param);
}

static obs_properties_t *mixer_properties(void *)
{
    obs_properties_t *props = obs_properties_create();

    obs_property_t *transition = obs_properties_add_list(
        props, "transition", "FX Transition",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(transition, "Linear Fade", 0);
    obs_property_list_add_int(transition, "Horizontal Wipe", 1);
    obs_property_list_add_int(transition, "Additive Blend", 2);
    obs_property_list_add_int(transition, "Hard Cut", 3);

    return props;
}

static void mixer_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "transition", 0);
}

void vdj_register_sources(void)
{
    obs_source_info master = {};
    master.id = "vdj_master_mixer";
    master.type = OBS_SOURCE_TYPE_INPUT;
    master.output_flags =
        OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE;
    master.get_name = mixer_get_name;
    master.create = mixer_create;
    master.destroy = mixer_destroy;
    master.video_render = mixer_video_render;
    master.audio_render = mixer_audio_render;
    master.enum_active_sources = mixer_enum_active_sources;
    master.enum_all_sources = mixer_enum_all_sources;
    master.get_width = mixer_get_width;
    master.get_height = mixer_get_height;
    master.get_properties = mixer_properties;
    master.get_defaults = mixer_defaults;
    obs_register_source_s(&master, sizeof(master));

    obs_source_info deck_a = {};
    deck_a.id = "vdj_deck_a_source";
    deck_a.type = OBS_SOURCE_TYPE_INPUT;
    deck_a.output_flags =
        OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE;
    deck_a.get_name = monitor_get_name;
    deck_a.create = monitor_create;
    deck_a.destroy = mixer_destroy;
    deck_a.video_render = monitor_video_render;
    deck_a.audio_render = monitor_audio_render;
    deck_a.enum_active_sources = monitor_enum_active_sources;
    deck_a.enum_all_sources = monitor_enum_all_sources;
    deck_a.get_width = mixer_get_width;
    deck_a.get_height = mixer_get_height;
    obs_register_source_s(&deck_a, sizeof(deck_a));

    obs_source_info deck_b = deck_a;
    deck_b.id = "vdj_deck_b_source";
    deck_b.enum_active_sources = monitor_enum_active_sources;
    deck_b.enum_all_sources = monitor_enum_all_sources;
    obs_register_source_s(&deck_b, sizeof(deck_b));
}

} // extern "C"
