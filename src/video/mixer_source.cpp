#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/graphics.h>
#include <graphics/effect.h>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <media-io/audio-io.h>
#include <util/platform.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "engine/deck_engine.hpp"
#include "effects/vj_config.hpp"
#include "effects/fluid_flow.hpp"
#include "effects/crossfade_fx_config.hpp"

extern "C" {

// =============================================================================
// Mester Mixer forrás struktúra (Master Mixer Source Structure & Audio Thread)
// =============================================================================
struct MixerSource {
    obs_source_t *source = nullptr;
    int channel = 0;

    gs_effect_t *effect = nullptr;
    gs_effect_t *fluid_effect = nullptr;
    uint32_t loaded_vj_effect_version = 0;
    gs_texture_t *dummy_tex = nullptr;
    gs_texrender_t *deck_a = nullptr;
    gs_texrender_t *deck_b = nullptr;

    uint64_t next_audio_ts = 0;
    std::thread audio_thread;
    std::atomic<bool> audio_thread_running{false};
    std::atomic<bool> need_monitor_wake_{true};
};

static const char *MIXER_EFFECT_PART1 = R"(
uniform float4x4 ViewProj;
uniform texture2d image_a;
uniform texture2d image_b;
uniform float4 gains_a;
uniform float4 gains_b;
uniform float saturation_a;
uniform float saturation_b;
uniform float crossfade;
uniform float transition_mode;

uniform float vol_fx_mode_a;
uniform float vol_fx_val_a;
uniform float vol_fx_mode_b;
uniform float vol_fx_val_b;

uniform float cf_fx_mode;
uniform float cf_fx_mode_prev;
uniform float cf_trans_t;
uniform float cf_fx_mid;
uniform float cf_blend_boost;
uniform float cf_param_intensity;
uniform float cf_param_rate;
uniform float anim_time;
uniform float cf_curve;
uniform float cf_smooth;
uniform float cf_p0;
uniform float cf_p1;
uniform float cf_p2;
uniform float cf_p3;
uniform float cf_p4;
uniform float cf_p5;
uniform float cf_p6;
uniform float cf_p7;

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

float4 colorize(float4 color, float4 gains, float saturation)
{
    color.rgb *= gains.rgb;
    float luma = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    color.rgb = lerp(luma.xxx, color.rgb, saturation);
    color.a = 1.0;
    return saturate(color);
}

float2 apply_cf_fx(float2 uv, float mode, float mid, float is_b, float intensity, float rate)
{
    if (mode < 0.5 || mid <= 0.001)
        return uv;

    float2 p = uv - float2(0.5, 0.5);
    float r = length(p);

    if (mode > 0.5 && mode < 1.5) {
        // Mode 1: Spirális Örvény (Swirl / Vortex)
        float twist = cf_p0;
        float radius = max(cf_p1, 0.1);
        float dir_mult = (cf_p2 >= 0.0) ? 1.0 : -1.0;
        float zoom = max(cf_p3, 0.1);
        float2 center = float2(0.5, 0.5);
        float2 p_swirl = (uv - center) / zoom;
        float r_swirl = length(p_swirl);
        float dir = ((is_b > 0.5) ? -1.0 : 1.0) * dir_mult;
        float angle = dir * mid * twist * (1.0 - saturate(r_swirl * 2.0 / radius)) * intensity;
        float s = sin(angle);
        float c = cos(angle);
        float2 rot_p = float2(p_swirl.x * c - p_swirl.y * s, p_swirl.x * s + p_swirl.y * c);
        return rot_p * zoom + center;
    }
    else if (mode > 1.5 && mode < 2.5) {
        // Mode 2: Halszem Optika (Fisheye Lens)
        float distortion = cf_p0;
        float zoom = max(cf_p1, 0.1);
        float vignette = cf_p2;
        float2 center = float2(0.5, 0.5);
        float2 p_fish = (uv - center) / zoom;
        float r_fish2 = dot(p_fish, p_fish);
        float dist_val = 1.0 + mid * distortion * r_fish2 * intensity * rate;
        return (p_fish * dist_val) * zoom + center;
    }
    else if (mode > 4.5 && mode < 5.5) {
        // Mode 5: Pixelesedés (Pixelate / Mosaic)
        float max_g = cf_p0;
        float min_g = cf_p1;
        float asp = max(cf_p2, 0.1);
        float border = cf_p3;
        float tiles = lerp(max_g / max(rate, 0.1), min_g / max(rate, 0.1), saturate(mid * intensity));
        float2 blk = float2(tiles, tiles * asp);
        float2 pix_uv = floor(uv * blk) / blk;
        if (border > 0.001) {
            float2 cell = frac(uv * blk);
            if (cell.x < border || cell.y < border) pix_uv = float2(0.0, 0.0);
        }
        return pix_uv;
    }
    else if (mode > 6.5 && mode < 7.5) {
        // Mode 7: Folyadék Hullámzás (Liquid Ripple Warp)
        float freq = cf_p0;
        float spd = cf_p1;
        float amp_param = cf_p2;
        float asp_x = max(cf_p3, 0.1);
        float decay = max(cf_p4, 0.01);
        float surge = cf_p5;
        float2 ap = p;
        ap.x *= asp_x;
        float ar = length(ap);
        float wave = sin(ar * freq * rate - anim_time * spd * rate - crossfade * surge);
        float amp = mid * amp_param * intensity * (1.0 / (ar + decay));
        float2 warp = (ap / (ar + 0.0001)) * wave * amp;
        warp.x /= asp_x;
        return uv + warp;
    }
    else if (mode > 7.5 && mode < 8.5) {
        // Mode 8: Kaleidoszkóp (Mirror Fold) - N-tükrös tiszta poláris szimmetria belső képeltolással
        float sides = max(floor(cf_p0 + 0.5), 2.0);
        float zoom = max(cf_p1, 0.1);
        float pan_x = cf_p2;
        float pan_y = cf_p3;
        float fold_angle = cf_p4;
        float softness = cf_p5;
        float contrast = max(cf_p6, 0.1);
        float blend_p = cf_p7;

        float2 kp = uv - float2(0.5, 0.5);
        kp.x *= 1.7777778; // 16:9 aránykorrekció a tökéletes körszimmetriához
        float kr = length(kp);
        float ka = atan2(kp.y, kp.x) + fold_angle; // Statikus dőlésszög - NINCS automatikus forgás!
        float tau = 6.2831853;
        float seg = tau / sides;
        // Folytonos háromszöghullámú poláris hajtás (varratmentes tükrözés):
        float a_mod = frac(ka / seg);
        if (a_mod < 0.0) a_mod += 1.0;
        float fold = abs(a_mod * 2.0 - 1.0);
        if (softness > 0.01) fold = smoothstep(0.0, 1.0, fold);
        float folded_a = fold * (seg * 0.5);
        float2 folded_dir = float2(cos(folded_a), sin(folded_a));
        float2 kal_uv = folded_dir * (kr / zoom);
        kal_uv.x /= 1.7777778; // inverz képarány
        kal_uv += float2(0.5, 0.5) + float2(pan_x, pan_y); // Középpont fixen (0.5, 0.5) + belső képeltolás
        return lerp(uv, kal_uv, saturate(mid * blend_p * intensity));
    }
    else if (mode > 8.5 && mode < 9.5) {
        // Mode 9: Harmonikus Pásztázás (Scan Dispersion) - Finom szimmetrikus hullámzó eltolás
        float bands = max(cf_p0, 2.0);
        float spd = cf_p1;
        float shift_amt = cf_p2;
        float softness = max(cf_p3, 0.05);
        float asp = max(cf_p5, 0.1);
        float blend_p = cf_p7;

        float y_phase = uv.y * bands * rate + anim_time * spd * rate;
        float wave = sin(y_phase);
        float shift = sin(wave * 3.14159265 * 0.5) * mid * shift_amt * intensity * softness;
        float2 p_scan = uv - float2(0.5, 0.5);
        p_scan.x *= asp;
        p_scan.x *= (1.0 + shift);
        p_scan.x /= asp;
        float2 disp_uv = float2(0.5, 0.5) + p_scan;
        return lerp(uv, disp_uv, saturate(mid * blend_p * intensity));
    }
    else if (mode > 9.5 && mode < 10.5) {
        // Mode 10: CRT / VHS Scanline (Curvature)
        float curve = cf_p2;
        float r2 = dot(p, p);
        return p * (1.0 + r2 * mid * curve * intensity * rate) + float2(0.5, 0.5);
    }

    return uv;
}
)";

