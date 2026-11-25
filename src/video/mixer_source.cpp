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
#include "mixer_fx_shaders.hpp"
#include "audio/asio_backend.hpp"

extern "C" {

// =============================================================================
// Mester Mixer forrás struktúra (Master Mixer Source Structure & Audio Thread)
// =============================================================================
struct MixerSource {
    obs_source_t *source = nullptr;
    int channel = 0;

    gs_effect_t *effect = nullptr;
    gs_effect_t *fluid_effect = nullptr;
    gs_effect_t *deck_color_effect = nullptr;
    bool effect_failed = false;
    bool deck_color_effect_failed = false;
    uint32_t loaded_vj_effect_version = 0;
    gs_texture_t *dummy_tex = nullptr;
    gs_texrender_t *deck_a = nullptr;
    gs_texrender_t *deck_b = nullptr;
    gs_texrender_t *raw_deck_a = nullptr;
    gs_texrender_t *raw_deck_b = nullptr;

    uint64_t next_audio_ts = 0;
    std::thread audio_thread;
    std::atomic<bool> audio_thread_running{false};
    std::atomic<bool> need_monitor_wake_{true};
};

static const char *MIXER_EFFECT_PART1 = R"(
uniform float4x4 ViewProj;
uniform texture2d image_a;
uniform texture2d image_b;
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
uniform float cf_prev_p0;
uniform float cf_prev_p1;
uniform float cf_prev_p2;
uniform float cf_prev_p3;
uniform float cf_prev_p4;
uniform float cf_prev_p5;
uniform float cf_prev_p6;
uniform float cf_prev_p7;

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

float2 apply_cf_fx(float2 uv, float mode, float mid, float is_b, float intensity, float rate,
                   float p0, float p1, float p2, float p3, float p4, float p5, float p6, float p7)
{
    if (mode < 0.5 || mid <= 0.001)
        return uv;

    float2 p = uv - float2(0.5, 0.5);
    float r = length(p);

    if (mode > 0.5 && mode < 1.5) {
        // Mode 1: Spirális Örvény (Swirl / Vortex)
        float twist = p0;
        float radius = max(p1, 0.1);
        float dir_mult = (p2 >= 0.0) ? 1.0 : -1.0;
        float zoom = max(p3, 0.1);
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
        float distortion = p0;
        float zoom = max(p1, 0.1);
        float vignette = p2;
        float2 center = float2(0.5, 0.5);
        float2 p_fish = (uv - center) / zoom;
        float r_fish2 = dot(p_fish, p_fish);
        float dist_val = 1.0 + mid * distortion * r_fish2 * intensity * rate;
        return (p_fish * dist_val) * zoom + center;
    }
    else if (mode > 4.5 && mode < 5.5) {
        // Mode 5: Pixelesedés (Pixelate / Mosaic)
        float max_g = p0;
        float min_g = p1;
        float asp = max(p2, 0.1);
        float border = p3;
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
        float freq = p0;
        float spd = p1;
        float amp_param = p2;
        float asp_x = max(p3, 0.1);
        float decay = max(p4, 0.01);
        float surge = p5;
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
        float sides = max(floor(p0 + 0.5), 2.0);
        float zoom = max(p1, 0.1);
        float pan_x = p2;
        float pan_y = p3;
        float fold_angle = p4;
        float softness = p5;
        float contrast = max(p6, 0.1);
        float blend_p = p7;

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
        float bands = max(p0, 2.0);
        float spd = p1;
        float shift_amt = p2;
        float softness = max(p3, 0.05);
        float asp = max(p5, 0.1);
        float blend_p = p7;

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
        float curve = p2;
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

float4 sample_deck_with_vol_fx(texture2d img, float2 uv, float fx_mode, float fx_val)
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
        sum += img.Sample(def_sampler, sample_uv + float2(off.x, 0.0));
        sum += img.Sample(def_sampler, sample_uv - float2(off.x, 0.0));
        sum += img.Sample(def_sampler, sample_uv + float2(0.0, off.y));
        sum += img.Sample(def_sampler, sample_uv - float2(0.0, off.y));
        sum += img.Sample(def_sampler, sample_uv + float2(off.x * 0.7, off.y * 0.7));
        sum += img.Sample(def_sampler, sample_uv - float2(off.x * 0.7, off.y * 0.7));
        sum += img.Sample(def_sampler, sample_uv + float2(-off.x * 0.7, off.y * 0.7));
        sum += img.Sample(def_sampler, sample_uv + float2(off.x * 0.7, -off.y * 0.7));
        col = sum / 9.0;
    } else {
        col = img.Sample(def_sampler, sample_uv);
    }

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

float4 sample_deck_a(float2 uv)
{
    return sample_deck_with_vol_fx(image_a, uv, vol_fx_mode_a, vol_fx_val_a);
}

float4 sample_deck_b(float2 uv)
{
    return sample_deck_with_vol_fx(image_b, uv, vol_fx_mode_b, vol_fx_val_b);
}

float4 compute_cf_mix(VertData v, float fx_mode,
                      float p0, float p1, float p2, float p3, float p4, float p5, float p6, float p7)
{
    float2 uv_a = apply_cf_fx(v.uv, fx_mode, cf_fx_mid, 0.0, cf_param_intensity, cf_param_rate,
                              p0, p1, p2, p3, p4, p5, p6, p7);
    float2 uv_b = apply_cf_fx(v.uv, fx_mode, cf_fx_mid, 1.0, cf_param_intensity, cf_param_rate,
                              p0, p1, p2, p3, p4, p5, p6, p7);

    float4 a = sample_deck_a(uv_a);
    float4 b = sample_deck_b(uv_b);

    // Mode 3: RGB Split (Chromatic Aberration)
    if (fx_mode > 2.5 && fx_mode < 3.5 && cf_fx_mid > 0.005) {
        float split = cf_fx_mid * p0 * cf_param_intensity;
        float ang = p1;
        float2 dir = float2(cos(ang), sin(ang)) * split;
        float2 g_off = float2(0.0, p2 * cf_fx_mid * cf_param_intensity);
        float4 a_r = sample_deck_a(uv_a + dir);
        float4 a_g = sample_deck_a(uv_a + g_off);
        float4 a_b = sample_deck_a(uv_a - dir);
        a.r = a_r.r;
        if (abs(p2) > 0.001) a.g = a_g.g;
        a.b = a_b.b;
        if (p4 > 1.01) a.rgb = lerp(dot(a.rgb, float3(0.3, 0.59, 0.11)).xxx, a.rgb, p4);

        float4 b_r = sample_deck_b(uv_b - dir);
        float4 b_g = sample_deck_b(uv_b - g_off);
        float4 b_b = sample_deck_b(uv_b + dir);
        b.r = b_r.r;
        if (abs(p2) > 0.001) b.g = b_g.g;
        b.b = b_b.b;
        if (p4 > 1.01) b.rgb = lerp(dot(b.rgb, float3(0.3, 0.59, 0.11)).xxx, b.rgb, p4);
    }
    // Mode 4: Radial Zoom Blur (Suhanó nagyítás)
    else if (fx_mode > 3.5 && fx_mode < 4.5 && cf_fx_mid > 0.005) {
        float2 center = float2(0.5, 0.5);
        float2 to_c_a = center - uv_a;
        float2 to_c_b = center - uv_b;
        float z_step = cf_fx_mid * p0 * cf_param_intensity;
        int max_s = clamp((int)p1, 2, 8);
        float4 sum_a = a;
        float4 sum_b = b;
        float w_total = 1.0;
        for (int i = 1; i <= 8; ++i) {
            if (i <= max_s) {
                float w = max(1.0 - (float)i * 0.12 * p2, 0.05);
                sum_a += sample_deck_a(uv_a + to_c_a * ((float)i * z_step)) * w;
                sum_b += sample_deck_b(uv_b + to_c_b * ((float)i * z_step)) * w;
                w_total += w;
            }
        }
        a = sum_a / w_total;
        b = sum_b / w_total;
        if (p3 > 0.01) {
            a.rgb += a.rgb * p3 * cf_fx_mid;
            b.rgb += b.rgb * p3 * cf_fx_mid;
        }
    }

    // Alpha-Over Rétegezés (Volume Fader Alakzat Áttűnés támogatása fekete hézagok nélkül)
    float3 comp_a = a.rgb + b.rgb * saturate(1.0 - a.a);
    float3 comp_b = b.rgb + a.rgb * saturate(1.0 - b.a);
    float4 eff_a = float4(comp_a, 1.0);
    float4 eff_b = float4(comp_b, 1.0);

    float4 res;

    // Mode 6: Luma Dissolve (Fényerő-alapú áttűnés)
    if (fx_mode > 5.5 && fx_mode < 6.5) {
        float luma_a = dot(a.rgb, float3(0.2126, 0.7152, 0.0722));
        if (p5 > 0.5) luma_a = 1.0 - luma_a;
        float width = max(p0 * cf_param_rate, 0.005);
        float thresh = lerp(-width, 1.0 + width, crossfade);
        float blend = smoothstep(thresh - width, thresh + width, luma_a);
        float glow = saturate(1.0 - abs((luma_a - thresh) / width)) * cf_fx_mid * p1 * cf_param_intensity;
        float3 glow_col = float3(p2, p3, p4);
        res = lerp(eff_b, eff_a, blend) + float4(glow_col, 0.0) * (glow * smoothstep(0.02, 0.98, blend));
    } else {
        // Standard Transition Modes
        if (transition_mode > 2.5) {
            // Hard Cut
            res = crossfade < 0.5 ? eff_a : eff_b;
        } else if (transition_mode > 0.5 && transition_mode < 1.5) {
            // Horizontal Wipe
            res = v.uv.x < (1.0 - crossfade) ? eff_a : eff_b;
        } else if (transition_mode > 1.5 && transition_mode < 2.5) {
            // Additive / Screen Energy Blend (Kontraszt-megtartó ragyogó fúzió, nincs tejszerű köd)
            float3 screen_col = 1.0 - (1.0 - eff_a.rgb * (1.0 - crossfade)) * (1.0 - eff_b.rgb * crossfade);
            float3 add_col = eff_a.rgb * (1.0 - crossfade) + eff_b.rgb * crossfade;
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

            float3 eq_blend = eff_a.rgb * w_a + eff_b.rgb * w_b;
            // Ha valamelyik deck alakzattal maszkolva van, a kivágásban a közös tartalom ne égjen ki:
            float sum_w = w_a + w_b;
            float norm_factor = lerp(1.0, 1.0 / max(sum_w, 1.0), saturate((1.0 - a.a) + (1.0 - b.a)));
            eq_blend *= norm_factor;

            float3 lin_blend = lerp(eff_a.rgb, eff_b.rgb, cf);
            res.rgb = lerp(eq_blend, lin_blend, 0.30); // 70% Equal Power védelem a sötétedés ellen
            res.a = 1.0;
        }
    }

    // Mode 10: CRT / VHS Scanline (Analóg TV & Csík - Fényerővédelemmel)
    if (fx_mode > 9.5 && fx_mode < 10.5 && cf_fx_mid > 0.01) {
        float density = p0 * cf_param_rate;
        float line_int = p1;
        float flash_int = p3;
        float scan = sin(v.uv.y * density * 3.14159);
        // Csökkentsük a vonalak elsötétítő mélységét, hogy a háttér tiszta és látható maradjon:
        res.rgb *= (1.0 - cf_fx_mid * line_int * 0.40 * cf_param_intensity * (scan * 0.5 + 0.5));
        float2 p_scan = v.uv - float2(0.5, 0.5);
        if (cf_fx_mid > 0.75) {
            float col_factor = saturate((cf_fx_mid - 0.75) / 0.25);
            float beam = exp(-abs(p_scan.y) * lerp(30.0, 220.0, col_factor)) * col_factor * flash_int;
            res.rgb = lerp(res.rgb, float3(1.3, 1.3, 1.4), beam);
        }
        if (p4 > 0.01) {
            float vig = 1.0 - dot(p_scan, p_scan) * p4 * 0.5;
            res.rgb *= saturate(vig);
        }
    }
    // Mode 11: Pop-Art Halftone (Képregény Pontrács - Energiamegőrzéssel)
    else if (fx_mode > 10.5 && fx_mode < 11.5 && cf_fx_mid > 0.01) {
        float2 grid = frac(v.uv * float2(p0 * cf_param_rate, p1 * cf_param_rate)) - 0.5;
        float dist = length(grid);
        float luma = dot(res.rgb, float3(0.2126, 0.7152, 0.0722));
        float radius = sqrt(luma) * p2;
        float dot_mask = smoothstep(radius + p3, radius - p3, dist);
        float q_steps = max(p4, 1.0);
        float3 pop_color = floor(res.rgb * q_steps + 0.5) / q_steps;
        // Energiamegőrzés: a pontok között a kép 50%-a megmarad, a pontokon pedig ragyog:
        float3 halftone = lerp(res.rgb * 0.50, pop_color * p5, dot_mask);
        res.rgb = lerp(res.rgb, halftone, saturate(cf_fx_mid * 0.85 * cf_param_intensity));
    }
    // Mode 12: Neon Kontúr (Sobel Edge Glow - Alapvideó megtartással)
    else if (fx_mode > 11.5 && fx_mode < 12.5 && cf_fx_mid > 0.01) {
        float thick = p0;
        float2 d = float2(thick / 1920.0, thick / 1080.0);
        float4 a_u = sample_deck_a(uv_a + float2(0, -d.y));
        float4 a_d = sample_deck_a(uv_a + float2(0, d.y));
        float4 a_l = sample_deck_a(uv_a + float2(-d.x, 0));
        float4 a_r = sample_deck_a(uv_a + float2(d.x, 0));
        float edge_a = length(a_u.rgb + a_d.rgb + a_l.rgb + a_r.rgb - 4.0 * a.rgb);

        float4 b_u = sample_deck_b(uv_b + float2(0, -d.y));
        float4 b_d = sample_deck_b(uv_b + float2(0, d.y));
        float4 b_l = sample_deck_b(uv_b + float2(-d.x, 0));
        float4 b_r = sample_deck_b(uv_b + float2(d.x, 0));
        float edge_b = length(b_u.rgb + b_d.rgb + b_l.rgb + b_r.rgb - 4.0 * b.rgb);

        float edge_val_a = smoothstep(p3, p3 + 0.1, edge_a);
        float edge_val_b = smoothstep(p3, p3 + 0.1, edge_b);

        float3 neon_a = float3(0.0, 0.9, 1.0) * edge_val_a * p1 * p2 * cf_param_intensity;
        float3 neon_b = float3(1.0, 0.1, 0.8) * edge_val_b * p1 * p2 * cf_param_intensity;
        float3 wireframe = neon_a * (1.0 - crossfade) + neon_b * crossfade;
        // Az alapvideó arányát nem nyomjuk le, legalább 0.70-en tartjuk a jó láthatóságért:
        float base_weight = max(p4, 0.70);
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
        return compute_cf_mix(v, cf_fx_mode, cf_p0, cf_p1, cf_p2, cf_p3, cf_p4, cf_p5, cf_p6, cf_p7);
    }
    float blend_w = smoothstep(0.0, 1.0, cf_trans_t);
    float4 col_prev = compute_cf_mix(v, cf_fx_mode_prev, cf_prev_p0, cf_prev_p1, cf_prev_p2, cf_prev_p3, cf_prev_p4, cf_prev_p5, cf_prev_p6, cf_prev_p7);
    float4 col_curr = compute_cf_mix(v, cf_fx_mode, cf_p0, cf_p1, cf_p2, cf_p3, cf_p4, cf_p5, cf_p6, cf_p7);
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
    if (!context->effect && !context->effect_failed) {
        char *errors = nullptr;
        context->effect = gs_effect_create(getMixerEffectCode(), "mixer_effect", &errors);
        if (!context->effect) {
            blog(LOG_ERROR, "[obs-vj-mixer] Failed to compile MIXER_EFFECT: %s", errors ? errors : "unknown error");
            bfree(errors);
            context->effect_failed = true;
        }
    }

    if (!context->deck_color_effect && !context->deck_color_effect_failed) {
        char *errors = nullptr;
        context->deck_color_effect = gs_effect_create(getDeckColorEffectCode(), "deck_color_effect", &errors);
        if (!context->deck_color_effect) {
            blog(LOG_ERROR, "[obs-vj-mixer] Failed to compile deck_color_effect: %s", errors ? errors : "unknown error");
            bfree(errors);
            context->deck_color_effect_failed = true;
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

    if (!context->raw_deck_a)
        context->raw_deck_a = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

    if (!context->raw_deck_b)
        context->raw_deck_b = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

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

static void apply_deck_post_fx(MixerSource *context,
                               gs_texrender_t *dst_render,
                               gs_texture_t *src_texture,
                               const DeckPlayer &deck,
                               bool is_deck_b,
                               uint32_t width,
                               uint32_t height)
{
    if (!context || !dst_render || !src_texture || !context->deck_color_effect)
        return;

    gs_texrender_reset(dst_render);
    if (!gs_texrender_begin(dst_render, width, height))
        return;

    vec4 clear = {0.0f, 0.0f, 0.0f, 0.0f};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);

    gs_eparam_t *param = nullptr;
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "image")))
        gs_effect_set_texture(param, src_texture);

    const bool vis_eq = deck.isVisualEqEnabled();
    vec4 gains;
    if (vis_eq) {
        vec4_set(&gains, deck.red(), deck.green(), deck.blue(), 1.0f);
    } else {
        vec4_set(&gains, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "gains")))
        gs_effect_set_vec4(param, &gains);

    const float sat = vis_eq ? deck.saturation() : 1.0f;
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "saturation")))
        gs_effect_set_float(param, sat);

    const float fx_val = vis_eq ? deck.filterVal() : 0.0f;
    const float fx_type = static_cast<float>(deck.mixerFxType());
    const float bpm_val = static_cast<float>(deck.bpm());
    const float anim_time = static_cast<float>(os_gettime_ns() / 1000000ULL) * 0.001f;

    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "mixer_fx_type")))
        gs_effect_set_float(param, fx_type);
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "mixer_fx_val")))
        gs_effect_set_float(param, fx_val);
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "bpm")))
        gs_effect_set_float(param, bpm_val);
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "anim_time")))
        gs_effect_set_float(param, anim_time);
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "visual_eq_enabled")))
        gs_effect_set_float(param, vis_eq ? 1.0f : 0.0f);

    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "alpha")))
        gs_effect_set_float(param, 1.0f);
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "wipe_cutoff")))
        gs_effect_set_float(param, -1.0f);
    if ((param = gs_effect_get_param_by_name(context->deck_color_effect, "is_deck_b")))
        gs_effect_set_float(param, is_deck_b ? 1.0f : 0.0f);

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

    while (gs_effect_loop(context->deck_color_effect, "Draw")) {
        gs_draw_sprite(src_texture, 0, width, height);
    }

    gs_blend_state_pop();
    gs_texrender_end(dst_render);
}

