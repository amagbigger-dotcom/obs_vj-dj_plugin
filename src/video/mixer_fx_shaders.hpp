#pragma once

#include <string>

// =============================================================================
// TRAKTOR MIXER FX 9-TAGÚ KÉPTORZÍTÓ VIZUÁLIS SHADER CSOMAG (HLSL)
// Mind a 9 audio effekt közvetlen képi torzítási algoritmusa Direct3D 11 / HLSL
// libobs HLSL: A textúra mintavételezés a globális 'image' uniformból történik.
// MSVC C2026 védelem: Két részre bontva a stringkonstans mérethatár miatt.
// =============================================================================

static const char *MIXER_FX_HLSL_PART1 = R"(

// Type 0: FILTER (Bipoláris Gaussian/Box Blur + Vignette vs. Sharpen + Contrast)
// fx_val < 0: Low-pass (mély, víz alatti elmosás és perem-sötétedés)
// fx_val > 0: High-pass (kristálytiszta élesítés és kontraszt-kiemelés)
float4 fx_filter(float2 uv, float v, float amt, float bass)
{
    if (v < 0.0) {
        // LPF: Multi-tap Blur + Vignette + Underwater Dimming
        float r = amt * 0.022;
        float4 sum = image.Sample(def_sampler, uv) * 0.24;
        sum += image.Sample(def_sampler, uv + float2( r,  0.0)) * 0.14;
        sum += image.Sample(def_sampler, uv + float2(-r,  0.0)) * 0.14;
        sum += image.Sample(def_sampler, uv + float2( 0.0,  r)) * 0.14;
        sum += image.Sample(def_sampler, uv + float2( 0.0, -r)) * 0.14;
        sum += image.Sample(def_sampler, uv + float2( r * 0.707,  r * 0.707)) * 0.05;
        sum += image.Sample(def_sampler, uv + float2(-r * 0.707,  r * 0.707)) * 0.05;
        sum += image.Sample(def_sampler, uv + float2( r * 0.707, -r * 0.707)) * 0.05;
        sum += image.Sample(def_sampler, uv + float2(-r * 0.707, -r * 0.707)) * 0.05;

        float2 p = (uv - 0.5) * float2(1.7777778, 1.0);
        float vig = 1.0 - smoothstep(0.30, 1.15, length(p)) * (amt * 0.85);
        sum.rgb *= saturate(vig * (1.0 + bass * 0.20 * amt));
        return saturate(sum);
    } else {
        // HPF: Laplacian Sharpening + Edge Sizzle Contrast
        float sh = amt * 0.0035;
        float4 c  = image.Sample(def_sampler, uv);
        float4 up = image.Sample(def_sampler, uv + float2(0.0, -sh));
        float4 dn = image.Sample(def_sampler, uv + float2(0.0,  sh));
        float4 lf = image.Sample(def_sampler, uv + float2(-sh, 0.0));
        float4 rt = image.Sample(def_sampler, uv + float2( sh, 0.0));
        float4 sharp = c * (1.0 + 4.5 * amt) - (up + dn + lf + rt) * (1.125 * amt);
        sharp.rgb = (sharp.rgb - 0.5) * (1.0 + amt * 0.65) + 0.5;
        return saturate(sharp);
    }
}