static const char *MIXER_EFFECT_PART2 = R"(
float get_shape_wipe_mask(float fx_mode, float fx_val, float2 uv)
{
    if (fx_mode < 3.5 || fx_val <= 0.001)
        return 1.0;

    float2 center = float2(0.5, 0.5);
    float2 p = uv - center;
    float2 p_ar = p * float2(1.7777778, 1.0);

    if (fx_mode > 3.5 && fx_mode < 4.5) {
        // Mode 4: Kör Szűkülés (Iris Circle)
        float r = length(p_ar);
        float cur_r = (1.0 - fx_val) * 1.05;
        return 1.0 - smoothstep(max(cur_r - 0.02, 0.0), cur_r + 0.02, r);
    }
    else if (fx_mode > 4.5 && fx_mode < 5.5) {
        // Mode 5: 16:9 Téglalap (Box Iris)
        float2 ap = abs(p);
        float d = max(ap.x, ap.y * 1.7777778);
        float cur_box = (1.0 - fx_val) * 0.52;
        return 1.0 - smoothstep(max(cur_box - 0.02, 0.0), cur_box + 0.02, d);
    }
    else if (fx_mode > 5.5 && fx_mode < 6.5) {
        // Mode 6: 1:1 Négyzet (Square Iris)
        float2 ap = abs(p_ar);
        float d = max(ap.x, ap.y);
        float cur_sq = (1.0 - fx_val) * 0.90;
        return 1.0 - smoothstep(max(cur_sq - 0.02, 0.0), cur_sq + 0.02, d);
    }
    else if (fx_mode > 6.5 && fx_mode < 7.5) {
        // Mode 7: Rombusz / Gyémánt (Diamond)
        float2 ap = abs(p_ar);
        float d = ap.x + ap.y;
        float cur_dia = (1.0 - fx_val) * 1.35;
        return 1.0 - smoothstep(max(cur_dia - 0.025, 0.0), cur_dia + 0.025, d);
    }
    else if (fx_mode > 7.5 && fx_mode < 8.5) {
        // Mode 8: Vízszintes Lamellák (Venetian Stripes)
        float stripes_y = frac(uv.y * 12.0);
        float d = abs(stripes_y - 0.5) * 2.0;
        float cur_s = (1.0 - fx_val);
        return 1.0 - smoothstep(max(cur_s - 0.03, 0.0), cur_s + 0.03, d);
    }
    else if (fx_mode > 8.5 && fx_mode < 9.5) {
        // Mode 9: Függőleges Lamellák (Vertical Slices)
        float slices_x = frac(uv.x * 18.0);
        float d = abs(slices_x - 0.5) * 2.0;
        float cur_s = (1.0 - fx_val);
        return 1.0 - smoothstep(max(cur_s - 0.03, 0.0), cur_s + 0.03, d);
    }
    else if (fx_mode > 9.5 && fx_mode < 10.5) {
        // Mode 10: Zoom Középre (Zoom Out to Center)
        float zoom_scale = max(1.0 - fx_val * 0.96, 0.01);
        float2 z_uv = p / zoom_scale + center;
        float mask_x = smoothstep(0.0, 0.015, z_uv.x) * (1.0 - smoothstep(0.985, 1.0, z_uv.x));
        float mask_y = smoothstep(0.0, 0.015, z_uv.y) * (1.0 - smoothstep(0.985, 1.0, z_uv.y));
        return saturate(mask_x * mask_y);
    }

    return 1.0;
}

float4 sample_deck_with_vol_fx(texture2d img, float2 uv, float fx_mode, float fx_val, float4 gains, float sat)
{
    float2 sample_uv = uv;
    if (fx_mode > 9.5 && fx_mode < 10.5 && fx_val > 0.001) {
        float zoom_scale = max(1.0 - fx_val * 0.96, 0.01);
        sample_uv = (uv - float2(0.5, 0.5)) / zoom_scale + float2(0.5, 0.5);
    }

    float4 col;
    if (fx_mode > 2.5 && fx_mode < 3.5 && fx_val > 0.01) {
        // Mode 3: Soft Focus / Homályosítás (Multi-tap Box/Gaussian Blur)
        float2 off = float2(fx_val * 0.012, fx_val * 0.012);
        float4 sum = img.Sample(def_sampler, sample_uv);
        sum += img.Sample(def_sampler, sample_uv + float2(off.x, 0));
        sum += img.Sample(def_sampler, sample_uv - float2(off.x, 0));
        sum += img.Sample(def_sampler, sample_uv + float2(0, off.y));
        sum += img.Sample(def_sampler, sample_uv - float2(0, off.y));
        sum += img.Sample(def_sampler, sample_uv + float2(off.x * 0.7, off.y * 0.7));
        sum += img.Sample(def_sampler, sample_uv - float2(off.x * 0.7, off.y * 0.7));
        sum += img.Sample(def_sampler, sample_uv + float2(-off.x * 0.7, off.y * 0.7));
        sum += img.Sample(def_sampler, sample_uv + float2(off.x * 0.7, -off.y * 0.7));
        col = sum / 9.0;
    } else {
        col = img.Sample(def_sampler, sample_uv);
    }

    // Színkorrekció (RGB Gain & Saturation)
    col = colorize(col, gains, sat);

    // Fade to Black vagy Fade to White
    if (fx_mode > 0.5 && fx_mode < 1.5) {
        // Mode 1: Fade to Black (Sötétedés / Dimming)
        col.rgb *= saturate(1.0 - fx_val);
    } else if (fx_mode > 1.5 && fx_mode < 2.5) {
        // Mode 2: Fade to White (Kiégés / Burn)
        col.rgb = lerp(col.rgb, float3(1.0, 1.0, 1.0), fx_val);
    }

    // Alakzat-alapú maszk (Shape Wipe Mask)
    float mask = get_shape_wipe_mask(fx_mode, fx_val, uv);
    col.a = mask;
    col.rgb *= mask;
    return saturate(col);
}

