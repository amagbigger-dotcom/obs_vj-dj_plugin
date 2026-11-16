#pragma once

// =============================================================================
// VJ Visualizer Shader Suite (Direct3D 11 / OpenGL HLSL)
// 4 Választható Mód:
//   0: FluidFlowSNoise (mojovideotech folyadék áramlás - Default)
//   1: TruchetDualLevelSpin (mojovideotech kettős szintű forgó labirintus)
//   2: HypnoWorm (mojovideotech 30-lépéses hipnotikus féregalagút)
//   3: TriangleSquareTwist (Joseph Fiola / xLights elektro hullám csavarás)
// =============================================================================

static const char *VJ_VISUALIZER_EFFECT = R"(
uniform float4x4 ViewProj;
uniform texture2d dummy_texture;

uniform float visualizer_mode;      // 0: FluidFlow, 1: Truchet, 2: HypnoWorm, 3: TriangleTwist
uniform float anim_time;            // Másodperc alapú futásidő
uniform float bpm;                  // Deck BPM sebesség
uniform float audio_amp;            // RMS pillanatnyi hangerő [0.0 .. 1.0]
uniform float bass_energy;          // Lábdob / Mélyhang transient lüktetés [0.0 .. 1.0]
uniform float mid_energy;           // Középtartomány energia [0.0 .. 1.0]
uniform float high_energy;          // Magashang energia [0.0 .. 1.0]
uniform float is_deck_b;            // 0.0 = Deck A, 1.0 = Deck B
uniform float pulse_enabled;        // 1.0 = Zenei lüktetés be, 0.0 = ki
uniform float tone_tracking_enabled;// 1.0 = Spektrum tónuskövetés be, 0.0 = ki

sampler_state def_sampler {
    Filter = Linear;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct VertData {
    float4 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

VertData VSDefault(VertData v)
{
    VertData v_out;
    v_out.pos = mul(float4(v.pos.xyz, 1.0), ViewProj);
    v_out.uv  = v.uv;
    return v_out;
}

// -----------------------------------------------------------------------------
// Matematikai segédfüggvények (Math Helpers)
// -----------------------------------------------------------------------------
float glsl_mod(float x, float y)
{
    return x - y * floor(x / y);
}

float2 rotate2D(float2 p, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(p.x * c - p.y * s, p.x * s + p.y * c);
}

// -----------------------------------------------------------------------------
// SHADER 0: FluidFlowSNoise (Simplex Noise Fluid Flow)
// -----------------------------------------------------------------------------
float snoise_fluid(float3 v, float freq, float freq2)
{
    const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);
    const float4 D = float4(0.0, 0.5, 1.0, 2.0);

    float3 i = floor(v + dot(v, C.yyy));
    float3 x0 = v - i + dot(i, C.xxx);

    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    float3 x1 = x0 - i1 + 1.0 * C.xxx;
    float3 x2 = x0 - i2 + 2.0 * C.xxx;
    float3 x3 = x0 - 1.0 + 3.0 * C.xxx;

    i = glsl_mod(i, 289.0);
    float4 p_val = glsl_mod(((i.z + float4(0.0, i1.z, i2.z, 1.0)) * 34.0 + 1.0) * (i.z + float4(0.0, i1.z, i2.z, 1.0)), 289.0);
    p_val = glsl_mod((p_val + (i.y + float4(0.0, i1.y, i2.y, 1.0)) * 34.0 + 1.0) * (p_val + (i.y + float4(0.0, i1.y, i2.y, 1.0))), 289.0);
    p_val = glsl_mod((p_val + (i.x + float4(0.0, i1.x, i2.x, 1.0)) * 34.0 + 1.0) * (p_val + (i.x + float4(0.0, i1.x, i2.x, 1.0))), 289.0);

    float4 j = p_val - 49.0 * floor(p_val * (1.0 / 49.0));
    float4 x_ = floor(j * (1.0 / 7.0));
    float4 y_ = floor(j - 7.0 * x_);

    float4 x = x_ * (2.0 / 7.0) + 0.5 / 7.0 - 1.0;
    float4 y = y_ * (2.0 / 7.0) + 0.5 / 7.0 - 1.0;
    float4 h = 1.0 - abs(x) - abs(y);

    float4 b0 = float4(x.xy, y.xy);
    float4 b1 = float4(x.zw, y.zw);

    float4 s0 = floor(b0) * 2.0 + 1.0;
    float4 s1 = floor(b1) * 2.0 + 1.0;
    float4 sh = -step(h, float4(0.0, 0.0, 0.0, 0.0));

    float4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    float4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    float3 p0 = float3(a0.xy, h.x);
    float3 p1 = float3(a0.zw, h.y);
    float3 p2 = float3(a1.xy, h.z);
    float3 p3 = float3(a1.zw, h.w);

    float4 norm = rsqrt(float4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)) + 0.00001);
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    float4 m = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m * m, float4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