// Type 1: DELAY (Rhythmic Multi-Tap Beat Echo / Discrete Ghost Stamping)
// Eltér a reverbtől: nagy kiterjedésű, ritmikus szellemképek és diszkrét bélyegzés
float4 fx_delay(float2 uv, float v, float amt, float bpm, float anim_time, float bass, float amp)
{
    float beat_sec = 60.0 / max(bpm, 60.0);
    float echo_time = beat_sec * 0.75; // 3/4 beat echo
    float phase = frac(anim_time / echo_time);

    float4 base_col = image.Sample(def_sampler, uv);

    if (v < 0.0) {
        // DUB DELAY / DARK ECHO:
        // Nagy távolságú, fokozatosan sötétbe vesző késleltetett sziluettek
        float2 dir = float2(-0.09, 0.05) * amt * (1.0 + bass * 0.35);
        float2 uv1 = uv + dir * (1.0 + phase * 0.35);
        float2 uv2 = uv + dir * (2.2 + phase * 0.35);
        float2 uv3 = uv + dir * (3.5 + phase * 0.35);

        float m1 = step(0.0, uv1.x) * step(uv1.x, 1.0) * step(0.0, uv1.y) * step(uv1.y, 1.0);
        float m2 = step(0.0, uv2.x) * step(uv2.x, 1.0) * step(0.0, uv2.y) * step(uv2.y, 1.0);
        float m3 = step(0.0, uv3.x) * step(uv3.x, 1.0) * step(0.0, uv3.y) * step(uv3.y, 1.0);

        float4 e1 = image.Sample(def_sampler, saturate(uv1)) * m1;
        float4 e2 = image.Sample(def_sampler, saturate(uv2)) * m2;
        float4 e3 = image.Sample(def_sampler, saturate(uv3)) * m3;

        // Sötét analóg szalag lecsengés (Dark tape decay)
        e1.rgb *= float3(0.55, 0.60, 0.65);
        e2.rgb *= float3(0.35, 0.38, 0.42);
        e3.rgb *= float3(0.18, 0.20, 0.22);

        float4 res = base_col + (e1 * 0.50 + e2 * 0.35 + e3 * 0.22) * amt;
        return saturate(res);
    } else {
        // PING-PONG BRIGHT ECHO:
        // Balra-jobbra ugráló, éles sztereó szellemképek prizmatikus RGB split-tel
        float ping = (frac(anim_time / (beat_sec * 0.5)) > 0.5) ? 1.0 : -1.0;
        float2 dir1 = float2(0.11 * ping, 0.03) * amt * (1.0 + bass * 0.35);
        float2 dir2 = float2(-0.20 * ping, -0.05) * amt * (1.0 + bass * 0.35);

        float2 uv1 = uv + dir1;
        float2 uv2 = uv + dir2;

        float m1 = step(0.0, uv1.x) * step(uv1.x, 1.0) * step(0.0, uv1.y) * step(uv1.y, 1.0);
        float m2 = step(0.0, uv2.x) * step(uv2.x, 1.0) * step(0.0, uv2.y) * step(uv2.y, 1.0);

        // Prizmatikus RGB elcsúszás a szellemképen
        float split = amt * 0.018 * (1.0 + bass * 0.40);
        float4 e1;
        e1.r = image.Sample(def_sampler, saturate(uv1 + float2( split, 0.0))).r * m1;
        e1.g = image.Sample(def_sampler, saturate(uv1)).g * m1;
        e1.b = image.Sample(def_sampler, saturate(uv1 - float2( split, 0.0))).b * m1;
        e1.a = m1;

        float4 e2 = image.Sample(def_sampler, saturate(uv2)) * m2;

        // Fényes, szikrázó visszhang hozzáadás (Additive shine)
        float3 echo_sum = e1.rgb * 0.65 + e2.rgb * 0.40;
        float4 res = base_col;
        res.rgb += echo_sum * (amt * 1.25 * (1.0 + amp * 0.35));
        return saturate(res);
    }
}