float4 compute_cf_mix(VertData v, float fx_mode)
{
    float2 uv_a = apply_cf_fx(v.uv, fx_mode, cf_fx_mid, 0.0, cf_param_intensity, cf_param_rate);
    float2 uv_b = apply_cf_fx(v.uv, fx_mode, cf_fx_mid, 1.0, cf_param_intensity, cf_param_rate);

    float4 a = sample_deck_with_vol_fx(image_a, uv_a, vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
    float4 b = sample_deck_with_vol_fx(image_b, uv_b, vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);

    // Mode 3: RGB Split (Chromatic Aberration)
    if (fx_mode > 2.5 && fx_mode < 3.5 && cf_fx_mid > 0.005) {
        float split = cf_fx_mid * cf_p0 * cf_param_intensity;
        float ang = cf_p1;
        float2 dir = float2(cos(ang), sin(ang)) * split;
        float2 g_off = float2(0.0, cf_p2 * cf_fx_mid * cf_param_intensity);
        float4 a_r = sample_deck_with_vol_fx(image_a, uv_a + dir, vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
        float4 a_g = sample_deck_with_vol_fx(image_a, uv_a + g_off, vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
        float4 a_b = sample_deck_with_vol_fx(image_a, uv_a - dir, vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
        a.r = a_r.r;
        if (abs(cf_p2) > 0.001) a.g = a_g.g;
        a.b = a_b.b;
        if (cf_p4 > 1.01) a.rgb = lerp(dot(a.rgb, float3(0.3, 0.59, 0.11)).xxx, a.rgb, cf_p4);

        float4 b_r = sample_deck_with_vol_fx(image_b, uv_b - dir, vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);
        float4 b_g = sample_deck_with_vol_fx(image_b, uv_b - g_off, vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);
        float4 b_b = sample_deck_with_vol_fx(image_b, uv_b + dir, vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);
        b.r = b_r.r;
        if (abs(cf_p2) > 0.001) b.g = b_g.g;
        b.b = b_b.b;
        if (cf_p4 > 1.01) b.rgb = lerp(dot(b.rgb, float3(0.3, 0.59, 0.11)).xxx, b.rgb, cf_p4);
    }
    // Mode 4: Radial Zoom Blur (Suhanó nagyítás)
    else if (fx_mode > 3.5 && fx_mode < 4.5 && cf_fx_mid > 0.005) {
        float2 center = float2(0.5, 0.5);
        float2 to_c_a = center - uv_a;
        float2 to_c_b = center - uv_b;
        float z_step = cf_fx_mid * cf_p0 * cf_param_intensity;
        int max_s = clamp((int)cf_p1, 2, 8);
        float4 sum_a = a;
        float4 sum_b = b;
        float w_total = 1.0;
        for (int i = 1; i <= 8; ++i) {
            if (i <= max_s) {
                float w = max(1.0 - (float)i * 0.12 * cf_p2, 0.05);
                sum_a += sample_deck_with_vol_fx(image_a, uv_a + to_c_a * ((float)i * z_step), vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a) * w;
                sum_b += sample_deck_with_vol_fx(image_b, uv_b + to_c_b * ((float)i * z_step), vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b) * w;
                w_total += w;
            }
        }
        a = sum_a / w_total;
        b = sum_b / w_total;
        if (cf_p3 > 0.01) {
            a.rgb += a.rgb * cf_p3 * cf_fx_mid;
            b.rgb += b.rgb * cf_p3 * cf_fx_mid;
        }
    }

    float4 res;

    // Mode 6: Luma Dissolve (Fényerő-alapú áttűnés)
    if (fx_mode > 5.5 && fx_mode < 6.5) {
        float luma_a = dot(a.rgb, float3(0.2126, 0.7152, 0.0722));
        if (cf_p5 > 0.5) luma_a = 1.0 - luma_a;
        float width = max(cf_p0 * cf_param_rate, 0.005);
        float thresh = lerp(-width, 1.0 + width, crossfade);
        float blend = smoothstep(thresh - width, thresh + width, luma_a);
        float glow = saturate(1.0 - abs((luma_a - thresh) / width)) * cf_fx_mid * cf_p1 * cf_param_intensity;
        float3 glow_col = float3(cf_p2, cf_p3, cf_p4);
        res = lerp(b, a, blend) + float4(glow_col, 0.0) * (glow * smoothstep(0.02, 0.98, blend));
    } else {
        // Standard Transition Modes
        if (transition_mode > 2.5) {
            // Hard Cut
            res = crossfade < 0.5 ? a : b;
        } else if (transition_mode > 0.5 && transition_mode < 1.5) {
            // Horizontal Wipe
            res = v.uv.x < (1.0 - crossfade) ? a : b;
        } else if (transition_mode > 1.5 && transition_mode < 2.5) {
            // Additive / Screen Energy Blend (Kontraszt-megtartó ragyogó fúzió, nincs tejszerű köd)
            float3 screen_col = 1.0 - (1.0 - a.rgb * (1.0 - crossfade)) * (1.0 - b.rgb * crossfade);
            float3 add_col = a.rgb * (1.0 - crossfade) + b.rgb * crossfade;
            res.rgb = lerp(add_col, screen_col, 0.65);
            res.a = 1.0;
        } else {
            // Equal Power & Smooth S-Curve Crossfade (Konstans Energiájú, nem sötétedő átkeverés)
            float cf = crossfade;
            if (abs(cf_curve - 1.0) > 0.02) cf = pow(saturate(cf), max(cf_curve, 0.05));
            if (cf_smooth > 0.01) cf = lerp(cf, smoothstep(0.0, 1.0, cf), cf_smooth);

            float pi_half = 1.57079632679;
            float w_a = cos(cf * pi_half);
            float w_b = sin(cf * pi_half);

            float3 eq_blend = a.rgb * w_a + b.rgb * w_b;
            float3 lin_blend = lerp(a.rgb, b.rgb, cf);
            res.rgb = lerp(eq_blend, lin_blend, 0.30); // 70% Equal Power védelem a sötétedés ellen
            res.a = 1.0;
        }
    }

    // Mode 10: CRT / VHS Scanline (Analóg TV & Csík - Fényerővédelemmel)
    if (fx_mode > 9.5 && fx_mode < 10.5 && cf_fx_mid > 0.01) {
        float density = cf_p0 * cf_param_rate;
        float line_int = cf_p1;
        float flash_int = cf_p3;
        float scan = sin(v.uv.y * density * 3.14159);
        // Csökkentsük a vonalak elsötétítő mélységét, hogy a háttér tiszta és látható maradjon:
        res.rgb *= (1.0 - cf_fx_mid * line_int * 0.40 * cf_param_intensity * (scan * 0.5 + 0.5));
        float2 p_scan = v.uv - float2(0.5, 0.5);
        if (cf_fx_mid > 0.75) {
            float col_factor = saturate((cf_fx_mid - 0.75) / 0.25);
            float beam = exp(-abs(p_scan.y) * lerp(30.0, 220.0, col_factor)) * col_factor * flash_int;
            res.rgb = lerp(res.rgb, float3(1.3, 1.3, 1.4), beam);
        }
        if (cf_p4 > 0.01) {
            float vig = 1.0 - dot(p_scan, p_scan) * cf_p4 * 0.5;
            res.rgb *= saturate(vig);
        }
    }
    // Mode 11: Pop-Art Halftone (Képregény Pontrács - Energiamegőrzéssel)
    else if (fx_mode > 10.5 && fx_mode < 11.5 && cf_fx_mid > 0.01) {
        float2 grid = frac(v.uv * float2(cf_p0 * cf_param_rate, cf_p1 * cf_param_rate)) - 0.5;
        float dist = length(grid);
        float luma = dot(res.rgb, float3(0.2126, 0.7152, 0.0722));
        float radius = sqrt(luma) * cf_p2;
        float dot_mask = smoothstep(radius + cf_p3, radius - cf_p3, dist);
        float q_steps = max(cf_p4, 1.0);
        float3 pop_color = floor(res.rgb * q_steps + 0.5) / q_steps;
        // Energiamegőrzés: a pontok között a kép 50%-a megmarad, a pontokon pedig ragyog:
        float3 halftone = lerp(res.rgb * 0.50, pop_color * cf_p5, dot_mask);
        res.rgb = lerp(res.rgb, halftone, saturate(cf_fx_mid * 0.85 * cf_param_intensity));
    }
    // Mode 12: Neon Kontúr (Sobel Edge Glow - Alapvideó megtartással)
    else if (fx_mode > 11.5 && fx_mode < 12.5 && cf_fx_mid > 0.01) {
        float thick = cf_p0;
        float2 d = float2(thick / 1920.0, thick / 1080.0);
        float4 a_u = sample_deck_with_vol_fx(image_a, uv_a + float2(0, -d.y), vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
        float4 a_d = sample_deck_with_vol_fx(image_a, uv_a + float2(0, d.y), vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
        float4 a_l = sample_deck_with_vol_fx(image_a, uv_a + float2(-d.x, 0), vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
        float4 a_r = sample_deck_with_vol_fx(image_a, uv_a + float2(d.x, 0), vol_fx_mode_a, vol_fx_val_a, gains_a, saturation_a);
        float edge_a = length(a_u.rgb + a_d.rgb + a_l.rgb + a_r.rgb - 4.0 * a.rgb);

        float4 b_u = sample_deck_with_vol_fx(image_b, uv_b + float2(0, -d.y), vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);
        float4 b_d = sample_deck_with_vol_fx(image_b, uv_b + float2(0, d.y), vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);
        float4 b_l = sample_deck_with_vol_fx(image_b, uv_b + float2(-d.x, 0), vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);
        float4 b_r = sample_deck_with_vol_fx(image_b, uv_b + float2(d.x, 0), vol_fx_mode_b, vol_fx_val_b, gains_b, saturation_b);
        float edge_b = length(b_u.rgb + b_d.rgb + b_l.rgb + b_r.rgb - 4.0 * b.rgb);

        float edge_val_a = smoothstep(cf_p3, cf_p3 + 0.1, edge_a);
        float edge_val_b = smoothstep(cf_p3, cf_p3 + 0.1, edge_b);

        float3 neon_a = float3(0.0, 0.9, 1.0) * edge_val_a * cf_p1 * cf_p2 * cf_param_intensity;
        float3 neon_b = float3(1.0, 0.1, 0.8) * edge_val_b * cf_p1 * cf_p2 * cf_param_intensity;
        float3 wireframe = neon_a * (1.0 - crossfade) + neon_b * crossfade;
        // Az alapvideó arányát nem nyomjuk le, legalább 0.70-en tartjuk a jó láthatóságért:
        float base_weight = max(cf_p4, 0.70);
        float3 base_comp = res.rgb * base_weight;
        res.rgb = lerp(res.rgb, base_comp + wireframe, saturate(cf_fx_mid * 0.90 * cf_param_intensity));
    }

    // Globális Blend Boost (Keverési Fényerő Kiemelés: 0.5x .. 2.5x)
    float boost = max(cf_blend_boost, 0.1);
    res.rgb *= boost;

    res.a = 1.0;
    return saturate(res);
}

float4 PS(VertData v) : TARGET
{
    if (cf_trans_t >= 0.999 || abs(cf_fx_mode - cf_fx_mode_prev) < 0.1) {
        return compute_cf_mix(v, cf_fx_mode);
    }
    float blend_w = smoothstep(0.0, 1.0, cf_trans_t);
    float4 col_prev = compute_cf_mix(v, cf_fx_mode_prev);
    float4 col_curr = compute_cf_mix(v, cf_fx_mode);
    return lerp(col_prev, col_curr, blend_w);
}

technique Draw {
    pass {
        vertex_shader = VSDefault(v);
        pixel_shader = PS(v);
    }
}
)";

static const char *getMixerEffectCode()
{
    static std::string s_code;
    if (s_code.empty()) {
        s_code = MIXER_EFFECT_PART1;
        s_code += MIXER_EFFECT_PART2;
    }
    return s_code.c_str();
}

static void ensure_resources(MixerSource *context)
{
    if (!context->effect) {
        char *errors = nullptr;
        context->effect = gs_effect_create(getMixerEffectCode(), "mixer_effect", &errors);
        if (!context->effect) {
            blog(LOG_ERROR, "[obs-vj-mixer] Failed to compile MIXER_EFFECT: %s", errors ? errors : "unknown error");
            bfree(errors);
        }
    }

    const uint32_t cur_vj_ver = CustomShaderManager::instance().version();
    if (context->fluid_effect && context->loaded_vj_effect_version != cur_vj_ver) {
        gs_effect_destroy(context->fluid_effect);
        context->fluid_effect = nullptr;
    }
    if (!context->fluid_effect) {
        char *err = nullptr;
        context->fluid_effect = gs_effect_create(FLUID_FLOW_EFFECT, "fluid_flow_effect", &err);
        if (!context->fluid_effect) {
            blog(LOG_ERROR, "[obs-vj-mixer] Failed to compile FLUID_FLOW_EFFECT: %s", err ? err : "unknown error");
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

    if (!context->deck_a)
        context->deck_a = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

    if (!context->deck_b)
        context->deck_b = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
}

static bool render_to_texture(MixerSource *context,
                              gs_texrender_t *target,
                              obs_source_t *source,
                              const DeckPlayer *deck,
                              uint32_t width,
                              uint32_t height)
{
    if (!target || width == 0 || height == 0)
        return false;

    gs_texrender_reset(target);
    if (!gs_texrender_begin(target, width, height))
        return false;

    vec4 clear = {0.0f, 0.0f, 0.0f, 1.0f};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);

    const uint32_t src_w = source ? obs_source_get_width(source) : 0;
    const uint32_t src_h = source ? obs_source_get_height(source) : 0;

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

    if (source && src_w > 0 && src_h > 0 && (!deck || deck->hasVideo())) {
        gs_matrix_push();
        DeckPair *dp = vdj_decks();
        const int scale_mode = dp ? dp->videoScaleMode() : 1; // 0 = Fit, 1 = Fill, 2 = Stretch

        if (scale_mode == 0) {
            // FIT (Aránytartó teljes kép, fekete sávokkal ha eltér a képarány)
            const float s = std::min(static_cast<float>(width) / static_cast<float>(src_w),
                                     static_cast<float>(height) / static_cast<float>(src_h));
            const float draw_w = static_cast<float>(src_w) * s;
            const float draw_h = static_cast<float>(src_h) * s;
            const float off_x = (static_cast<float>(width) - draw_w) * 0.5f;
            const float off_y = (static_cast<float>(height) - draw_h) * 0.5f;
            gs_matrix_translate3f(off_x, off_y, 0.0f);
            gs_matrix_scale3f(s, s, 1.0f);
        } else if (scale_mode == 1) {
            // FILL (Aránytartó teljes vászonkitöltés fekete sávok nélkül)
            const float s = std::max(static_cast<float>(width) / static_cast<float>(src_w),
                                     static_cast<float>(height) / static_cast<float>(src_h));
            const float draw_w = static_cast<float>(src_w) * s;
            const float draw_h = static_cast<float>(src_h) * s;
            const float off_x = (static_cast<float>(width) - draw_w) * 0.5f;
            const float off_y = (static_cast<float>(height) - draw_h) * 0.5f;
            gs_matrix_translate3f(off_x, off_y, 0.0f);
            gs_matrix_scale3f(s, s, 1.0f);
        } else {
            // STRETCH (Nyújtás)
            const float sx = static_cast<float>(width) / static_cast<float>(src_w);
            const float sy = static_cast<float>(height) / static_cast<float>(src_h);
            gs_matrix_scale3f(sx, sy, 1.0f);
        }
        obs_source_video_render(source);
        gs_matrix_pop();
    } else if (deck && deck->hasAudioOnlyTrack() && deck->isPlaying() && !deck->isAtEnd()) {
        // Procedurális Audio-Reaktív VJ Shader Suite (audió fájlokhoz vagy üres deckhez)
        if (context && context->fluid_effect && context->dummy_tex) {
            const float time_sec = static_cast<float>(os_gettime_ns() / 1000000ULL) * 0.001f;
            const float bpm_val = static_cast<float>(deck->bpm());
            const float amp_val = deck->currentAmplitude();
            const float bass_val = deck->currentBassEnergy();
            const float mid_val = deck->currentMidEnergy();
            const float high_val = deck->currentHighEnergy();
            const bool is_b = (deck->side() == DeckPlayer::DeckSide::B);

            VjDeckConfig v_cfg = deck->vjConfig();
            const float viz_mode = static_cast<float>(deck->vjShaderIdx());
            const float viz_mode_prev = static_cast<float>(deck->vjShaderIdxPrev());
            const float viz_trans_t = deck->vjShaderTransProgress();
            const float macro_tonus = deck->currentMacroTonus();

            const auto &defs = getVjShaderDefinitions();
            float p_vals[8] = {0.0f};
            const float tone_balance = (high_val + mid_val * 0.5f) / (bass_val + mid_val + high_val + 0.001f);

            DeckPair *d_pair = vdj_decks();
            const float lfo1 = d_pair ? d_pair->currentLfo1Value() : 0.0f;
            const float lfo2 = d_pair ? d_pair->currentLfo2Value() : 0.0f;

            if (v_cfg.shader_idx >= 0 && v_cfg.shader_idx < static_cast<int>(defs.size())) {
                const auto &p_defs = defs[v_cfg.shader_idx].params;
                for (size_t i = 0; i < p_defs.size() && i < 8; ++i) {
                    p_vals[i] = computeEffectiveParam(p_defs[i], v_cfg.params[i].value,
                                                      v_cfg.params[i].mod_depth,
                                                      v_cfg.params[i].mod_mode,
                                                      bass_val, mid_val, high_val,
                                                      macro_tonus, lfo1, lfo2);
                }
            }

            const float audio_time = time_sec + bass_val * 0.12f;
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
                gs_effect_set_float(p, amp_val);
            if ((p = gs_effect_get_param_by_name(context->fluid_effect, "bass_energy")))
                gs_effect_set_float(p, bass_val);
            if ((p = gs_effect_get_param_by_name(context->fluid_effect, "mid_energy")))
                gs_effect_set_float(p, mid_val);
            if ((p = gs_effect_get_param_by_name(context->fluid_effect, "high_energy")))
                gs_effect_set_float(p, high_val);
            if ((p = gs_effect_get_param_by_name(context->fluid_effect, "is_deck_b")))
                gs_effect_set_float(p, is_b ? 1.0f : 0.0f);

            float theme_r = 0.0f, theme_g = 0.0f, theme_b = 0.0f, theme_act = 0.0f;
            computeThemeColor(v_cfg, is_b, time_sec, bpm_val, bass_val, macro_tonus,
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
                gs_draw_sprite(context->dummy_tex, 0, width, height);
            }
        }
    }

    gs_blend_state_pop();

    gs_texrender_end(target);
    return true;
}

static void set_color_params(MixerSource *context,
                             const DeckPlayer &deck_a,
                             const DeckPlayer &deck_b,
                             float crossfade,
                             int transition_mode,
                             int cf_fx_mode)
{
    gs_eparam_t *param = nullptr;

    vec4 gains_a;
    vec4 gains_b;

    if (deck_a.isVisualEqEnabled()) {
        vec4_set(&gains_a, deck_a.red(), deck_a.green(), deck_a.blue(), 1.0f);
    } else {
        vec4_set(&gains_a, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (deck_b.isVisualEqEnabled()) {
        vec4_set(&gains_b, deck_b.red(), deck_b.green(), deck_b.blue(), 1.0f);
    } else {
        vec4_set(&gains_b, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    if ((param = gs_effect_get_param_by_name(context->effect, "gains_a")))
        gs_effect_set_vec4(param, &gains_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "gains_b")))
        gs_effect_set_vec4(param, &gains_b);

    const float sat_a = deck_a.isVisualEqEnabled() ? deck_a.saturation() : 1.0f;
    const float sat_b = deck_b.isVisualEqEnabled() ? deck_b.saturation() : 1.0f;

    if ((param = gs_effect_get_param_by_name(context->effect, "saturation_a")))
        gs_effect_set_float(param, sat_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "saturation_b")))
        gs_effect_set_float(param, sat_b);

    if ((param = gs_effect_get_param_by_name(context->effect, "crossfade")))
        gs_effect_set_float(param, crossfade);

    if ((param = gs_effect_get_param_by_name(context->effect, "transition_mode")))
        gs_effect_set_float(param, static_cast<float>(transition_mode));

    // Volume FX (Deck A & Deck B)
    const float vol_val_a = std::clamp(1.0f - deck_a.volume(), 0.0f, 1.0f);
    const float vol_val_b = std::clamp(1.0f - deck_b.volume(), 0.0f, 1.0f);

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_mode_a")))
        gs_effect_set_float(param, static_cast<float>(deck_a.volumeFxMode()));

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_val_a")))
        gs_effect_set_float(param, vol_val_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_mode_b")))
        gs_effect_set_float(param, static_cast<float>(deck_b.volumeFxMode()));

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_val_b")))
        gs_effect_set_float(param, vol_val_b);

    // Crossfader FX (Midpoint peak: 0 at edges, 1 at center)
    const float cf_mid = std::clamp(1.0f - std::abs(crossfade - 0.5f) * 2.0f, 0.0f, 1.0f);

    DeckPair *decks = vdj_decks();
    const float cf_fx_prev = static_cast<float>(decks ? decks->crossfadeFxModePrev() : cf_fx_mode);
    const float cf_trans_t = decks ? decks->crossfadeFxTransProgress() : 1.0f;

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_fx_mode")))
        gs_effect_set_float(param, static_cast<float>(cf_fx_mode));

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_fx_mode_prev")))
        gs_effect_set_float(param, cf_fx_prev);

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_trans_t")))
        gs_effect_set_float(param, cf_trans_t);

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_fx_mid")))
        gs_effect_set_float(param, cf_mid);
    const float raw_intensity = decks ? decks->crossfadeFxIntensity() : 1.0f;
    const int intensity_mod = decks ? decks->crossfadeFxIntensityMod() : 0;
    const float raw_rate = decks ? decks->crossfadeFxRate() : 1.0f;
    const int rate_mod = decks ? decks->crossfadeFxRateMod() : 0;

    const float bass_energy = (1.0f - crossfade) * deck_a.currentBassEnergy() + crossfade * deck_b.currentBassEnergy();
    const float mid_energy = (1.0f - crossfade) * deck_a.currentMidEnergy() + crossfade * deck_b.currentMidEnergy();
    const float high_energy = (1.0f - crossfade) * deck_a.currentHighEnergy() + crossfade * deck_b.currentHighEnergy();
    const float macro_tonus = (1.0f - crossfade) * deck_a.currentMacroTonus() + crossfade * deck_b.currentMacroTonus();
    const float lfo1 = decks ? decks->currentLfo1Value() : 0.0f;
    const float lfo2 = decks ? decks->currentLfo2Value() : 0.0f;

    const float eff_intensity = computeEffectiveParamSimple(raw_intensity, 0.0f, 2.0f, 0.0f, intensity_mod, bass_energy, mid_energy, high_energy, macro_tonus, lfo1, lfo2);
    const float eff_rate = computeEffectiveParamSimple(raw_rate, 0.1f, 3.0f, 0.0f, rate_mod, bass_energy, mid_energy, high_energy, macro_tonus, lfo1, lfo2);
    const float blend_boost = decks ? decks->crossfadeFxBlendBoost() : 1.0f;

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_param_intensity")))
        gs_effect_set_float(param, eff_intensity);

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_param_rate")))
        gs_effect_set_float(param, eff_rate);

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_blend_boost")))
        gs_effect_set_float(param, blend_boost);

    const float cf_curve = decks ? decks->crossfadeFxParamValue(0, 0) : 1.0f;
    const float cf_smooth = decks ? decks->crossfadeFxParamValue(0, 1) : 0.0f;

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_curve")))
        gs_effect_set_float(param, cf_curve);

    if ((param = gs_effect_get_param_by_name(context->effect, "cf_smooth")))
        gs_effect_set_float(param, cf_smooth);

    const int active_fx = std::clamp(cf_fx_mode, 0, 15);
    const auto &cf_defs = getCrossfadeFxDefinitions();
    const auto &p_defs = (active_fx < static_cast<int>(cf_defs.size())) ? cf_defs[active_fx].params : std::vector<CrossfadeFxParamDef>{};

    char p_str[8] = "cf_p0";
    for (int i = 0; i < 8; ++i) {
        p_str[4] = static_cast<char>('0' + i);
        float p_val = 1.0f;
        int p_mod = 0;
        float p_depth = 0.0f;
        if (decks) {
            p_val = decks->crossfadeFxParamValue(active_fx, i);
            p_mod = decks->crossfadeFxParamMod(active_fx, i);
            p_depth = decks->crossfadeFxParamModDepth(active_fx, i);
        }
        float eff_p = p_val;
        if (i < static_cast<int>(p_defs.size())) {
            eff_p = computeCrossfadeEffectiveParam(p_defs[i], p_val, p_depth, p_mod, bass_energy, mid_energy, high_energy, macro_tonus, lfo1, lfo2);
        } else {
            eff_p = computeEffectiveParamSimple(p_val, 0.0f, 10.0f, p_depth, p_mod, bass_energy, mid_energy, high_energy, macro_tonus, lfo1, lfo2);
        }
        if ((param = gs_effect_get_param_by_name(context->effect, p_str)))
            gs_effect_set_float(param, eff_p);
    }

    const float anim_time = static_cast<float>(os_gettime_ns() / 1000000ULL) * 0.001f;
    if ((param = gs_effect_get_param_by_name(context->effect, "anim_time")))
        gs_effect_set_float(param, anim_time);
}

static const char *mixer_get_name(void *)
{
    return "OBS VJ Master Mixer";
}

static void mixer_update(void *data, obs_data_t *settings);

static void *mixer_create(obs_data_t *settings, obs_source_t *source)
{
    auto *context = new MixerSource();
    context->source = source;

    // Minden hangsáv engedélyezése az OBS rögzítéshez és streameléshez (Enable all 6 audio tracks)
    obs_source_set_audio_mixers(source, 0x3F);

    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setMasterSource(source);
    }

    // Alkalmazzuk a mentett beállításokat (hangkártya kimenet, átmeneti mód) azonnal az indításkor
    if (settings) {
        mixer_update(context, settings);
    } else {
        if (obs_audio_monitoring_available()) {
            obs_set_audio_monitoring_device("Default", "default");
        }
    }

    // Kényszerített monitorozási típus beállítás (Destroy stale handle & create fresh monitor)
    obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
    obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);

    // =========================================================================
    // Mester RAM Audió Szál: Deck A + Deck B valós idejű keverése Crossfaderrel
    // (Master RAM Audio Thread: Mixing Deck A + Deck B in Real-Time with Crossfader)
    // =========================================================================
    context->audio_thread_running.store(true, std::memory_order_relaxed);
    context->audio_thread = std::thread([context]() {
#ifdef _WIN32
        timeBeginPeriod(1);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
        const uint32_t sample_rate = 48000;
        const size_t frames_per_tick = 480; // 10ms precíz időszelet 48kHz-en

        std::vector<float> buf_a_l(frames_per_tick);
        std::vector<float> buf_a_r(frames_per_tick);
        std::vector<float> buf_b_l(frames_per_tick);
        std::vector<float> buf_b_r(frames_per_tick);
        std::vector<float> master_l(frames_per_tick);
        std::vector<float> master_r(frames_per_tick);

        auto next_wake = std::chrono::steady_clock::now();

        while (context->audio_thread_running.load(std::memory_order_relaxed)) {
            next_wake += std::chrono::milliseconds(10);

            DeckPair *dp = vdj_decks();
            if (!dp || !context->source) {
                std::this_thread::sleep_until(next_wake);
                continue;
            }

            DeckPlayer &da = dp->deckA();
            DeckPlayer &db = dp->deckB();

            bool have_a = false;
            if (da.hasAudioBuffer() && da.isPlaying()) {
                da.readAudioFrames(buf_a_l.data(), buf_a_r.data(), frames_per_tick, sample_rate);
                da.processAudioDsp(buf_a_l.data(), frames_per_tick, 0, sample_rate);
                da.processAudioDsp(buf_a_r.data(), frames_per_tick, 1, sample_rate);
                have_a = true;
            } else {
                std::fill(buf_a_l.begin(), buf_a_l.end(), 0.0f);
                std::fill(buf_a_r.begin(), buf_a_r.end(), 0.0f);
            }

            bool have_b = false;
            if (db.hasAudioBuffer() && db.isPlaying()) {
                db.readAudioFrames(buf_b_l.data(), buf_b_r.data(), frames_per_tick, sample_rate);
                db.processAudioDsp(buf_b_l.data(), frames_per_tick, 0, sample_rate);
                db.processAudioDsp(buf_b_r.data(), frames_per_tick, 1, sample_rate);
                have_b = true;
            } else {
                std::fill(buf_b_l.begin(), buf_b_l.end(), 0.0f);
                std::fill(buf_b_r.begin(), buf_b_r.end(), 0.0f);
            }

            // Fejhallgató CUE & PGM precíz keverése a két csatorna mintáiból (szinkronizált sztereó)
            dp->mixAndQueueCue(buf_a_l.data(), buf_a_r.data(), buf_b_l.data(), buf_b_r.data(), frames_per_tick);

            // Crossfader, csatornahangerő és Gain súlyozás (Gain & Crossfader weighting)
            const float gain_a = da.gain() * da.volume() * da.mixGain();
            const float gain_b = db.gain() * db.volume() * db.mixGain();

            for (size_t i = 0; i < frames_per_tick; ++i) {
                master_l[i] = std::clamp(buf_a_l[i] * gain_a + buf_b_l[i] * gain_b, -1.2f, 1.2f);
                master_r[i] = std::clamp(buf_a_r[i] * gain_a + buf_b_r[i] * gain_b, -1.2f, 1.2f);
            }

            if (have_a || have_b) {
                const uint64_t cur_time = os_gettime_ns();
                if (context->next_audio_ts == 0) {
                    context->next_audio_ts = cur_time + 25000000ULL;
                } else if (cur_time > context->next_audio_ts + 80000000ULL ||
                           context->next_audio_ts > cur_time + 150000000ULL) {
                    context->next_audio_ts = cur_time + 25000000ULL;
                }

                struct obs_source_audio out_audio = {};
                out_audio.data[0] = reinterpret_cast<uint8_t *>(master_l.data());
                out_audio.data[1] = reinterpret_cast<uint8_t *>(master_r.data());
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

            const auto now = std::chrono::steady_clock::now();
            if (next_wake > now) {
                std::this_thread::sleep_until(next_wake);
            } else {
                next_wake = now;
            }
        }

#ifdef _WIN32
        timeEndPeriod(1);
#endif
    });

    return context;
}

static void mixer_destroy(void *data)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!context)
        return;

    // Audió szál biztonságos leállítása a grafika felszabadítása előtt
    context->audio_thread_running.store(false, std::memory_order_relaxed);
    if (context->audio_thread.joinable()) {
        context->audio_thread.join();
    }

    DeckPair *decks = vdj_decks();
    if (decks && decks->masterSource() == context->source) {
        decks->setMasterSource(nullptr);
    }

    obs_enter_graphics();

    if (context->deck_a)
        gs_texrender_destroy(context->deck_a);

    if (context->deck_b)
        gs_texrender_destroy(context->deck_b);

    if (context->effect)
        gs_effect_destroy(context->effect);

    if (context->fluid_effect)
        gs_effect_destroy(context->fluid_effect);

    if (context->dummy_tex)
        gs_texture_destroy(context->dummy_tex);

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

    static uint64_t s_last_mixer_draw_ns = 0;
    const uint64_t now_draw_ns = os_gettime_ns();
    if (s_last_mixer_draw_ns > 0) {
        float dt = static_cast<float>(now_draw_ns - s_last_mixer_draw_ns) * 1e-9f;
        if (dt > 0.1f) dt = 0.1f;
        if (dt > 0.0f) {
            deck_a.updateVjShaderTransition(dt);
            deck_b.updateVjShaderTransition(dt);
            decks->updateCrossfadeFxTransition(dt);
        }
    }
    s_last_mixer_draw_ns = now_draw_ns;

    deck_a.updateLoop();
    deck_b.updateLoop();

    // Közvetlenül a két Deck privát belső forrását rendereljük textúrára,
    // így az OBS VJ Master Mixer BÁRMELY jelenetben működik, Stúdió Mód nélkül, valós időben!
    // (Directly render Deck A and B decoder sources so Master Mixer works in ANY scene without Studio Mode)
    obs_source_t *src_a = deck_a.source();
    obs_source_t *src_b = deck_b.source();

    const bool rendered_a =
        render_to_texture(context, context->deck_a, src_a, &deck_a, width, height);
    const bool rendered_b =
        render_to_texture(context, context->deck_b, src_b, &deck_b, width, height);

    gs_texture_t *texture_a =
        rendered_a ? gs_texrender_get_texture(context->deck_a) : nullptr;
    gs_texture_t *texture_b =
        rendered_b ? gs_texrender_get_texture(context->deck_b) : nullptr;

    const int output_mode = decks->obsOutputMode();
    const DeckPlayer *color_a = &deck_a;
    const DeckPlayer *color_b = &deck_b;
    float xfade = static_cast<float>(decks->crossfader());
    if (output_mode == 1) {
        xfade = 0.0f;
    } else if (output_mode == 2) {
        xfade = 1.0f;
    }

    if (!texture_a && !texture_b)
        return;

    if (!texture_a) {
        texture_a = texture_b;
        color_a = &deck_b;
    } else if (!texture_b) {
        texture_b = texture_a;
        color_b = &deck_a;
    }

    gs_eparam_t *param = nullptr;

    if ((param = gs_effect_get_param_by_name(context->effect, "image_a")))
        gs_effect_set_texture(param, texture_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "image_b")))
        gs_effect_set_texture(param, texture_b);

    set_color_params(context, *color_a, *color_b, xfade,
                     decks->transitionMode(),
                     decks->crossfadeFxMode());

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

    while (gs_effect_loop(context->effect, "Draw")) {
        gs_draw_sprite(texture_a, 0, width, height);
    }

    gs_blend_state_pop();
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

static bool enum_audio_devices_cb(void *param, const char *name, const char *id)
{
    obs_property_t *prop = static_cast<obs_property_t *>(param);
    obs_property_list_add_string(prop, name, id);
    return true;
}

static obs_properties_t *mixer_properties(void *)
{
    obs_properties_t *props = obs_properties_create();

    obs_property_t *dev_list = obs_properties_add_list(
        props, "master_device", "Master Audio Output Device (Soundcard)",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(dev_list, "Default Device (Behringer 1-2 Out / Windows Default)", "default");
    obs_enum_audio_monitoring_devices(enum_audio_devices_cb, dev_list);

    obs_property_t *transition = obs_properties_add_list(
        props, "transition", "FX Transition",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(transition, "Linear Fade", 0);
    obs_property_list_add_int(transition, "Horizontal Wipe", 1);
    obs_property_list_add_int(transition, "Additive Blend", 2);
    obs_property_list_add_int(transition, "Hard Cut", 3);

    obs_property_t *cf_fx = obs_properties_add_list(
        props, "crossfade_fx", "Crossfade Center FX",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(cf_fx, "Off (Alapértelmezett)", 0);
    obs_property_list_add_int(cf_fx, "Spirális Örvény (Swirl / Vortex)", 1);
    obs_property_list_add_int(cf_fx, "Halszem Optika (Fisheye Lens)", 2);
    obs_property_list_add_int(cf_fx, "RGB Split (Chromatic Aberration)", 3);
    obs_property_list_add_int(cf_fx, "Radial Zoom Blur (Suhanó nagyítás)", 4);
    obs_property_list_add_int(cf_fx, "Pixelesedés (Pixelate / Mosaic)", 5);
    obs_property_list_add_int(cf_fx, "Luma Dissolve (Fényerő áttűnés)", 6);
    obs_property_list_add_int(cf_fx, "Folyadék Hullámzás (Liquid Ripple Warp)", 7);
    obs_property_list_add_int(cf_fx, "Kaleidoszkóp (Mirror Fold)", 8);
    obs_property_list_add_int(cf_fx, "Glitch Displacement (Zajelcsúszás)", 9);
    obs_property_list_add_int(cf_fx, "CRT / VHS Scanline (Analóg TV & Csík)", 10);
    obs_property_list_add_int(cf_fx, "Pop-Art Halftone (Képregény Pontrács)", 11);
    obs_property_list_add_int(cf_fx, "Neon Kontúr (Sobel Edge Glow)", 12);

    obs_property_t *viz = obs_properties_add_list(
        props, "visualizer_mode", "Audio-Only VJ Shader Visualizer",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(viz, "1. Fluid Flow 3D (Organikus Folyadék)", 0);
    obs_property_list_add_int(viz, "2. Truchet Spin 3D (3D Raymarching Labirintus)", 1);
    obs_property_list_add_int(viz, "3. Hypno Worm (24-Lépéses OpArt Alagút)", 2);
    obs_property_list_add_int(viz, "4. Shape Twist (18-Lépéses Elektro Hullám)", 3);
    obs_property_list_add_int(viz, "5. String Art (Geometrikus Szálművészet)", 4);
    obs_property_list_add_int(viz, "6. Dodecahedron 3D (3D Raymarching Kristály)", 5);
    obs_property_list_add_int(viz, "7. Kali Circuits (Fraktál Áramkörök)", 6);
    obs_property_list_add_int(viz, "8. Diamond Vision (Tórusz Sugárkövetés)", 7);
    obs_property_list_add_int(viz, "9. Pupila (Organikus Szem & Írisz)", 8);
    obs_property_list_add_int(viz, "10. MandalaScope (72-Lépéses Fraktál Mandala)", 9);
    obs_property_list_add_int(viz, "11. Searching Light (Kiber Fényalagút)", 10);
    obs_property_list_add_int(viz, "12. CubeField 3D (Végtelen Kockamező Raymarching)", 11);
    obs_property_list_add_int(viz, "13. Electro Bands (Pulzáló Neon Szalagok)", 12);
    obs_property_list_add_int(viz, "14. GlobularCluster (3D Gömbhalmaz Torzulás)", 13);
    obs_property_list_add_int(viz, "15. HexTruchWarpFlow2 (Hexagonális Truchet Áramlás)", 14);

    obs_properties_add_bool(props, "visualizer_pulse", "Zenei Lüktetés (Kick & Bass Pulse Dynamics)");
    obs_properties_add_bool(props, "visualizer_tone", "Zenei Tónuskövetés (Track Spectral Tone Tracking)");

    return props;
}

static void mixer_update(void *data, obs_data_t *settings)
{
    auto *context = static_cast<MixerSource *>(data);
    if (!settings) return;

    const char *dev = obs_data_get_string(settings, "master_device");
    if (obs_audio_monitoring_available()) {
        if (dev && *dev && strcmp(dev, "default") != 0) {
            obs_set_audio_monitoring_device(dev, dev);
        } else {
            obs_set_audio_monitoring_device("Default", "default");
        }
    }

    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setTransitionMode(static_cast<int>(obs_data_get_int(settings, "transition")));
        decks->setCrossfadeFxMode(static_cast<int>(obs_data_get_int(settings, "crossfade_fx")));
        decks->setVisualizerMode(static_cast<int>(obs_data_get_int(settings, "visualizer_mode")));
        decks->setVisualizerPulse(obs_data_get_bool(settings, "visualizer_pulse"));
        decks->setVisualizerTone(obs_data_get_bool(settings, "visualizer_tone"));
    }

    if (context && context->source) {
        obs_source_set_monitoring_type(context->source, OBS_MONITORING_TYPE_NONE);
        obs_source_set_monitoring_type(context->source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
    }
}

static void mixer_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "master_device", "default");
    obs_data_set_default_int(settings, "transition", 0);
    obs_data_set_default_int(settings, "crossfade_fx", 0);
    obs_data_set_default_int(settings, "visualizer_mode", 0);
    obs_data_set_default_bool(settings, "visualizer_pulse", true);
    obs_data_set_default_bool(settings, "visualizer_tone", true);
}

static const char *cue_get_name(void *)
{
    return "OBS VJ Headphone CUE";
}

// =============================================================================
// Videó tick ciklus (Renderelés előtti állapotfrissítés - Video Render Tick)
// =============================================================================
static void mixer_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(seconds);
    auto *context = static_cast<MixerSource *>(data);
    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->deckA().updateLoop();
        decks->deckB().updateLoop();

        // Amikor a felhasználó elindítja a lejátszást az OBS indítása után, azonnal felébresztjük a WASAPI kimenetet
        if (context && context->need_monitor_wake_.load(std::memory_order_relaxed)) {
            if ((decks->deckA().isPlaying() || decks->deckB().isPlaying()) && context->source) {
                context->need_monitor_wake_.store(false, std::memory_order_relaxed);
                obs_source_set_monitoring_type(context->source, OBS_MONITORING_TYPE_NONE);
                obs_source_set_monitoring_type(context->source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
            }
        }
    }
}

static obs_properties_t *cue_properties(void *)
{
    obs_properties_t *props = obs_properties_create();
    obs_property_t *dev_list = obs_properties_add_list(
        props, "monitoring_device", "CUE Headphone Soundcard / Device",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(dev_list, "Default Audio Device", "default");
    obs_enum_audio_monitoring_devices(enum_audio_devices_cb, dev_list);
    return props;
}

static void cue_update(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    if (!settings) return;
    const char *dev = obs_data_get_string(settings, "monitoring_device");
    DeckPair *decks = vdj_decks();
    if (decks && dev && *dev) {
        decks->setCueDeviceId(dev);
    }
}

static void *cue_create(obs_data_t *settings, obs_source_t *source)
{
    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setCueSource(source);
    }
    obs_source_set_audio_mixers(source, 0); // Mute on main stream / recording tracks
    obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE); // Send direct to headphones via WASAPI, not OBS master monitor!
    cue_update(source, settings);
    return source;
}

static void cue_destroy(void *)
{
    DeckPair *decks = vdj_decks();
    if (decks) {
        decks->setCueSource(nullptr);
    }
}

void vdj_register_sources(void)
{
    obs_source_info cue_info = {};
    cue_info.id = "vdj_headphone_cue";
    cue_info.type = OBS_SOURCE_TYPE_INPUT;
    cue_info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
    cue_info.get_name = cue_get_name;
    cue_info.create = cue_create;
    cue_info.destroy = cue_destroy;
    cue_info.get_properties = cue_properties;
    cue_info.update = cue_update;
    obs_register_source_s(&cue_info, sizeof(cue_info));

    obs_source_info master = {};
    master.id = "vdj_master_mixer";
    master.type = OBS_SOURCE_TYPE_INPUT;
    master.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;
    master.get_name = mixer_get_name;
    master.create = mixer_create;
    master.destroy = mixer_destroy;
    master.update = mixer_update;
    master.video_tick = mixer_video_tick;
    master.video_render = mixer_video_render;
    // Az aktív gyermek deck források átadása az OBS-nek az automatikus dekódolási tickhez
    master.enum_active_sources = mixer_enum_active_sources;
    master.enum_all_sources = nullptr;
    master.get_width = mixer_get_width;
    master.get_height = mixer_get_height;
    master.get_properties = mixer_properties;
    master.get_defaults = mixer_defaults;
    obs_register_source_s(&master, sizeof(master));
}

} // extern "C"