static void set_color_params(MixerSource *context,
                             float crossfade,
                             int transition_mode,
                             int cf_fx_mode)
{
    gs_eparam_t *param = nullptr;

    if ((param = gs_effect_get_param_by_name(context->effect, "crossfade")))
        gs_effect_set_float(param, crossfade);

    if ((param = gs_effect_get_param_by_name(context->effect, "transition_mode")))
        gs_effect_set_float(param, static_cast<float>(transition_mode));

    // Volume FX (Deck A & Deck B)
    DeckPair *decks = vdj_decks();
    const float vol_val_a = decks ? std::clamp(1.0f - decks->deckA().volume(), 0.0f, 1.0f) : 0.0f;
    const float vol_val_b = decks ? std::clamp(1.0f - decks->deckB().volume(), 0.0f, 1.0f) : 0.0f;

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_mode_a")))
        gs_effect_set_float(param, static_cast<float>(decks ? decks->deckA().volumeFxMode() : 0));

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_val_a")))
        gs_effect_set_float(param, vol_val_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_mode_b")))
        gs_effect_set_float(param, static_cast<float>(decks ? decks->deckB().volumeFxMode() : 0));

    if ((param = gs_effect_get_param_by_name(context->effect, "vol_fx_val_b")))
        gs_effect_set_float(param, vol_val_b);

    // Crossfader FX (Midpoint peak: 0 at edges, 1 at center)
    const float cf_mid = std::clamp(1.0f - std::abs(crossfade - 0.5f) * 2.0f, 0.0f, 1.0f);

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

    const float bass_energy = decks ? ((1.0f - crossfade) * decks->deckA().currentBassEnergy() + crossfade * decks->deckB().currentBassEnergy()) : 0.0f;
    const float mid_energy = decks ? ((1.0f - crossfade) * decks->deckA().currentMidEnergy() + crossfade * decks->deckB().currentMidEnergy()) : 0.0f;
    const float high_energy = decks ? ((1.0f - crossfade) * decks->deckA().currentHighEnergy() + crossfade * decks->deckB().currentHighEnergy()) : 0.0f;
    const float macro_tonus = decks ? ((1.0f - crossfade) * decks->deckA().currentMacroTonus() + crossfade * decks->deckB().currentMacroTonus()) : 0.0f;
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
    const int prev_fx = std::clamp(static_cast<int>(cf_fx_prev + 0.5f), 0, 15);
    const auto &cf_defs = getCrossfadeFxDefinitions();
    const auto &p_defs = (active_fx < static_cast<int>(cf_defs.size())) ? cf_defs[active_fx].params : std::vector<CrossfadeFxParamDef>{};
    const auto &p_prev_defs = (prev_fx < static_cast<int>(cf_defs.size())) ? cf_defs[prev_fx].params : std::vector<CrossfadeFxParamDef>{};

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

    char prev_p_str[16] = "cf_prev_p0";
    for (int i = 0; i < 8; ++i) {
        prev_p_str[9] = static_cast<char>('0' + i);
        float p_val = 1.0f;
        int p_mod = 0;
        float p_depth = 0.0f;
        if (decks) {
            p_val = decks->crossfadeFxParamValue(prev_fx, i);
            p_mod = decks->crossfadeFxParamMod(prev_fx, i);
            p_depth = decks->crossfadeFxParamModDepth(prev_fx, i);
        }
        float eff_p = p_val;
        if (i < static_cast<int>(p_prev_defs.size())) {
            eff_p = computeCrossfadeEffectiveParam(p_prev_defs[i], p_val, p_depth, p_mod, bass_energy, mid_energy, high_energy, macro_tonus, lfo1, lfo2);
        } else {
            eff_p = computeEffectiveParamSimple(p_val, 0.0f, 10.0f, p_depth, p_mod, bass_energy, mid_energy, high_energy, macro_tonus, lfo1, lfo2);
        }
        if ((param = gs_effect_get_param_by_name(context->effect, prev_p_str)))
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

            auto &asio = AsioBackend::instance();
            if (asio.isRunning()) {
                if (have_a || have_b) {
                    asio.writeMasterFrames(master_l.data(), master_r.data(), frames_per_tick);
                    if (asio.isExternalMixer()) {
                        asio.writeDeckFrames(0, buf_a_l.data(), buf_a_r.data(), frames_per_tick);
                        asio.writeDeckFrames(1, buf_b_l.data(), buf_b_r.data(), frames_per_tick);
                    }
                }
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

    if (context->raw_deck_a)
        gs_texrender_destroy(context->raw_deck_a);

    if (context->raw_deck_b)
        gs_texrender_destroy(context->raw_deck_b);

    if (context->deck_a)
        gs_texrender_destroy(context->deck_a);

    if (context->deck_b)
        gs_texrender_destroy(context->deck_b);

    if (context->effect)
        gs_effect_destroy(context->effect);

    if (context->deck_color_effect)
        gs_effect_destroy(context->deck_color_effect);

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

    const bool rendered_raw_a =
        render_to_texture(context, context->raw_deck_a, src_a, &deck_a, width, height);
    const bool rendered_raw_b =
        render_to_texture(context, context->raw_deck_b, src_b, &deck_b, width, height);

    if (rendered_raw_a) {
        apply_deck_post_fx(context, context->deck_a, gs_texrender_get_texture(context->raw_deck_a),
                           deck_a, false, width, height);
    }
    if (rendered_raw_b) {
        apply_deck_post_fx(context, context->deck_b, gs_texrender_get_texture(context->raw_deck_b),
                           deck_b, true, width, height);
    }

    gs_texture_t *texture_a =
        rendered_raw_a ? gs_texrender_get_texture(context->deck_a) : nullptr;
    gs_texture_t *texture_b =
        rendered_raw_b ? gs_texrender_get_texture(context->deck_b) : nullptr;

    const int output_mode = decks->obsOutputMode();
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
    } else if (!texture_b) {
        texture_b = texture_a;
    }

    gs_eparam_t *param = nullptr;

    if ((param = gs_effect_get_param_by_name(context->effect, "image_a")))
        gs_effect_set_texture(param, texture_a);

    if ((param = gs_effect_get_param_by_name(context->effect, "image_b")))
        gs_effect_set_texture(param, texture_b);

    set_color_params(context, xfade,
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

        if (obs_data_has_user_value(settings, "mixer_fx_slot_0")) {
            decks->setSlotFxType(0, static_cast<int>(obs_data_get_int(settings, "mixer_fx_slot_0")));
            decks->setSlotFxType(1, static_cast<int>(obs_data_get_int(settings, "mixer_fx_slot_1")));
            decks->setSlotFxType(2, static_cast<int>(obs_data_get_int(settings, "mixer_fx_slot_2")));
            decks->setSlotFxType(3, static_cast<int>(obs_data_get_int(settings, "mixer_fx_slot_3")));
            decks->selectSlot(static_cast<int>(obs_data_get_int(settings, "mixer_fx_active_slot")));
        }
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
    obs_data_set_default_int(settings, "mixer_fx_slot_0", 0);
    obs_data_set_default_int(settings, "mixer_fx_slot_1", 1);
    obs_data_set_default_int(settings, "mixer_fx_slot_2", 2);
    obs_data_set_default_int(settings, "mixer_fx_slot_3", 3);
    obs_data_set_default_int(settings, "mixer_fx_active_slot", 0);
}

static void mixer_save(void *, obs_data_t *settings)
{
    DeckPair *decks = vdj_decks();
    if (decks && settings) {
        obs_data_set_int(settings, "mixer_fx_slot_0", decks->slotFxType(0));
        obs_data_set_int(settings, "mixer_fx_slot_1", decks->slotFxType(1));
        obs_data_set_int(settings, "mixer_fx_slot_2", decks->slotFxType(2));
        obs_data_set_int(settings, "mixer_fx_slot_3", decks->slotFxType(3));
        obs_data_set_int(settings, "mixer_fx_active_slot", decks->activeSlot());
    }
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
    master.save = mixer_save;
    obs_register_source_s(&master, sizeof(master));
}

} // extern "C"
