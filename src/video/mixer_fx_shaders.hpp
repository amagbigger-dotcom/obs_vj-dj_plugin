#pragma once

#include <string>

// =============================================================================
// TRAKTOR MIXER FX 9-TAGÚ KÉPTORZÍTÓ VIZUÁLIS SHADER CSOMAG (HLSL)
// Mind a 9 audio effekt közvetlen képi torzítási algoritmusa Direct3D 11 / HLSL
// =============================================================================

static const char *MIXER_FX_HLSL = R"(

// Type 0: FILTER (Bipoláris Gaussian/Box Blur + Vignette vs. Sharpen + Contrast)
// fx_val < 0: Low-pass (mély, víz alatti elmosás és perem-sötétedés)
// fx_val > 0: High-pass (kristálytiszta élesítés és kontraszt-kiemelés)
float4 fx_filter(texture2d img, float2 uv, float v, float amt)
{
    if (v < 0.0) {
        // LPF: Multi-tap Blur + Vignette + Dimming
        float r = amt * 0.016;
        float4 sum = img.Sample(def_sampler, uv) * 0.24;
        sum += img.Sample(def_sampler, uv + float2( r,  0.0)) * 0.14;
        sum += img.Sample(def_sampler, uv + float2(-r,  0.0)) * 0.14;
        sum += img.Sample(def_sampler, uv + float2( 0.0,  r)) * 0.14;
        sum += img.Sample(def_sampler, uv + float2( 0.0, -r)) * 0.14;
        sum += img.Sample(def_sampler, uv + float2( r * 0.7,  r * 0.7)) * 0.05;
        sum += img.Sample(def_sampler, uv + float2(-r * 0.7,  r * 0.7)) * 0.05;
        sum += img.Sample(def_sampler, uv + float2( r * 0.7, -r * 0.7)) * 0.05;
        sum += img.Sample(def_sampler, uv + float2(-r * 0.7, -r * 0.7)) * 0.05;

        float2 p = (uv - 0.5) * float2(1.7777778, 1.0);
        float d = length(p);
        float vig = 1.0 - smoothstep(0.35, 1.15, d) * amt * 0.85;
        sum.rgb *= saturate(vig);
        return saturate(sum);
    } else {
        // HPF: Laplacian Sharpening + Edge Sizzle Contrast
        float sh = amt * 0.003;
        float4 c  = img.Sample(def_sampler, uv);
        float4 up = img.Sample(def_sampler, uv + float2(0.0, -sh));
        float4 dn = img.Sample(def_sampler, uv + float2(0.0,  sh));
        float4 lf = img.Sample(def_sampler, uv + float2(-sh, 0.0));
        float4 rt = img.Sample(def_sampler, uv + float2( sh, 0.0));
        float4 sharp = c * (1.0 + 4.0 * amt) - (up + dn + lf + rt) * amt;
        sharp.rgb = (sharp.rgb - 0.5) * (1.0 + amt * 0.55) + 0.5;
        return saturate(sharp);
    }
}

// Type 1: DELAY (Dub Echo / Motion Trails / Multi-tap Feedback Ghosting)
float4 fx_delay(texture2d img, float2 uv, float v, float amt, float bpm, float anim_time)
{
    float beat_sec = 60.0 / max(bpm, 60.0);
    float echo_time = beat_sec * 0.75; // 3/4 beat
    float phase = frac(anim_time / echo_time);
    
    // Többrétegű irányított mozgási szellemképek (Directional Spatial Ghosting)
    float2 dir = float2(cos(anim_time * 1.5), sin(anim_time * 1.5)) * (amt * 0.035);
    float4 col = img.Sample(def_sampler, uv);
    float4 e1 = img.Sample(def_sampler, uv - dir * (0.5 + phase * 0.5));
    float4 e2 = img.Sample(def_sampler, uv - dir * (1.2 + phase * 0.8));
    float4 e3 = img.Sample(def_sampler, uv - dir * (2.0 + phase * 1.0));

    float decay = amt * 0.75;
    col.rgb = col.rgb + e1.rgb * (decay * 0.50) + e2.rgb * (decay * 0.30) + e3.rgb * (decay * 0.18);
    return saturate(col);
}