// Type 2: REVERB (Expansive Ethereal Spatial Bloom & Cathedral Light Wash)
// Eltér a delaytől: lágy, szétfolyó diffúz fényköd és térmélység (nincsenek éles szellemképek)
float4 fx_reverb(float2 uv, float v, float amt, float mid, float high, float amp)
{
    float4 base_col = image.Sample(def_sampler, uv);
    float thresh = max(0.18, 0.65 - amt * 0.45);
    float rad = amt * 0.045 * (1.0 + (mid + high) * 0.35); // Nagy kiterjedésű, lágy diffúziós sugár
    float inv_range = 1.0 / max(1.0 - thresh, 0.01);

    float4 bloom = float4(0.0, 0.0, 0.0, 0.0);
    float4 s1 = image.Sample(def_sampler, uv + float2( rad,  0.0));
    float4 s2 = image.Sample(def_sampler, uv + float2(-rad,  0.0));
    float4 s3 = image.Sample(def_sampler, uv + float2( 0.0,  rad));
    float4 s4 = image.Sample(def_sampler, uv + float2( 0.0, -rad));
    float4 s5 = image.Sample(def_sampler, uv + float2( rad * 0.707,  rad * 0.707));
    float4 s6 = image.Sample(def_sampler, uv + float2(-rad * 0.707,  rad * 0.707));
    float4 s7 = image.Sample(def_sampler, uv + float2( rad * 0.707, -rad * 0.707));
    float4 s8 = image.Sample(def_sampler, uv + float2(-rad * 0.707, -rad * 0.707));

    bloom += s1 * saturate((dot(s1.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s2 * saturate((dot(s2.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s3 * saturate((dot(s3.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s4 * saturate((dot(s4.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s5 * saturate((dot(s5.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s6 * saturate((dot(s6.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s7 * saturate((dot(s7.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom += s8 * saturate((dot(s8.rgb, float3(0.299, 0.587, 0.114)) - thresh) * inv_range);
    bloom *= 0.125;

    if (v < 0.0) {
        // ABYSS / DARK CAVERN: Sötét, misztikus köd, a háttér és a fények tompulnak
        base_col.rgb = lerp(base_col.rgb, bloom.rgb * float3(0.65, 0.75, 0.95), amt * 0.65);
        base_col.rgb *= (1.0 - amt * 0.28);
    } else {
        // CATHEDRAL SOLAR BLOOM: Hatalmas, szétterülő vakító fehér-arany fényszivárgás
        float3 aura = bloom.rgb * float3(1.30, 1.20, 1.05) * (1.0 + amp * 0.35);
        base_col.rgb += aura * (amt * 2.4);
    }
    return saturate(base_col);
}

// Type 3: FLANGER (Jet Sweep Wave Warp & Chromatic Aberration RGB Shift)
float4 fx_flanger(float2 uv, float v, float amt, float anim_time, float bass)
{
    float wave_phase = uv.y * 18.0 + anim_time * (v < 0.0 ? 3.0 : 6.5);
    float2 wave = float2(sin(wave_phase), cos(wave_phase * 0.75)) * (amt * 0.025 * (1.0 + bass * 0.50));
    float2 center_uv = uv + wave;

    float split = amt * 0.020 * (1.0 + bass * 0.45);
    float r = image.Sample(def_sampler, center_uv + float2( split, 0.0)).r;
    float g = image.Sample(def_sampler, center_uv).g;
    float b = image.Sample(def_sampler, center_uv - float2( split, 0.0)).b;
    float a = image.Sample(def_sampler, center_uv).a;

    if (v < 0.0) {
        // Mély folyékony hullámzás (Deep Liquid Sub-Wave) sötétebb tónussal
        float3 res = float3(r, g, b) * (1.0 - amt * 0.25);
        return saturate(float4(res, a));
    } else {
        // Sugárhajtómű-szerű fémes interferencia (Jet sweep sheen) + kifehéredő csillogás
        float sweep = sin(anim_time * 4.5 + uv.x * 3.14159) * 0.5 + 0.5;
        float3 res = float3(r, g, b) + float3(0.20, 0.28, 0.35) * (sweep * amt * 0.65);
        return saturate(float4(res, a));
    }
}
)";

static const char *MIXER_FX_HLSL_PART2 = R"(

// Type 4: BEATMASHER 2 (Rhythmic Beat-Synced Glitch / Heavy Slices / Machine-Gun Strobe)
float4 fx_beatmasher(float2 uv, float v, float amt, float bpm, float anim_time, float bass, float amp)
{
    float beat_sec = 60.0 / max(bpm, 60.0);

    // 8-fokozatú zenei leosztás (1/4 -> 1/8 -> 1/16 -> 1/24 -> 1/32)
    float div = 0.25;
    if (amt >= 0.85)      div = 0.03125; // 1/32 beat (gépfegyver darálás)
    else if (amt >= 0.70) div = 0.04167; // 1/24 beat
    else if (amt >= 0.50) div = 0.0625;  // 1/16 beat
    else if (amt >= 0.30) div = 0.125;   // 1/8 beat
    else                  div = 0.25;    // 1/4 beat

    float slice_sec = beat_sec * div;
    // Lábdob beütésnél plusz zenei fázisugrás (Kick-synced stutter jump)
    float kick_nudge = (bass > 0.40) ? (bass - 0.40) * 0.45 : 0.0;
    float eff_time = anim_time + kick_nudge;
    float step_idx = floor(eff_time / slice_sec);
    float step_phase = frac(eff_time / slice_sec);

    // Sávok száma: kis potinál 8 sáv, maximumon 36 sáv
    float num_bands = floor(lerp(8.0, 36.0, amt));
    float band = floor(uv.y * num_bands);
    float band_rnd = frac(sin(band * 127.1 + step_idx * 311.7) * 43758.5453);

    float2 slice_uv = uv;

    // Drasztikus horizontális elcsúszás a lábdob energiájával felerősítve
    if (band_rnd > 0.30 || bass > 0.55) {
        float shift = (band_rnd - 0.5) * amt * (0.90 + bass * 0.45);
        slice_uv.x = frac(slice_uv.x + shift);
    }

    // Kvantált blokk-ugrálás és Stutter Loop
    if (amt > 0.30 && (step_phase < 0.45 || bass > 0.60)) {
        float block_rnd = frac(sin(step_idx * 987.65) * 43758.5453);
        if (block_rnd > 0.40) {
            slice_uv.y = frac(slice_uv.y + (block_rnd - 0.5) * amt * (0.35 + bass * 0.25));
        }
    }

    // Szeletenkénti RGB Chromatic Aberration
    float split = amt * 0.025 * (1.0 + bass * 0.50) * (band_rnd > 0.5 ? 1.0 : -1.0);
    float4 col;
    col.r = image.Sample(def_sampler, frac(slice_uv + float2( split, 0.0))).r;
    col.g = image.Sample(def_sampler, frac(slice_uv)).g;
    col.b = image.Sample(def_sampler, frac(slice_uv - float2( split, 0.0))).b;
    col.a = 1.0;

    if (v < 0.0) {
        // DARK CYBER SLICER: Fekete scanline kiesések és digitális signal drop-out
        if (band_rnd > 0.70 || (bass > 0.70 && band_rnd > 0.45)) {
            col.rgb *= 0.15; // Kiesett fekete sáv
        }
        if (amt > 0.45 && step_phase < 0.20 && frac(step_idx * 0.25) < 0.26) {
            col.rgb *= 0.10;
        }
    } else {
        // NEON STROBE SLICER: Ragyogó fehér csíkok és stroboszkóp inverzió dobtaktusra
        if (band_rnd > 0.80 || (bass > 0.65 && step_phase < 0.30)) {
            col.rgb = 1.0 - col.rgb;
        }
        if (amt > 0.35 && (step_phase < 0.25 || bass > 0.55)) {
            col.rgb += float3(0.55, 0.55, 0.55) * (amt * (1.0 + bass * 0.50));
        }
    }

    return saturate(col);
}

// Type 5: TURNTABLE FX (Bipolar: Vinyl Brake vs. Spinback Twirl)
// v < 0: Brake (lassulás, kimerevedés, vertikális nyúlás, elhalványuló sötétedés)
// v > 0: Spinback (örvénylő Twirl, centrifugális forgási elmosódás, villanások)
float4 fx_turntable(float2 uv, float v, float amt, float anim_time, float bass)
{
    float2 center = float2(0.5, 0.5);
    float2 p = uv - center;

    if (v < 0.0) {
        // BRAKE: Anamorphic Vertical Stretch + Drag Scanline + Dimming
        float stretch = 1.0 + amt * (0.55 + bass * 0.25);
        float2 brake_uv = float2(p.x / stretch, p.y * stretch) + center;
        float4 col = image.Sample(def_sampler, brake_uv);
        col.rgb *= (1.0 - amt * 0.45);
        return saturate(col);
    } else {
        // SPINBACK: Polar Twirl / Swirl + Radial Spin Blur + Bright Flashes
        float r = length(p * float2(1.7777778, 1.0));
        float angle = atan2(p.y, p.x);
        float twirl = (1.0 - saturate(r * 1.35)) * amt * (4.5 + bass * 1.5);
        angle += twirl - (anim_time + bass * 0.12) * amt * 8.0;

        float2 twirl_uv = float2(cos(angle), sin(angle)) * length(p) + center;
        float4 col = image.Sample(def_sampler, twirl_uv);

        float4 b1 = image.Sample(def_sampler, float2(cos(angle + amt * 0.08), sin(angle + amt * 0.08)) * length(p) + center);
        float4 b2 = image.Sample(def_sampler, float2(cos(angle - amt * 0.08), sin(angle - amt * 0.08)) * length(p) + center);
        col = (col * 0.5) + (b1 * 0.25) + (b2 * 0.25);
        col.rgb += float3(0.20, 0.20, 0.25) * (amt * (0.50 + bass * 0.35));
        return saturate(col);
    }
}

// Type 6: RAMP DELAY (Infinite Inward Tunnel Zoom vs. Outward Warp Speed)
float4 fx_ramp_delay(float2 uv, float v, float amt, float anim_time, float bass, float amp)
{
    float2 p = (uv - 0.5) * float2(1.7777778, 1.0);
    float r = max(length(p), 0.001);
    float angle = atan2(p.y, p.x);

    if (v < 0.0) {
        // INWARD DARK VORTEX (Befelé zuhanó sötét alagút)
        float speed = (2.0 + amt * 6.5) * (1.0 + bass * 0.35);
        float log_r = frac(log(r) * 0.65 - anim_time * speed);
        float swirl = angle + log_r * 3.14159 * (1.0 + amt * 2.5);

        float2 tunnel_uv = float2(cos(swirl), sin(swirl)) * log_r * 0.5 + 0.5;
        float4 col_tunnel = image.Sample(def_sampler, tunnel_uv) * (1.0 - amt * 0.40);
        float4 col_orig   = image.Sample(def_sampler, uv);
        return saturate(lerp(col_orig, col_tunnel, saturate(amt * (1.25 + amp * 0.30))));
    } else {
        // OUTWARD WARP SPEED (Kifelé táguló gyorsuló fehér fényalagút)
        float speed = (2.5 + amt * 7.5) * (1.0 + bass * 0.35);
        float log_r = frac(1.0 - (log(r) * 0.65 + anim_time * speed));
        float swirl = angle - log_r * 3.14159 * (1.0 + amt * 2.5);

        float2 tunnel_uv = float2(cos(swirl), sin(swirl)) * log_r * 0.5 + 0.5;
        float4 col_tunnel = image.Sample(def_sampler, tunnel_uv);
        col_tunnel.rgb += float3(0.35, 0.35, 0.40) * (amt * (1.0 + amp * 0.45));
        float4 col_orig   = image.Sample(def_sampler, uv);
        return saturate(lerp(col_orig, col_tunnel, saturate(amt * (1.25 + amp * 0.30))));
    }
}

// Type 7: MULHOLLAND DRIVE (Analog Overdrive / Solarize Foldback / Film Grain)
float4 fx_mulholland(float2 uv, float v, float amt, float anim_time, float bass, float amp)
{
    float4 col = image.Sample(def_sampler, uv);

    if (v < 0.0) {
        // DARK ANALOG TAPE CRUNCH: Meleg szalagtelítés, sötét kontraszt, CRT zaj
        col.rgb *= (1.0 + amt * 2.5 * (1.0 + amp * 0.60));
        col.rgb = saturate((col.rgb - 0.5) * (1.0 + amt * 0.8) + 0.5);
        col.rgb *= (1.0 - amt * 0.35);

        float grain = frac(sin(dot(uv * (anim_time + 1.23), float2(12.9898, 78.233))) * 43758.5453);
        col.rgb += (grain - 0.5) * (amt * (0.30 + bass * 0.20));
        return saturate(col);
    } else {
        // SOLAR FLARE OVERDRIVE: Vakító fehérbe égő csöves túlhajtás és szolárizáció
        col.rgb *= (1.0 + amt * 5.5 * (1.0 + amp * 0.70));
        if (amt > 0.15) {
            float3 fold = abs(sin(col.rgb * 1.5707963));
            col.rgb = lerp(col.rgb, fold, saturate((amt - 0.15) / 0.85));
        }
        col.rgb += float3(0.30, 0.30, 0.30) * (amt * (1.0 + amp * 0.40));
        float grain = frac(sin(dot(uv * (anim_time + 1.23), float2(12.9898, 78.233))) * 43758.5453);
        col.rgb += (grain - 0.5) * (amt * (0.20 + bass * 0.15));
        return saturate(col);
    }
}

// Type 8: TRANSPOSE STRETCH (Bipolar: Barrel Bulge vs. Pincushion Pinch)
// v < 0: Pitch Down -> Bulge / Fisheye
// v > 0: Pitch Up   -> Pinch / Pincushion
float4 fx_transpose(float2 uv, float v, float amt, float bass)
{
    float2 p = (uv - 0.5) * float2(1.7777778, 1.0);
    float r = length(p);

    float2 dist_p;
    if (v < 0.0) {
        // BULGE (Halszemoptika / Domborodás)
        float factor = 1.0 + amt * 1.35 * r * r * (1.0 + bass * 0.30);
        dist_p = p / factor;
    } else {
        // PINCH (Párnatorzítás / Behorpadás)
        float factor = 1.0 + amt * 1.45 * (1.0 - saturate(r * 1.2)) * (1.0 + bass * 0.30);
        dist_p = p * factor;
    }

    dist_p.x /= 1.7777778;
    float2 dist_uv = dist_p + 0.5;

    // Szélmaszk a határokon túli mintavételezés feketítésére
    float mask = step(0.0, dist_uv.x) * step(dist_uv.x, 1.0) * step(0.0, dist_uv.y) * step(dist_uv.y, 1.0);
    float4 col = image.Sample(def_sampler, saturate(dist_uv)) * mask;
    return col;
}

// Fő koordinátor függvény a 9 effekt és Visual EQ együttes lekezelésére
float4 apply_deck_mixer_fx(float2 uv, float fx_type, float fx_val, float bpm, float anim_time, float4 gains, float sat, bool vis_eq, float bass, float mid, float high, float amp)
{
    float4 col;
    float amt = abs(fx_val);
    if (amt <= 0.005) {
        // ZÉRÓ GPU TERHELÉS: Középállásban közvetlen minta, minden torzítás és módosítás átugrása!
        col = image.Sample(def_sampler, uv);
    } else {
        // Agresszív kezdeti görbe (|v|^0.62) mint az audióban
        float curve_amt = pow(saturate(amt), 0.62);

        int t = (int)(fx_type + 0.5);
        if (t == 0)      col = fx_filter(uv, fx_val, curve_amt, bass);
        else if (t == 1) col = fx_delay(uv, fx_val, curve_amt, bpm, anim_time, bass, amp);
        else if (t == 2) col = fx_reverb(uv, fx_val, curve_amt, mid, high, amp);
        else if (t == 3) col = fx_flanger(uv, fx_val, curve_amt, anim_time, bass);
        else if (t == 4) col = fx_beatmasher(uv, fx_val, curve_amt, bpm, anim_time, bass, amp);
        else if (t == 5) col = fx_turntable(uv, fx_val, curve_amt, anim_time, bass);
        else if (t == 6) col = fx_ramp_delay(uv, fx_val, curve_amt, anim_time, bass, amp);
        else if (t == 7) col = fx_mulholland(uv, fx_val, curve_amt, anim_time, bass, amp);
        else if (t == 8) col = fx_transpose(uv, fx_val, curve_amt, bass);
        else             col = image.Sample(def_sampler, uv);

        // =====================================================================
        // UNIVERZÁLIS BIPOLÁRIS TÓNUSLÁNC MIND A 9 EFFEKTRE
        // Balra (v < 0, LPF / Mély irány): Sötétedés, deszaturáció, peremsötétedés
        // Jobbra (v > 0, HPF / Magas irány): Kifehéredés, ragyogás, expozíció emelés
        // =====================================================================
        if (fx_val < 0.0) {
            // Sötétedés & Feketék mélyülése
            col.rgb *= (1.0 - curve_amt * 0.42);

            // Deszaturáció (monokróm / fakuló mélység)
            float luma = dot(col.rgb, float3(0.2126, 0.7152, 0.0722));
            col.rgb = lerp(luma.xxx, col.rgb, saturate(1.0 - curve_amt * 0.75));

            // Finom mélyszűrő vinnyetta
            float2 vp = (uv - 0.5) * float2(1.7777778, 1.0);
            float vig = 1.0 - smoothstep(0.35, 1.15, length(vp)) * (curve_amt * 0.55);
            col.rgb *= saturate(vig);
        } else {
            // Kifehéredés & Világosodó csúcsfények (Exposure Wash & Shine)
            col.rgb = col.rgb * (1.0 + curve_amt * 0.35) + float3(0.30, 0.30, 0.33) * pow(curve_amt, 1.3);

            // Szaturáció megőrzés / enyhe élénkítés
            float luma = dot(col.rgb, float3(0.2126, 0.7152, 0.0722));
            col.rgb = lerp(luma.xxx, col.rgb, 1.0 + curve_amt * 0.30);
        }
    }

    // Színkorrekció és Visual EQ (Filter szaturáció + 3-sávos RGB gain)
    if (vis_eq) {
        col.rgb *= gains.rgb;
        float eq_luma = dot(col.rgb, float3(0.2126, 0.7152, 0.0722));
        col.rgb = lerp(eq_luma.xxx, col.rgb, sat);
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
uniform float bass_energy;
uniform float mid_energy;
uniform float high_energy;
uniform float audio_amp;

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
    float4 col = apply_deck_mixer_fx(v.uv, mixer_fx_type, mixer_fx_val, bpm, anim_time, gains, saturation, visual_eq_enabled > 0.5, bass_energy, mid_energy, high_energy, audio_amp);
    
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
        s_code += MIXER_FX_HLSL_PART1;
        s_code += MIXER_FX_HLSL_PART2;
        s_code += DECK_COLOR_EFFECT_SUFFIX;
    }
    return s_code.c_str();
}