float4 render_fluid_flow(float2 uv)
{
    float b_val = (bpm > 20.0) ? bpm : 120.0;
    float rate = clamp(b_val / 120.0, 0.6, 2.5) * 1.5;
    float t_flow = anim_time * rate;

    float p_kick = (pulse_enabled > 0.5) ? bass_energy : 0.0;
    float p_amp  = (pulse_enabled > 0.5) ? audio_amp : 0.0;

    // Erőteljes zenei lüktetés: a koordináták a lábdob leütésekor kitágulnak
    float2 centered = uv - float2(0.5, 0.5);
    float2 flow_uv = centered * (1.0 - p_kick * 0.22) + float2(0.5, 0.5);

    float depth = 0.77;
    float blur = 0.12;
    if (pulse_enabled > 0.5) {
        depth = clamp(0.64 + p_kick * 0.28, 0.50, 0.96);
        blur  = clamp(0.08 + p_amp * 0.22, 0.05, 0.35);
    }

    float3 c1, c2;
    if (is_deck_b > 0.5) {
        c1 = float3(0.20, 0.04, 0.01);
        c2 = float3(1.00, 0.55, 0.05);
    } else {
        c1 = float3(0.01, 0.04, 0.20);
        c2 = float3(0.05, 0.75, 1.00);
    }

    if (tone_tracking_enabled > 0.5) {
        float h_boost = high_energy * 0.35;
        float m_boost = mid_energy * 0.25;
        c2 += float3(h_boost, m_boost, h_boost * 1.2);
    }

    float n1 = 0.0;
    float3 flow_pos = float3(flow_uv * 3.0, t_flow * 0.2);
    for (int k = 0; k < 12; ++k) {
        float f_step = (float)k;
        n1 += snoise_fluid(flow_pos + float3(0.0, 0.0, f_step * 0.08), 2.2, 1.1) * (1.0 / (f_step + 1.0));
    }

    float edge_val = smoothstep(depth - blur, depth + blur, n1 * 0.5 + 0.5);
    float3 col = lerp(c1, c2, edge_val);
    return float4(saturate(col), 1.0);
}

// -----------------------------------------------------------------------------
// SHADER 1: TruchetDualLevelSpin (mojovideotech Dual Level Truchet)
// -----------------------------------------------------------------------------
float2 hash22(float2 p)
{
    float2 dot_val = float2(dot(p * float2(91.45, 243.21), float2(57.0, 27.0)),
                            dot(p * float2(91.45, 243.21), float2(27.0, 57.0)));
    return frac(sin(float2(262144.0, 32768.0)) * sin(dot_val));
}