// Type 2: REVERB (Bloom / Ethereal Glow / Light Leak)
float4 fx_reverb(texture2d img, float2 uv, float v, float amt)
{
    float4 base_col = img.Sample(def_sampler, uv);
    float thresh = max(0.20, 0.65 - amt * 0.40);
    float rad = amt * 0.030;
    float inv_range = 1.0 / max(1.0 - thresh, 0.01);

    float4 bloom = float4(0.0, 0.0, 0.0, 0.0);
    float4 s1 = img.Sample(def_sampler, uv + float2( rad,  0.0));
    float4 s2 = img.Sample(def_sampler, uv + float2(-rad,  0.0));
    float4 s3 = img.Sample(def_sampler, uv + float2( 0.0,  rad));
    float4 s4 = img.Sample(def_sampler, uv + float2( 0.0, -rad));
    float4 s5 = img.Sample(def_sampler, uv + float2( rad * 0.7,  rad * 0.7));
    float4 s6 = img.Sample(def_sampler, uv + float2(-rad * 0.7,  rad * 0.7));
    float4 s7 = img.Sample(def_sampler, uv + float2( rad * 0.7, -rad * 0.7));
    float4 s8 = img.Sample(def_sampler, uv + float2(-rad * 0.7, -rad * 0.7));

    bloom += s1 * saturate((dot(s1.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s2 * saturate((dot(s2.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s3 * saturate((dot(s3.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s4 * saturate((dot(s4.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s5 * saturate((dot(s5.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s6 * saturate((dot(s6.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s7 * saturate((dot(s7.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s8 * saturate((dot(s8.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom *= 0.125;

    // Meleg/arany fényudvar fúzió
    float3 aura = bloom.rgb * float3(1.15, 1.05, 0.95);
    base_col.rgb += aura * (amt * 1.85);
    return saturate(base_col);
}

// Type 3: FLANGER (Jet Sweep Wave Warp & Chromatic Aberration RGB Shift)
float4 fx_flanger(texture2d img, float2 uv, float v, float amt, float anim_time)
{
    float wave_phase = uv.y * 18.0 + anim_time * 5.0;
    float2 wave = float2(sin(wave_phase), cos(wave_phase * 0.75)) * (amt * 0.022);
    float2 center_uv = uv + wave;

    float split = amt * 0.015;
    float r = img.Sample(def_sampler, center_uv + float2( split, 0.0)).r;
    float g = img.Sample(def_sampler, center_uv).g;
    float b = img.Sample(def_sampler, center_uv - float2( split, 0.0)).b;
    float a = img.Sample(def_sampler, center_uv).a;

    // Sugárhajtómű-szerű fémes interferencia (Jet sweep sheen)
    float sweep = sin(anim_time * 4.0 + uv.x * 3.14159) * 0.5 + 0.5;
    float3 res = float3(r, g, b) + float3(0.12, 0.18, 0.25) * (sweep * amt * 0.35);
    return saturate(float4(res, a));
}

// Type 4: BEATMASHER 2 (Rhythmic Beat-Synced Glitch / Slices / Invert Strobe)
float4 fx_beatmasher(texture2d img, float2 uv, float v, float amt, float bpm, float anim_time)
{
    float beat_sec = 60.0 / max(bpm, 60.0);
    // 7-fokozatú ritmikai felosztás mint az audióban (1/4 .. 1/32)
    float div = 0.25;
    if (amt >= 0.88) div = 0.03125;
    else if (amt >= 0.76) div = 0.04167;
    else if (amt >= 0.62) div = 0.0625;
    else if (amt >= 0.48) div = 0.08333;
    else if (amt >= 0.32) div = 0.125;
    else if (amt >= 0.18) div = 0.1875;
    else div = 0.25;

    float slice_sec = beat_sec * div;
    float step_idx = floor(anim_time / slice_sec);
    float step_phase = frac(anim_time / slice_sec);

    // Vízszintes sávos szeletelés (Slices)
    float num_bands = lerp(8.0, 32.0, amt);
    float band = floor(uv.y * num_bands);
    float band_rnd = frac(sin(band * 127.1 + step_idx * 311.7) * 43758.5453);

    float2 slice_uv = uv;
    if (band_rnd > 0.40) {
        float shift = (band_rnd - 0.5) * amt * 0.15;
        slice_uv.x = frac(slice_uv.x + shift);
    }

    // Stutter Freeze UV ugrálás
    if (amt > 0.35 && step_phase < 0.40) {
        float block_rnd = frac(sin(step_idx * 987.65) * 43758.5453);
        if (block_rnd > 0.50) {
            slice_uv.y = frac(slice_uv.y + (block_rnd - 0.5) * amt * 0.12);
        }
    }

    float4 col = img.Sample(def_sampler, slice_uv);

    // Invert Strobe az ütések elején magas intenzitásnál
    if (amt > 0.65 && step_phase < 0.15 && frac(step_idx * 0.25) < 0.26) {
        col.rgb = 1.0 - col.rgb;
    }

    return saturate(col);
}

// Type 5: TURNTABLE FX (Bipolar: Vinyl Brake vs. Spinback Twirl)
// v < 0: Brake (lassulás, kimerevedés, vertikális nyúlás)
// v > 0: Spinback (örvénylő Twirl, forgási elmosódás)
float4 fx_turntable(texture2d img, float2 uv, float v, float amt, float anim_time)
{
    float2 center = float2(0.5, 0.5);
    float2 p = uv - center;

    if (v < 0.0) {
        // BRAKE: Anamorphic Vertical Stretch + Drag Scanline
        float stretch = 1.0 + amt * 0.45;
        float2 brake_uv = float2(p.x / stretch, p.y * stretch) + center;
        float4 col = img.Sample(def_sampler, brake_uv);
        // Enyhe analóg lefékeződés fényerő-csökkenés
        col.rgb *= (1.0 - amt * 0.35);
        return saturate(col);
    } else {
        // SPINBACK: Polar Twirl / Swirl + Radial Spin Blur
        float r = length(p * float2(1.7777778, 1.0));
        float angle = atan2(p.y, p.x);
        float twirl = (1.0 - saturate(r * 1.35)) * amt * 4.5;
        angle += twirl - anim_time * amt * 8.0;

        float2 twirl_uv = float2(cos(angle), sin(angle)) * length(p) + center;
        float4 col = img.Sample(def_sampler, twirl_uv);

        // Radial spin blur taps
        float4 b1 = img.Sample(def_sampler, float2(cos(angle + amt * 0.08), sin(angle + amt * 0.08)) * length(p) + center);
        float4 b2 = img.Sample(def_sampler, float2(cos(angle - amt * 0.08), sin(angle - amt * 0.08)) * length(p) + center);
        col = (col * 0.5) + (b1 * 0.25) + (b2 * 0.25);
        return saturate(col);
    }
}

// Type 6: RAMP DELAY (Infinite Inward Tunnel Zoom & Swirl Vortex)
float4 fx_ramp_delay(texture2d img, float2 uv, float v, float amt, float anim_time)
{
    float2 p = (uv - 0.5) * float2(1.7777778, 1.0);
    float r = max(length(p), 0.001);
    float angle = atan2(p.y, p.x);

    // Befelé gyorsuló logaritmikus zuhanás (Infinite Tunnel)
    float speed = 2.0 + amt * 6.5;
    float log_r = frac(log(r) * 0.65 - anim_time * speed);
    float swirl = angle + log_r * 3.14159 * (1.0 + amt * 2.5);

    float2 tunnel_uv = float2(cos(swirl), sin(swirl)) * log_r * 0.5 + 0.5;
    float4 col_tunnel = img.Sample(def_sampler, tunnel_uv);
    float4 col_orig   = img.Sample(def_sampler, uv);

    return saturate(lerp(col_orig, col_tunnel, saturate(amt * 1.15)));
}

// Type 7: MULHOLLAND DRIVE (Analog Overdrive / Solarize Foldback / Film Grain)
float4 fx_mulholland(texture2d img, float2 uv, float v, float amt, float anim_time)
{
    float4 col = img.Sample(def_sampler, uv);

    // 1. Analóg túlhajtás (Overdrive gain)
    col.rgb *= (1.0 + amt * 5.0);

    // 2. Szinuszos szolárizáció (Solarize Foldback): a túlvezérelt csúcsfények negatívba fordulnak
    if (amt > 0.15) {
        float3 fold = abs(sin(col.rgb * 1.5707963));
        col.rgb = lerp(col.rgb, fold, saturate((amt - 0.15) / 0.85));
    }

    // 3. CRT Filmzaj (Dynamic Grain)
    float grain = frac(sin(dot(uv * (anim_time + 1.23), float2(12.9898, 78.233))) * 43758.5453);
    col.rgb += (grain - 0.5) * (amt * 0.24);

    return saturate(col);
}

// Type 8: TRANSPOSE STRETCH (Bipolar: Barrel Bulge vs. Pincushion Pinch)
// v < 0: Pitch Down -> Bulge / Fisheye
// v > 0: Pitch Up   -> Pinch / Pincushion
float4 fx_transpose(texture2d img, float2 uv, float v, float amt)
{
    float2 p = (uv - 0.5) * float2(1.7777778, 1.0);
    float r = length(p);

    float2 dist_p;
    if (v < 0.0) {
        // BULGE (Halszemoptika / Domborodás)
        float factor = 1.0 + amt * 1.25 * r * r;
        dist_p = p / factor;
    } else {
        // PINCH (Párnatorzítás / Behorpadás)
        float factor = 1.0 + amt * 1.35 * (1.0 - saturate(r * 1.2));
        dist_p = p * factor;
    }

    dist_p.x /= 1.7777778;
    float2 dist_uv = dist_p + 0.5;

    // Szélmaszk a határokon túli mintavételezés feketítésére
    float mask = step(0.0, dist_uv.x) * step(dist_uv.x, 1.0) * step(0.0, dist_uv.y) * step(dist_uv.y, 1.0);
    float4 col = img.Sample(def_sampler, saturate(dist_uv)) * mask;
    return col;
}

// Fő koordinátor függvény a 9 effekt és Visual EQ együttes lekezelésére
float4 apply_deck_mixer_fx(texture2d img, float2 uv, float fx_type, float fx_val, float bpm, float anim_time, float4 gains, float sat, bool vis_eq)
{
    float4 col;
    float amt = abs(fx_val);
    if (amt <= 0.005) {
        // ZÉRÓ GPU TERHELÉS: Középállásban közvetlen minta, minden torzítás átugrása!
        col = img.Sample(def_sampler, uv);
    } else {
        // Agresszív kezdeti görbe (|v|^0.62) mint az audióban
        float curve_amt = pow(saturate(amt), 0.62);

        int t = (int)(fx_type + 0.5);
        if (t == 0)      col = fx_filter(img, uv, fx_val, curve_amt);
        else if (t == 1) col = fx_delay(img, uv, fx_val, curve_amt, bpm, anim_time);
        else if (t == 2) col = fx_reverb(img, uv, fx_val, curve_amt);
        else if (t == 3) col = fx_flanger(img, uv, fx_val, curve_amt, anim_time);
        else if (t == 4) col = fx_beatmasher(img, uv, fx_val, curve_amt, bpm, anim_time);
        else if (t == 5) col = fx_turntable(img, uv, fx_val, curve_amt, anim_time);
        else if (t == 6) col = fx_ramp_delay(img, uv, fx_val, curve_amt, anim_time);
        else if (t == 7) col = fx_mulholland(img, uv, fx_val, curve_amt, anim_time);
        else if (t == 8) col = fx_transpose(img, uv, fx_val, curve_amt);
        else             col = img.Sample(def_sampler, uv);
    }

    // Színkorrekció és Visual EQ (Basszus vezérelt szaturáció + 3-sávos gain)
    if (vis_eq) {
        col.rgb *= gains.rgb;
        float luma = dot(col.rgb, float3(0.2126, 0.7152, 0.0722));
        col.rgb = lerp(luma.xxx, col.rgb, sat);
    }

    return saturate(col);
}
)";

static const char *DECK_COLOR_EFFECT_PREFIX = R"(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform float4 gains;
uniform float saturation;
uniform float alpha;
uniform float wipe_cutoff;
uniform float is_deck_b;
uniform float mixer_fx_type;
uniform float mixer_fx_val;
uniform float bpm;
uniform float anim_time;
uniform float visual_eq_enabled;

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
)";

static const char *DECK_COLOR_EFFECT_SUFFIX = R"(
float4 PSColorize(VertData v) : TARGET
{
    float4 col = apply_deck_mixer_fx(image, v.uv, mixer_fx_type, mixer_fx_val, bpm, anim_time, gains, saturation, visual_eq_enabled > 0.5);
    
    float final_a = alpha;
    if (wipe_cutoff >= 0.0) {
        if (is_deck_b > 0.5) {
            final_a *= smoothstep(wipe_cutoff - 0.005, wipe_cutoff + 0.005, v.uv.x);
        } else {
            final_a *= smoothstep(wipe_cutoff + 0.005, wipe_cutoff - 0.005, v.uv.x);
        }
    }
    col.a = saturate(final_a * col.a);
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

inline const char *getDeckColorEffectCode()
{
    static std::string s_code;
    if (s_code.empty()) {
        s_code = DECK_COLOR_EFFECT_PREFIX;
        s_code += MIXER_FX_HLSL;
        s_code += DECK_COLOR_EFFECT_SUFFIX;
    }
    return s_code.c_str();
}