float4 render_truchet(float2 uv)
{
    float b_val = (bpm > 20.0) ? bpm : 120.0;
    float rate = clamp(b_val / 120.0, 0.5, 2.5) * 2.5;
    float T = anim_time * rate;

    float p_kick = (pulse_enabled > 0.5) ? bass_energy : 0.0;
    float p_amp  = (pulse_enabled > 0.5) ? audio_amp : 0.0;

    float2 p_center = uv - float2(0.5, 0.5);
    p_center.x *= 1.7778;

    float spin_angle = anim_time * 0.15;
    p_center = rotate2D(p_center, spin_angle);

    // Lüktetés a léptékben
    float sc = 0.3 * (1.0 - p_kick * 0.18);
    float2 oP = p_center * 10.0 + float2(0.5, T * 0.4);

    float2 d = float2(1e5, 1e5);
    float2 rndTh = float2(0.99, 1.0);
    float dim = sc;

    float pulse_term = sin(anim_time * 4.0) * 0.075 * (1.0 + p_kick * 2.0);
    float thickness = 1.0 + p_amp * 1.5;

    for (int k = 0; k < 2; ++k) {
        float2 ip = floor(oP * dim);
        float2 rnd = hash22(ip);
        if (rnd.x < rndTh[k]) {
            float hd = 0.5 / dim;
            float2 p = oP - (ip + 0.5) / dim;
            d.y = abs(max(abs(p.x), abs(p.y)) - hd) - 0.333 / 6.0;
            p.y *= (rnd.y > 0.5) ? 1.0 : -1.0;
            float aw = 0.5 / 3.0 / dim;
            p = (p.x > -p.y) ? p : -p.yx;
            d.x = abs(length(p - hd) - hd) - aw;
            d.x *= (k == 1) ? -1.0 : 1.0;
            d.x = min(d.x, length(abs(p) - hd) - aw);
            d.x -= 0.05 * thickness;
            d.x -= pulse_term;
            break;
        }
        dim *= 2.0;
    }

    float fo = (10.1 - 6.0) / 600.0;
    float3 col = float3(0.01, 0.01, 0.02);
    col = lerp(col, float3(0.0, 0.0, 0.0), (1.0 - smoothstep(0.0, fo * 5.0, d.y - 0.1)) * 0.15);
    col = lerp(col, float3(1.0, 1.0, 1.0), (1.0 - smoothstep(0.0, fo, d.y)) * 0.15);

    fo = 21.0 / 600.0 / sqrt(max(dim, 0.001));
    float sh = max(0.75 - d.x * 7.5, 0.0);
    sh *= clamp(-sin(d.x * 6.28318 * 7.5) + 1.0, 0.25, 1.0) + 0.0025;

    col = lerp(col, float3(0.0, 0.0, 0.0), 1.0 - smoothstep(0.0, fo, d.x));

    float3 c_fill, c_core;
    if (is_deck_b > 0.5) {
        c_fill = float3(0.9, 0.35, 0.05);
        c_core = float3(1.0, 0.7, 0.1);
    } else {
        c_fill = float3(0.05, 0.35, 0.9);
        c_core = float3(0.1, 0.8, 1.0);
    }

    if (tone_tracking_enabled > 0.5) {
        c_core += float3(mid_energy * 0.3, high_energy * 0.3, high_energy * 0.3);
    }

    col = lerp(col, c_fill * sh, 1.0 - smoothstep(0.0, fo, d.x + 0.21));
    col = lerp(col, c_core * sh, 1.0 - smoothstep(0.0, fo, abs(d.x + 0.12) - 0.02));

    return float4(saturate(sqrt(max(col, 0.0))), 1.0);
}

// -----------------------------------------------------------------------------
// SHADER 2: HypnoWorm (mojovideotech OpArt Concentric Hypnotic Worm)
// -----------------------------------------------------------------------------
float4 render_hypno_worm(float2 uv)
{
    float b_val = (bpm > 20.0) ? bpm : 120.0;
    float rate = clamp(b_val / 120.0, 0.5, 2.5) * 1.6;
    float T = anim_time * rate;

    float p_kick = (pulse_enabled > 0.5) ? bass_energy : 0.0;
    float p_amp  = (pulse_enabled > 0.5) ? audio_amp : 0.0;

    float3 col1, col2;
    if (is_deck_b > 0.5) {
        col1 = float3(1.0, 0.2, 0.1);
        col2 = float3(1.0, 0.75, 0.1);
    } else {
        col1 = float3(0.1, 0.3, 1.0);
        col2 = float3(0.1, 0.9, 0.8);
    }

    if (tone_tracking_enabled > 0.5) {
        col2 = lerp(col2, float3(1.0, 0.9, 0.4), mid_energy * 0.5);
    }

    float3 fc = col1;
    float2 p = (uv - float2(0.5, 0.5)) * 2.0;
    p.x *= 1.7778;
    p *= (11.0 - 8.95);

    float amp1 = 0.08 + p_kick * 0.14;
    float amp2 = 0.05 + p_kick * 0.12;

    float2 pos = float2(0.0, 0.0);
    for (float i = 24.0; i > 0.0; i -= 1.0) {
        pos += float2(sin(T + (-0.5 - i) * -1.05) * amp1,
                      cos(T + (1.0 - i) * 0.15) * amp2);
        float dist = length(pos - p) - 0.075 * i;
        float o = 1.0 - smoothstep(-0.01, 23.0 / 1920.0, dist);
        fc = lerp(fc, float3(1.0, 1.0, 1.0) * (1.0 - glsl_mod(i, 2.0)), o);
    }

    float3 cc = col1.gbr * col2.rgb;
    float4 col = float4(pow(abs(col2.rgb), abs(fc.rgb)) - cc, 1.0);
    col = pow(abs(col), float4(1.0 / 0.9, 1.0 / 0.9, 1.0 / 0.9, 1.0));
    return saturate(col);
}

// -----------------------------------------------------------------------------
// SHADER 3: TriangleSquareTwist (Joseph Fiola / xLights Electro Geometric Wave)
// -----------------------------------------------------------------------------
float electro_wave(float2 p_uv, float d, float f, float o, float a, float b_param, float time_sec)
{
    float theta = atan2(p_uv.y, p_uv.x);
    float amp = smoothstep(0.0, 1.0, (sin(theta + time_sec * 3.14159) * 0.5 + 0.5) - b_param) * a;
    float phase = d + sin(theta * f + o + time_sec * 3.14159) * amp;
    return sin(clamp(phase, 0.0, 6.28318) + 1.57079) + 1.0005;
}

float4 render_triangle_twist(float2 uv)
{
    float b_val = (bpm > 20.0) ? bpm : 120.0;
    float rate = clamp(b_val / 120.0, 0.5, 2.5) * 1.5;
    float t_sec = anim_time * rate * 0.3;

    float p_kick = (pulse_enabled > 0.5) ? bass_energy : 0.0;
    float p_amp  = (pulse_enabled > 0.5) ? audio_amp : 0.0;

    float2 p = uv - float2(0.5, 0.5);
    p.x *= 1.7778;

    float zoom = 1.0 + p_kick * 0.35;
    p *= zoom;

    float grey = 0.0;
    float thickness = 0.003 + p_amp * 0.009;
    float amplitude = 15.0 + p_kick * 45.0;
    float twist_step = 0.02 + p_kick * 0.03;

    for (int i = 0; i < 18; ++i) {
        float root2 = 1.732;
        float d = dot(p, float2(0.0, -2.0));
        d = max(d, dot(p, float2(-root2, 1.0)));
        d = max(d, dot(p, float2(root2, 1.0)));

        grey += 1.0 - smoothstep(0.0, thickness, electro_wave(p, d * 10.0, 0.0, 0.0, amplitude, 0.0, t_sec));
        grey += 1.0 - smoothstep(0.0, thickness, electro_wave(p, d * 10.0, 0.0, 1.57, amplitude, 0.0, t_sec));
        grey += 1.0 - smoothstep(0.0, thickness, electro_wave(p, d * 10.0, 0.0, 3.14, amplitude, 0.0, t_sec));

        p *= 1.08;
        p = rotate2D(p, twist_step * -6.28318);
    }

    float3 theme_col;
    if (is_deck_b > 0.5) {
        theme_col = float3(1.0, 0.55, 0.1);
    } else {
        theme_col = float3(0.0, 0.8, 1.0);
    }

    if (tone_tracking_enabled > 0.5) {
        theme_col += float3(mid_energy * 0.2, high_energy * 0.3, high_energy * 0.3);
    }

    float3 out_rgb = theme_col * grey;
    return float4(saturate(out_rgb), 1.0);
}

// -----------------------------------------------------------------------------
// Pixel Shader Főbelépési Pont (Main Entry Point with 4 Selectable Modes)
// -----------------------------------------------------------------------------
float4 PSVisualizer(VertData v) : TARGET
{
    if (visualizer_mode < 0.5) {
        // Mode 0: FluidFlowSNoise (Default)
        return render_fluid_flow(v.uv);
    } else if (visualizer_mode < 1.5) {
        // Mode 1: TruchetDualLevelSpin
        return render_truchet(v.uv);
    } else if (visualizer_mode < 2.5) {
        // Mode 2: HypnoWorm
        return render_hypno_worm(v.uv);
    } else {
        // Mode 3: TriangleSquareTwist
        return render_triangle_twist(v.uv);
    }
}

technique Draw
{
    pass
    {
        vertex_shader = VSDefault(v);
        pixel_shader  = PSVisualizer(v);
    }
}
)";
