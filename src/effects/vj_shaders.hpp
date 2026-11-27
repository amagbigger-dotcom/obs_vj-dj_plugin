#pragma once
#include <string>
#include "effects/custom_shader_manager.hpp"

// =============================================================================
// VJ Visualizer Shader Suite (Direct3D 11 / OpenGL HLSL)
// 15 Beépített Mód + Dinamikus Egyedi ISF Shaderek
// =============================================================================

inline void invalidateVjVisualizerEffect() {
    CustomShaderManager::instance().incrementVersion();
}

inline const char *getVjVisualizerEffect() {
    static std::string effect_str;
    static uint32_t cached_version = 0;
    const uint32_t current_version = CustomShaderManager::instance().version();

    if (effect_str.empty() || cached_version != current_version) {
        cached_version = current_version;
        effect_str.clear();
        effect_str.reserve(65536);

        // ---------------------------------------------------------------------
        // Header & Uniform Definitions
        // ---------------------------------------------------------------------
        effect_str += R"(
uniform float4x4 ViewProj;
uniform texture2d dummy_texture;

uniform float visualizer_mode;      // 0..11
uniform float visualizer_mode_prev; // Előző VJ mód áttűnéshez
uniform float visualizer_trans_t;   // Áttűnési progressz [0.0 .. 1.0], 1.0 = nyugalmi állapot
uniform float anim_time;            // Másodperc alapú futásidő

#define TIME anim_time
#define time anim_time
#define iTime anim_time
#define shader_time anim_time
#define u_time anim_time
#define RENDERSIZE float2(1920.0, 1080.0)
#define iResolution float3(1920.0, 1080.0, 1.0)

uniform float bpm;                  // Deck BPM sebesség
uniform float audio_amp;            // RMS pillanatnyi hangerő [0.0 .. 1.0]
uniform float bass_energy;          // Lábdob / Mélyhang transient lüktetés [0.0 .. 1.0]
uniform float mid_energy;           // Középtartomány energia [0.0 .. 1.0]
uniform float high_energy;          // Magashang energia [0.0 .. 1.0]
uniform float audio_reactivity;     // Globális zenei reaktivitási / érzékenységi szorzó [0.0 .. 2.5]
uniform float is_deck_b;            // 0.0 = Deck A, 1.0 = Deck B
uniform float4 custom_theme_color;  // rgb = szín, a = felülírási kapcsoló (0.0 = alapértelmezett, 1.0 = egyedi)

float3 get_base_theme_color()
{
    float3 def_col = (is_deck_b > 0.5) ? float3(1.0, 0.46, 0.0) : float3(0.0, 0.85, 1.0);
    return lerp(def_col, custom_theme_color.rgb, saturate(custom_theme_color.a));
}

// Kifordított ISF Paraméterek (Effektív értékek a C++ oldali moduláció után)
uniform float p0;
uniform float p1;
uniform float p2;
uniform float p3;
uniform float p4;
uniform float p5;
uniform float p6;
uniform float p7;

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
// Matematikai segédfüggvények (Common Math Helpers - No Function Overloading)
// -----------------------------------------------------------------------------
float2 to_float2(float v) { return float2(v, v); }
float3 to_float3(float v) { return float3(v, v, v); }
float4 to_float4(float v) { return float4(v, v, v, v); }

float glsl_mod(float x, float y)
{
    return x - y * floor(x / y);
}

float2 glsl_mod2(float2 x, float2 y)
{
    return x - y * floor(x / y);
}

float2 glsl_mod2s(float2 x, float y)
{
    return x - y * floor(x / y);
}

float3 glsl_mod3(float3 x, float3 y)
{
    return x - y * floor(x / y);
}

float3 glsl_mod3s(float3 x, float y)
{
    return x - y * floor(x / y);
}

float4 glsl_mod4(float4 x, float4 y)
{
    return x - y * floor(x / y);
}

float4 glsl_mod4s(float4 x, float y)
{
    return x - y * floor(x / y);
}

float4 sample_procedural_noise(float2 p)
{
    float n1 = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
    float n2 = frac(sin(dot(p + float2(0.1, 0.1), float2(39.346, 11.135))) * 23421.631);
    float n3 = frac(sin(dot(p + float2(0.2, 0.3), float2(73.156, 52.235))) * 54321.123);
    return float4(n1, n2, n3, 1.0);
}

float2 rotate2D(float2 p, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(p.x * c - p.y * s, p.x * s + p.y * c);
}

float2 Rot2D(float2 p, float angle)
{
    return rotate2D(p, angle);
}

float2 rot2D(float2 p, float angle)
{
    return rotate2D(p, angle);
}

float3 rotate_vec(float3 v, float2 r)
{
    float cy = cos(r.y), sy = sin(r.y), cx = cos(r.x), sx = sin(r.x);
    float3x3 rxmat = float3x3(1.0, 0.0, 0.0,
                              0.0, cy, -sy,
                              0.0, sy, cy);
    float3x3 rymat = float3x3(cx, 0.0, -sx,
                              0.0, 1.0, 0.0,
                              sx, 0.0, cx);
    return mul(mul(v, rxmat), rymat);
}

float3 hsv2rgb(float h, float s, float v)
{
    return lerp(float3(1.0, 1.0, 1.0), clamp(abs(frac(h + float3(3.0, 2.0, 1.0) / 3.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * v;
}

float2 hash22(float2 p)
{
    float2 dot_val = float2(dot(p * float2(91.45, 243.21), float2(57.0, 27.0)),
                            dot(p * float2(91.45, 243.21), float2(27.0, 57.0)));
    return frac(sin(float2(262144.0, 32768.0)) * sin(dot_val));
}

float snoise_fluid(float3 v)
{
    const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);
    float4 D = float4(0.0, 0.5, 1.0, 2.0);

    float3 i = floor(v + dot(v, C.yyy));
    float3 x0 = v - i + dot(i, C.xxx);

    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    float3 x1 = x0 - i1 + C.xxx;
    float3 x2 = x0 - i2 + C.yyy;
    float3 x3 = x0 - D.yyy;

    i = glsl_mod3(i, 289.0);
    float4 p_val = glsl_mod4(((i.z + float4(0.0, i1.z, i2.z, 1.0)) * 34.0 + 1.0) * (i.z + float4(0.0, i1.z, i2.z, 1.0)), 289.0);
    p_val = glsl_mod4((p_val + (i.y + float4(0.0, i1.y, i2.y, 1.0)) * 34.0 + 1.0) * (p_val + (i.y + float4(0.0, i1.y, i2.y, 1.0))), 289.0);
    p_val = glsl_mod4((p_val + (i.x + float4(0.0, i1.x, i2.x, 1.0)) * 34.0 + 1.0) * (p_val + (i.x + float4(0.0, i1.x, i2.x, 1.0))), 289.0);

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

    float3 p0_v = float3(a0.xy, h.x);
    float3 p1_v = float3(a0.zw, h.y);
    float3 p2_v = float3(a1.xy, h.z);
    float3 p3_v = float3(a1.zw, h.w);

    float4 norm = rsqrt(float4(dot(p0_v, p0_v), dot(p1_v, p1_v), dot(p2_v, p2_v), dot(p3_v, p3_v)) + 0.00001);
    p0_v *= norm.x;
    p1_v *= norm.y;
    p2_v *= norm.z;
    p3_v *= norm.w;

    float4 m = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m * m, float4(dot(p0_v, x0), dot(p1_v, x1), dot(p2_v, x2), dot(p3_v, x3)));
}
)";

        // ---------------------------------------------------------------------
        // SHADER 0: FluidFlow 3D (Eredeti 3D Térfogati Simplex Folyadék)
        // ---------------------------------------------------------------------
        effect_str += R"(
float4 render_fluid_flow(float2 uv)
{
    float scale = max(p0, 0.2);
    float viscosity = max(p1, 0.05);
    float sharpness = saturate(p2);
    float density = max(p3, 0.1);
    float layers = clamp(p4, 2.0, 8.0);
    float glow = max(p5, 0.1);
    float color_mix = saturate(p6);
    float edge_rim = max(p7, 0.0);

    float2 p = uv - float2(0.5, 0.5);
    p.x *= 1.77777;
    p *= (1.0 / scale);

    float2 m = 0.5 * 3.14159265 * float2(2.0, 1.0);
    float3 pos = normalize(rotate_vec(float3(p, 0.5), m));

    float T = anim_time * 0.8;
    float dist = 0.0;
    float total_rim = 0.0;

    float edge_low = lerp(0.02, 0.30, sharpness) * (1.0 / density);
    float edge_high = lerp(0.40, edge_low + 0.04, sharpness);

    for (float i = 1.0; i <= layers; i += 1.0) {
        float3 flow_pos = pos * (i * viscosity) + float3(sin(T * 0.7 + i), T * 0.5, cos(T * 0.7 + i * 0.5)) * 0.25;
        float flow = abs(snoise_fluid(flow_pos));

        float body = smoothstep(edge_low, edge_high, flow);
        dist = max(dist, body * (1.0 - (i * (0.80 / layers))));

        float rim = smoothstep(edge_low - 0.03, edge_low, flow) * (1.0 - smoothstep(edge_low, edge_low + 0.05, flow));
        total_rim = max(total_rim, rim * (1.0 - (i * 0.1)));
    }

    float3 th_base = get_base_theme_color();
    float3 c_gold = float3(1.0, 0.85, 0.5);
    float3 c_fluid = lerp(th_base, c_gold, color_mix) * glow;

    float3 c_bg = float3(0.005, 0.005, 0.01);
    float3 col = lerp(c_bg, c_fluid, dist);

    col += (th_base * 1.5 + float3(0.2, 0.2, 0.2)) * (total_rim * edge_rim);
    return float4(saturate(col), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 1: TruchetSpin 3D (Eredeti 48-lépéses 3D Raymarching Labirintus)
        // ---------------------------------------------------------------------
        effect_str += R"(
float4 render_truchet(float2 uv)
{
    float zoom = max(p0, 0.2);
    float thickness = p1;
    float pulse = p2;
    float twist = p3;
    float roughness = saturate(p4);
    float glow = max(p5, 0.1);
    float fog_depth = max(p6, 0.1);
    float symmetry = saturate(p7);

    float T = anim_time * 1.5;
    float2 p_center = uv - float2(0.5, 0.5);
    p_center.x *= 1.7778;

    p_center = rotate2D(p_center, anim_time * 0.12 + twist * length(p_center));
    if (symmetry > 0.5) {
        p_center = abs(p_center);
    }

    float sc = (0.3 / max(0.1, zoom * 0.5)) * (1.0 - bass_energy * 0.18 * pulse);
    float2 oP = p_center * 10.0 + float2(0.5, T * 0.4);

    float2 d = float2(1e5, 1e5);
    float2 rndTh = float2(0.99, 1.0);
    float dim = sc;

    float pulse_term = sin(anim_time * 4.0) * 0.075 * (1.0 + bass_energy * 2.0 * pulse);
    float th = (thickness + roughness * 0.05) * 20.0 * (1.0 + audio_amp * 1.5);

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
            d.x -= 0.05 * th;
            d.x -= pulse_term;
            break;
        }
        dim *= 2.0;
    }

    float fo = (10.1 - 6.0) / (600.0 * fog_depth);
    float3 col = float3(0.01, 0.01, 0.02);
    col = lerp(col, float3(0.0, 0.0, 0.0), (1.0 - smoothstep(0.0, fo * 5.0, d.y - 0.1)) * 0.15);
    col = lerp(col, float3(1.0, 1.0, 1.0), (1.0 - smoothstep(0.0, fo, d.y)) * 0.15);

    fo = 21.0 / (600.0 * fog_depth) / sqrt(max(dim, 0.001));
    float sh = max(0.75 - d.x * 7.5, 0.0);
    sh *= clamp(-sin(d.x * 6.28318 * 7.5) + 1.0, 0.25, 1.0) + 0.0025;

    col = lerp(col, float3(0.0, 0.0, 0.0), 1.0 - smoothstep(0.0, fo, d.x));

    float3 th_base1 = get_base_theme_color();
    float3 c_fill = th_base1 * 0.65;
    float3 c_core = th_base1 * 1.15;

    c_core += float3(mid_energy * 0.3, high_energy * 0.3, high_energy * 0.3);

    col = lerp(col, c_fill * sh, 1.0 - smoothstep(0.0, fo, d.x + 0.21));
    col = lerp(col, c_core * sh, 1.0 - smoothstep(0.0, fo, abs(d.x + 0.12) - 0.02));
    float luma1 = dot(col, float3(0.299, 0.587, 0.114));
    col *= (1.0 + bass_energy * 0.45 * pulse);
    col += c_core * (bass_energy * 0.35 * pulse * luma1);
    col *= glow;

    return float4(saturate(sqrt(max(col, 0.0))), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 2: HypnoWorm (Eredeti 24-lépéses OpArt Féregalagút)
        // ---------------------------------------------------------------------
        effect_str += R"(
float4 render_hypno_worm(float2 uv)
{
    float size = p0;
    float rings = clamp(p1, 5.0, 40.0);
    float amp_x = p2;
    float amp_y = p3;
    float pulse = p4;
    float twist = p5;
    float edge_contrast = max(p6, 0.2);
    float glow = max(p7, 0.2);

    float T = anim_time * 1.5;

    float3 th_base2 = get_base_theme_color();
    float3 col1 = th_base2;
    float3 col2 = th_base2 * 0.75 + float3(0.15, 0.15, 0.15);
    col2 = lerp(col2, float3(1.0, 0.9, 0.4), mid_energy * 0.5);

    float3 fc = col1;
    float2 p = (uv - float2(0.5, 0.5)) * 2.0;
    p.x *= 1.7778;
    p = rotate2D(p, twist * length(p));
    p *= (11.0 - clamp(size, 1.0, 10.5));

    float a1 = amp_x * (1.0 + bass_energy * 0.45 * pulse);
    float a2 = amp_y * (1.0 + bass_energy * 0.45 * pulse);

    float2 pos = float2(0.0, 0.0);
    for (float i = 40.0; i > 0.0; i -= 1.0) {
        if (i > rings) continue;
        pos += float2(sin(T + (-0.5 - i) * -1.05) * a1,
                      cos(T + (1.0 - i) * 0.15) * a2);
        float dist = length(pos - p) - 0.075 * i;
        float o = 1.0 - smoothstep(-0.01, (20.0 / edge_contrast) / 1920.0, dist);
        fc = lerp(fc, float3(1.0, 1.0, 1.0) * (1.0 - glsl_mod(i, 2.0)), o);
    }

    float3 cc = col1.gbr * col2.rgb;
    float4 col = float4(pow(abs(col2.rgb), abs(fc.rgb)) - cc, 1.0);
    col = pow(abs(col), float4(1.0 / 0.9, 1.0 / 0.9, 1.0 / 0.9, 1.0));
    float luma2 = dot(col.rgb, float3(0.299, 0.587, 0.114));
    col.rgb *= (1.0 + bass_energy * 0.45 * pulse);
    col.rgb += col2 * (bass_energy * 0.3 * pulse * luma2);
    col.rgb *= glow;
    return saturate(col);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 3: ShapeTwist (Eredeti 18-lépéses Elektro Hullámcsavarás)
        // ---------------------------------------------------------------------
        effect_str += R"(
float electro_wave(float2 p_uv, float d, float f, float o, float a, float b_param, float time_sec)
{
    float theta = atan2(p_uv.y, p_uv.x);
    float amp = smoothstep(0.0, 1.0, (sin(theta + time_sec * 3.14159) * 0.5 + 0.5) - b_param) * a;
    float phase = d + sin(theta * f + o + time_sec * 3.14159) * amp;
    return sin(clamp(phase, 0.0, 6.28318) + 1.57079) + 1.0005;
}

float4 render_triangle_twist(float2 uv)
{
    float zoom = max(p0, 0.1);
    float amplitude = p1;
    float thickness = max(p2, 0.0005);
    float freq_val = max(p3, 0.5);
    float twist_param = p4;
    float pulse = p5;
    float glow = max(p6, 0.1);
    float decay = clamp(p7, 0.1, 2.0);

    float t_sec = anim_time * 0.4;

    float2 p = uv - float2(0.5, 0.5);
    p.x *= 1.7778;

    p *= (zoom * (1.0 + bass_energy * 0.15 * pulse));

    float grey = 0.0;
    float th = thickness + audio_amp * 0.003 * pulse;
    float amp_val = amplitude + bass_energy * 8.0 * pulse;
    float twist_step = twist_param + bass_energy * 0.01 * pulse;

    for (int i = 0; i < 18; ++i) {
        float root2 = 1.732;
        float d = dot(p, float2(0.0, -2.0));
        d = max(d, dot(p, float2(-root2, 1.0)));
        d = max(d, dot(p, float2(root2, 1.0)));

        grey += (1.0 - smoothstep(0.0, th, electro_wave(p, d * 10.0, freq_val, 0.0, amp_val, 0.0, t_sec))) * decay;
        grey += (1.0 - smoothstep(0.0, th, electro_wave(p, d * 10.0, freq_val, 1.57, amp_val, 0.0, t_sec))) * decay;
        grey += (1.0 - smoothstep(0.0, th, electro_wave(p, d * 10.0, freq_val, 3.14, amp_val, 0.0, t_sec))) * decay;

        p *= 1.08;
        p = rotate2D(p, twist_step * -6.28318);
    }

    float3 theme_col = get_base_theme_color();
    theme_col += float3(mid_energy * 0.2, high_energy * 0.3, high_energy * 0.3);

    float3 out_rgb = theme_col * grey * glow * (1.0 + bass_energy * 0.45 * pulse);
    return float4(saturate(out_rgb), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 4: StringArt (Geometrikus Szálművészet)
        // ---------------------------------------------------------------------
        effect_str += R"(
float4 render_string_art(float2 uv) {
    float scale = max(p0, 0.2);
    float lines = clamp(p1, 10.0, 40.0);
    float inner_r = p2;
    float outer_r = p3;
    float thickness = max(p4, 0.0002);
    float pulse = p5;
    float twist = p6;
    float glow = max(p7, 0.1);

    float t = anim_time * 0.6;
    float2 p = (uv - 0.5) * float2(1.77777, 1.0) * (scale * 2.0);

    float c = 0.0;
    for (float i = 0.0; i < 40.0; i += 1.0) {
        if (i > lines) break;
        float a1 = t * 0.5 + i * 0.157 + twist;
        float a2 = t * 1.25 + i * 0.314 + twist * 0.5;
        float2 pt1 = float2(cos(a1), sin(a1)) * (inner_r + 0.5 + bass_energy * 0.30 * pulse);
        float2 pt2 = float2(cos(a2), sin(a2)) * ((outer_r + 1.0) * (1.0 + bass_energy * 0.15 * pulse));
        float2 pa = p - pt1, ba = pt2 - pt1;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        float d = length(pa - ba * h);
        c += smoothstep(max(thickness * 6.0, 0.004), 0.0, d);
    }

    float3 theme = get_base_theme_color();
    float3 col = theme * c * 0.65;
    col += float3(high_energy * 0.35, mid_energy * 0.35, high_energy * 0.5) * c * 0.35;
    col *= (1.0 + bass_energy * 0.65 * pulse);
    col *= glow;
    return float4(saturate(col), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 5: Dodecahedron (3D Kristály Raymarching)
        // ---------------------------------------------------------------------
        effect_str += R"(
float map_dodecahedron(float3 p, float facet, float explode) {
    float3 q = abs(p);
    float phi = (1.0 + sqrt(5.0)) * 0.5;
    float d1 = dot(q, normalize(float3(1.0, phi, 0.0))) - facet - explode;
    float d2 = dot(q, normalize(float3(0.0, 1.0, phi))) - facet * 1.2 - explode;
    float d3 = dot(q, normalize(float3(phi, 0.0, 1.0))) - facet * 1.2 - explode;
    return max(max(d1, d2), d3);
}

float4 render_dodecahedron(float2 uv) {
    float zoom = max(p0, 0.2);
    float facet_cut = max(p1, 0.1);
    float explode = p2;
    float pulse = p3;
    float refraction = saturate(p4);
    float glow_val = max(p5, 0.1);
    float rot_tilt = p6;
    float dispersion = saturate(p7);

    float2 p = (uv - 0.5) * float2(1.77777, 1.0) * zoom * 2.0;
    float3 ro = float3(0.0, 0.0, -3.5);
    float3 rd = normalize(float3(p, 1.5));

    float rot_cur_y = anim_time * 0.6 + rot_tilt * 0.5;
    float rot_cur_x = anim_time * 0.35;
    float3 ro_rot = ro;
    ro_rot.xz = rotate2D(ro_rot.xz, rot_cur_y);
    ro_rot.yz = rotate2D(ro_rot.yz, rot_cur_x);
    rd.xz = rotate2D(rd.xz, rot_cur_y);
    rd.yz = rotate2D(rd.yz, rot_cur_x);

    float t = 0.0;
    float d = 0.0;
    float glow = 0.0;
    float pulse_d = bass_energy * 0.25 * pulse;
    for (int i = 0; i < 48; ++i) {
        float3 pos = ro_rot + rd * t;
        d = map_dodecahedron(pos, facet_cut + pulse_d, explode);
        glow += 0.01 / (0.05 + abs(d));
        if (d < 0.001 || t > 10.0) break;
        t += d * 0.8;
    }

    float3 theme = get_base_theme_color();
    float3 col = theme * (glow * 0.5 * glow_val);
    float luma5 = dot(col, float3(0.299, 0.587, 0.114));
    col += float3(high_energy * 0.4 * dispersion, mid_energy * 0.2, high_energy * 0.4) * luma5;
    col *= (1.0 + bass_energy * 0.5 * pulse);
    return float4(saturate(col), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 6: KaliCircuits (Fraktál Áramkörök)
        // ---------------------------------------------------------------------
        effect_str += R"(
float4 render_kali_circuits(float2 uv) {
    float zoom = max(p0, 0.1);
    float complexity = clamp(p1, 4.0, 18.0);
    float trace_width = max(p2, 0.05);
    float pulse = p3;
    float branching = p4;
    float glow = max(p5, 0.1);
    float invert = saturate(p6);
    float core_aura = max(p7, 0.1);

    float T = anim_time * 0.15;
    float2 pos = (uv - 0.5) * float2(1.77777, 1.0);
    float2 p = pos * (1.8 / zoom);

    p = rotate2D(p, T * 0.5);

    float S = 100.0 * core_aura;
    float3 color = float3(0.0, 0.0, 0.0);

    float2 z = p;
    float ot = 1000.0;
    float ot2 = 1000.0;
    float M = 0.0;
    int iter_steps = int(complexity);
    for (int i = 0; i < iter_steps; ++i) {
        z = abs(z) / clamp(dot(z, z), 0.1, 0.5) - float2(cos(T + i * branching), sin(T + i * branching)) * 0.15;
        float l = length(z);
        float o = min(max(abs(min(z.x, z.y)), -l + 0.25), abs(l - 0.25));
        ot = min(ot, o);
        ot2 = min(l * 0.1, ot2);
        M = max(M, float(i) * (1.0 - abs(sign(ot - o))));
    }
    M += 1.0;
    float w = trace_width * 0.1 * M * (1.0 + bass_energy * 0.3 * pulse);
    float circ = pow(max(0.0, (w - ot2) / max(0.0001, w)), 6.0);
    S += max(pow(max(0.0, (w - ot) / max(0.0001, w)), 0.25), circ);

    float3 theme = get_base_theme_color();
    float3 col = normalize(0.1 + float3(0.45, 0.75, M * 0.1));
    color += col * (0.4 + glsl_mod(M / 9.0 - T * 0.2 + ot2 * 2.0, 1.0));
    color += theme * circ * (10.0 - M) * 2.5 * core_aura;

    float3 bg = (invert > 0.5) ? float3(0.9, 0.9, 0.95) : float3(0.005, 0.005, 0.01);
    float3 out_rgb = lerp(bg, color * glow, clamp(S * 0.15, 0.0, 1.0));
    if (invert > 0.5) {
        out_rgb = 1.0 - out_rgb;
    }
    float luma6 = dot(out_rgb, float3(0.299, 0.587, 0.114));
    out_rgb *= (1.0 + bass_energy * 0.45 * pulse);
    out_rgb += float3(high_energy * 0.2, mid_energy * 0.2, high_energy * 0.3) * luma6;
    return float4(saturate(out_rgb), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 7: DiamondVision (36-Lépéses Tórusz Sugárkövetés)
        // ---------------------------------------------------------------------
        effect_str += R"(
float torus_dv(float3 pos, float2 tor) {
    float2 qt = abs(float2(max(abs(pos.x), abs(pos.z)) - tor.x, pos.y));
    return max(qt.x, qt.y) - tor.y;
}

float trap_dv(float3 tp, float r_size, float th) {
    return abs(min(torus_dv(tp, float2(r_size * 0.865, th * 0.5)), max(abs(tp.z) - 0.125, abs(tp.x) - 0.125))) - 0.005;
}

float map_dv(float3 pm, float r_size, float th, float twist, float time2) {
    float c = dot(abs(pm.yz), float2(0.5, 0.5)) - 0.05;
    float3 m = abs(1.0 - glsl_mod3(pm, 2.0));
    m.yz = rotate2D(m.yz, sqrt(time2));
    float e = 9999.0, f = 1.0;
    for (float i = 0.0; i < 4.0; i += 1.0) {
        m.xz = rotate2D(m.xz, (i * 0.333 * twist + time2) * 0.01745);
        m.zy = rotate2D(m.zy, ((i + i) * 0.667 * twist + time2 * 1.618) * 0.01745);
        m = abs(1.0 - glsl_mod3(m + i / 3.0, 2.0));
        m *= abs(sqrt(m) * 1.77245);
        f *= 0.5;
        e = min(e, trap_dv(m, r_size, th) * f);
    }
    return max(e, -c);
}

float4 render_diamond_vision(float2 uv) {
    float zoom = max(p0, 0.2);
    float torus_size = max(p1, 0.1);
    float thickness = max(p2, 0.02);
    float twist = p3;
    float pulse = p4;
    float specular = max(p5, 0.1);
    float glow = max(p6, 0.1);
    float hue_shift = p7;

    float time2 = anim_time * 1.5 + 50.0;
    float2 ps = (uv - 0.5) * float2(1.77777, 1.0) * (2.0 / zoom);

    float3 co = float3(time2 * 0.05, 0.0, 0.0);
    float3 rd = normalize(float3(ps.x, ps.y, 1.0 - length(ps) * 0.5));

    float r_eff = torus_size * (1.0 + bass_energy * 0.25 * pulse);
    float th_eff = thickness * (1.0 + mid_energy * 0.20 * pulse);

    float d = 1.0, it = 0.0;
    float3 p_ray = co;
    float md = 1.618 + sin(time2 * 0.5) * 0.25;

    for (int i = 0; i < 36; ++i) {
        if (d < 0.001) break;
        d = map_dv(p_ray, r_eff, th_eff, twist, time2);
        p_ray += max(d, 0.005) * rd;
        md = min(md, d);
        it += 1.0;
    }

    float3 col = float3(0.0, 0.0, 0.0);
    if (d < 0.002) {
        float q = 1.0 - (it / 35.0);
        col = hsv2rgb(glsl_mod(q * 0.2 + 0.5 + hue_shift, 1.0), 1.0 - q * 0.865, q * specular);
    }
    col += hsv2rgb(glsl_mod(d + hue_shift, 1.0), 1.0, 1.0) * md * 20.0 * specular;

    float3 theme = get_base_theme_color();
    col = lerp(col, col * theme, 0.6);
    col *= (1.0 + bass_energy * 0.5 * pulse);
    col *= glow;
    return float4(saturate(col), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 8: Pupila (Uará Lab / Shadertoy wtX3Dl Organikus Szem & Írisz)
        // ---------------------------------------------------------------------
        effect_str += R"(
float hash_pupila(float2 uv)
{
    return frac(sin(dot(uv, float2(20.974, 80.2171))) * 300.963);
}

float noise_pupila(float2 uv)
{
    float2 f = smoothstep(0.0, 1.0, frac(uv));
    float2 uv00 = floor(uv);
    float2 uv01 = uv00 + float2(0.0, 1.0);
    float2 uv10 = uv00 + float2(1.0, 0.0);
    float2 uv11 = uv00 + 1.0;

    float v00 = hash_pupila(uv00);
    float v01 = hash_pupila(uv01);
    float v10 = hash_pupila(uv10);
    float v11 = hash_pupila(uv11);

    float v0 = lerp(v00, v01, f.y);
    float v1 = lerp(v10, v11, f.y);
    return lerp(v0, v1, f.x);
}

float radialNoise_pupila(float2 uv, float dist, float noiseScale)
{
    float2 polarUV = float2((atan2(uv.y, uv.x) + 3.14159265) / (2.0 * 3.14159265), dist);
    polarUV.x *= noiseScale;
    if (polarUV.x + 0.5 > noiseScale) polarUV.x -= noiseScale;
    polarUV.x += hash_pupila(float2(polarUV.y, 2.0)) * 834.1;
    return noise_pupila(polarUV);
}

float irisNoise_pupila(float2 uv, float len, float deltaDist, float densityMultiplier, float distanceStart)
{
    float dist = len + (radialNoise_pupila(uv, deltaDist, 30.0) - 0.5) * 0.1 + distanceStart;
    float lerpVal = frac(dist / deltaDist);

    float firstIndex = floor(dist / deltaDist);
    float secondIndex = firstIndex + 1.0;

    float firstDist = firstIndex * deltaDist;
    float secondDist = secondIndex * deltaDist;

    float firstDensity = firstIndex * densityMultiplier;
    float secondDensity = secondIndex * densityMultiplier;

    float firstNoise = radialNoise_pupila(uv, firstDist, firstDensity);
    float secondNoise = radialNoise_pupila(uv, secondDist, secondDensity);
    return lerp(firstNoise, secondNoise, lerpVal);
}

float4 render_pupila(float2 uv)
{
    float zoom = max(p0, 0.1);
    float pupil_size = clamp(p1, 0.02, 0.7);
    float pupil_pulse = p2;
    float iris_fibers = clamp(p3, 2.0, 25.0);
    float border_width = clamp(p4, 0.02, 0.6);
    float reflection_val = max(p5, 0.0);
    float iris_blend = max(p6, 0.1);
    float glow = max(p7, 0.1);

    float2 p = (uv - 0.5) * 2.0;
    p.x *= 1.77777;
    p *= (zoom * 2.0);

    float dist = length(p);

    // Zenei lábdobra dinamikusan kitáguló pupilla
    float beat_mod = bass_energy * 0.40 * pupil_pulse;
    float pupilModifier = (1.0 - sqrt(noise_pupila(float2(anim_time * 0.5, 2.1))) * 0.5);
    pupilModifier = clamp(pupilModifier + beat_mod, 0.15, 1.5);

    float IRIS_RADIUS = 0.45;
    float PUPIL_RADIUS = pupil_size * 0.5;
    float ALIASING = 0.01;

    float vDist = (-IRIS_RADIUS + dist) * pupilModifier;

    float pupil = 1.0 - smoothstep(PUPIL_RADIUS, PUPIL_RADIUS + ALIASING * pupilModifier, vDist + PUPIL_RADIUS * 2.0);
    float pupil2 = 1.0 - smoothstep(PUPIL_RADIUS * 1.05, PUPIL_RADIUS * 1.05 + ALIASING * 4.0 * pupilModifier, vDist + PUPIL_RADIUS * 2.0);

    float fiber_scale = iris_fibers * 7.5;
    float iris = irisNoise_pupila(p, vDist, 0.015, fiber_scale, 0.15);
    float irisCenter = irisNoise_pupila(p, vDist, 0.015, fiber_scale * 2.0, 0.15);
    float irisDetails = irisNoise_pupila(p, vDist, 0.016, fiber_scale * 2.5, 0.14);

    float irisBorder = 1.0 - smoothstep(0.0, border_width, abs(dist - IRIS_RADIUS));
    float irisWhiteBorder = 1.0 - smoothstep(0.0, border_width * 1.1 * (1.0 - pow(pupilModifier, 1.4)), abs(dist - IRIS_RADIUS + 0.001));
    float whiteEye = smoothstep(IRIS_RADIUS, IRIS_RADIUS + ALIASING * 3.0, dist);

    float3 th_iris = get_base_theme_color();
    float3 custom_border = th_iris * 0.6 + float3(0.1, 0.1, 0.15);

    float3 eye = smoothstep(-0.5, 1.0, iris) * th_iris * iris_blend;
    eye = lerp(eye, eye * irisDetails * irisDetails + irisWhiteBorder * 0.7 * custom_border, 0.3);
    eye = lerp(eye, eye * irisCenter * 0.3, pupil2);
    eye = lerp(eye, float3(0.0, 0.0, 0.0), pupil);
    eye = lerp(eye, float3(0.95, 0.96, 0.98), whiteEye);
    eye = lerp(eye, float3(irisBorder, irisBorder, irisBorder), 0.4);

    eye = smoothstep(0.1, 1.0, eye);

    // Tükröződés / Csillogás (Specular reflection)
    float refl = smoothstep(0.0, 0.088, 0.05 - distance(p, float2(0.16, 0.22))) * 1.2 * reflection_val;

    float3 final_col = (eye + refl) * glow;
    float luma8 = dot(final_col, float3(0.299, 0.587, 0.114));
    final_col += th_iris * (bass_energy * 0.35 * pupil_pulse * luma8);
    final_col *= (1.0 + bass_energy * 0.25 * pupil_pulse);
    return float4(saturate(final_col), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 9: MandalaScope (mojovideotech 72-lépéses Fraktál Kaleidoszkóp)
        // ---------------------------------------------------------------------
        effect_str += R"(
float2 Kscope_mandala(float2 uv, float k, float T)
{
    float angle = abs(glsl_mod(atan2(uv.y, uv.x), 2.0 * k) - k) + 0.1 * T;
    return length(uv) * float2(cos(angle), sin(angle));
}

float4 render_mandala_scope(float2 uv)
{
    float scale = max(p0, 0.2);
    float sides = clamp(p1, 2.0, 24.0);
    float iterations = clamp(p2, 6.0, 48.0);
    float pulse = p3;
    float petal_spread = p4;
    float glow = max(p5, 0.1);
    float contrast = max(p6, 0.2);
    float rot_spd = p7;

    float T = anim_time * 0.4;
    float2 p_uv = (5.5 - scale) * ((uv - 0.5) * 2.0);
    p_uv.x *= 1.77777;

    p_uv = rotate2D(p_uv, anim_time * rot_spd * 0.3);

    float k = 3.14159265 / floor(max(2.0, sides));
    float2 s = Kscope_mandala(p_uv, k, T);
    float2 t = Kscope_mandala(s, k, T);
    float v = dot(t, s);
    float2 u = lerp(s, t, cos(v) * (1.0 + bass_energy * 0.2 * pulse));

    if (petal_spread > 0.001) {
        float m = floor(petal_spread * 2.0 + 1.0);
        u.yx += glsl_mod2(glsl_mod2(u.yx, m), -u.xy);
    }

    float3 p = float3(u, 0.3 * v);
    float rcpi = 0.318309886;
    int max_loops = (int)iterations;

    for (int i = 0; i < 48; ++i) {
        if (i > max_loops) break;
        p.xzy = float3(1.3, 0.999, 0.678) * abs(abs(p) / max(0.0001, dot(p, p)) - float3(1.0, 1.02, -0.75 * rcpi));
    }

    float3 theme = get_base_theme_color();
    float3 out_rgb = lerp(p, p * theme, 0.65);
    out_rgb = pow(abs(out_rgb), float3(contrast, contrast, contrast));
    float luma9 = dot(out_rgb, float3(0.299, 0.587, 0.114));
    out_rgb *= (1.0 + bass_energy * 0.45 * pulse);
    out_rgb += float3(high_energy * 0.3, mid_energy * 0.2, high_energy * 0.4) * luma9;
    out_rgb *= glow;

    return float4(saturate(out_rgb), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 10: SearchingLight (Kiber Fényalagút & Fénysugarak)
        // ---------------------------------------------------------------------
        effect_str += R"(
float4 render_searching_light(float2 uv)
{
    float tunnel_w = max(p0, 0.1);
    float ray_count = clamp(p1, 2.0, 16.0);
    float ray_sharpness = clamp(p2, 0.0005, 0.02);
    float pulse = p3;
    float glow = max(p4, 0.1);
    float twist = p5;
    float speed_z = p6;
    float depth_fade = max(p7, 0.1);

    float4 c = float4(0.0, 0.0, 0.0, 0.0);
    float l = 0.0;
    float z = anim_time * speed_z;

    for (int i = 0; i < 4; ++i) {
        float2 p = uv - float2(0.5, 0.5);
        p.x *= 1.77777;
        p = rotate2D(p, anim_time * 0.3 + twist * length(p));
        z += 0.7;
        l = length(p);
        float angle = atan2(p.y, p.x);
        float ray_mod = sin(angle * ray_count + z);
        float2 uv_mod = uv + p / max(0.0001, l) * (cos(z) + 0.7) * (ray_mod + 0.7);
        float val_i = ray_sharpness / max(0.0001, length(abs(glsl_mod2(uv_mod, tunnel_w) - tunnel_w * 0.5)));
        if (i == 0) c.x = val_i;
        else if (i == 1) c.y = val_i;
        else if (i == 2) c.z = val_i;
        else c.w = val_i;
    }

    float3 theme = get_base_theme_color();
    float3 out_rgb = (c.rgb / max(0.0001, pow(l, depth_fade))) * glow * theme;
    float luma10 = dot(out_rgb, float3(0.299, 0.587, 0.114));
    out_rgb *= (1.0 + bass_energy * 0.50 * pulse);
    out_rgb += float3(high_energy * 0.3, mid_energy * 0.2, high_energy * 0.4) * luma10;

    return float4(saturate(out_rgb), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 11: CubeField 3D (sheltron3030 3D Kockamező Raymarching)
        // ---------------------------------------------------------------------
        effect_str += R"(
float3x3 rotationMatrix_cf(float3 axis, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return float3x3(
        oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
        oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
        oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c
    );
}

float3 opRep_cf(float3 p, float3 c)
{
    return glsl_mod3(p, c.x) - 0.5 * c;
}

float sdBox_cf(float3 p, float3 b)
{
    float3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float2 DE_cf(float3 pos, float rot_z, float squiggle, float dist1, float repeat, float box_size)
{
    pos = mul(pos, rotationMatrix_cf(normalize(float3(0.0, 0.0, 1.0)), rot_z + anim_time * 0.1 + squiggle * sin(pos.z)));
    pos.z = pos.z + 0.3 * dist1 * sin(pos.y * 10.0);
    pos.y = pos.y + 0.5 * sin(pos.y * 10.0);
    pos = opRep_cf(pos, float3(repeat, repeat, repeat));

    float a = sdBox_cf(pos, float3(box_size, box_size, box_size));
    return float2(pos.z, a);
}

float3 gradient_cf(float3 p, float t, float rot_z, float squiggle, float dist1, float repeat, float box_size)
{
    float2 e = float2(0.0, t);
    return normalize(float3(
        DE_cf(p + e.yxx, rot_z, squiggle, dist1, repeat, box_size).y - DE_cf(p - e.yxx, rot_z, squiggle, dist1, repeat, box_size).y,
        DE_cf(p + e.xyx, rot_z, squiggle, dist1, repeat, box_size).y - DE_cf(p - e.xyx, rot_z, squiggle, dist1, repeat, box_size).y,
        DE_cf(p + e.xxy, rot_z, squiggle, dist1, repeat, box_size).y - DE_cf(p - e.xxy, rot_z, squiggle, dist1, repeat, box_size).y
    ));
}

float3 palette_cf(float t)
{
    float3 a = float3(0.5, 0.5, 0.7);
    float3 b = float3(1.0, 0.0, 1.0);
    float3 c = float3(1.0, 0.9, 1.0);
    float3 d = float3(0.0, 0.6, 0.3);
    return a + b * cos(6.28318 * (c * t + d));
}

float4 render_cube_field(float2 uv)
{
    float spacing = max(p0, 0.3);
    float box_size = clamp(p1, 0.02, 0.45);
    float height_wave = p2;
    float pulse = p3;
    float edge_glow = max(p4, 0.1);
    float camera_height = p5;
    float fog_dist = max(p6, 1.0);
    float twist = p7;

    float3 camera = float3(0.0, camera_height, anim_time * 1.5);
    float2 screenPos = -1.0 + 2.0 * uv;
    screenPos.x *= 1.77777;

    float3 ray = normalize(float3(screenPos, 1.0));
    float t = 0.0;
    float3 point_p = camera;
    int iter = 0;
    bool hit = false;
    float2 dist = float2(0.0, 0.0);
    float box_eff = box_size * (1.0 + bass_energy * 0.25 * pulse);

    for (int i = 0; i < 36; ++i) {
        point_p = camera + ray * t;
        dist = DE_cf(point_p, twist, 0.5, height_wave, spacing, box_eff);
        float thresh = exp(-fog_dist) * exp(t * 0.25);
        if (abs(dist.y) < thresh) {
            hit = true;
            break;
        }
        t += max(dist.y * 0.5, 0.01);
        iter++;
    }

    float3 color = float3(0.0, 0.0, 0.0);
    if (hit) {
        float shade = dot(gradient_cf(point_p, 0.01, twist, 0.5, height_wave, spacing, box_eff), -ray);
        float ao = 1.0 - float(iter) / 36.0;
        color = palette_cf(point_p.z * 0.2) * sqrt(max(0.0, ao)) * max(0.2, shade) * edge_glow;
    }

    float3 theme = get_base_theme_color();
    color = lerp(color, color * theme, 0.5);
    float luma11 = dot(color, float3(0.299, 0.587, 0.114));
    color *= (1.0 + bass_energy * 0.50 * pulse);
    color += theme * (bass_energy * 0.35 * luma11 * pulse);
    return float4(saturate(color), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 12: Electro Bands (echophons)
        // ---------------------------------------------------------------------
        effect_str += R"(
float electro_bands_func(float2 pos, float d, float f, float o, float a, float b, float sz, float time_val)
{
    float ti = time_val * 0.5;
    float theta = atan2(pos.y, pos.x);
    float amp_val = smoothstep(0.0, 1.0, (sin(theta + time_val * 2.0) * 0.5 + 0.5) - b) * a;
    float phase = d - sin(theta * f + o + ti * 1.5) * amp_val * 2.5;
    return sin(clamp(phase, 0.0, 3.14159265 * sz) + 3.14159265 / max(sz, 0.001)) + 0.9999999;
}

float electro_circle(float2 pos, float r, float a, float o, float f, float b, float t, float sz, float time_val)
{
    float d = length(pos);
    return 1.0 - smoothstep(0.0, max(t, 0.0001), electro_bands_func(pos, d / max(r, 0.0001), f, o, a, b, sz, time_val));
}

float4 render_electro_bands(float2 uv)
{
    float radius = max(p0, 0.002);
    float bands_count = clamp(p1, 2.0, 20.0);
    float amplitude = p2;
    float thickness = max(p3, 0.05);
    float pulse = p4;
    float glow = max(p5, 0.1);
    float twist = p6;
    float color_split = saturate(p7);

    float2 pos = uv * 2.0 - 1.0;
    pos.x *= 1.77777;
    pos -= float2(0.0, -0.2);
    pos = rotate2D(pos, twist * length(pos));

    float amp_eff = amplitude * (1.0 + bass_energy * 0.40 * pulse);
    float r_eff = radius * (1.0 + bass_energy * 0.30 * pulse);
    float sz = 40.0;
    float band = 0.5;

    float3 color = float3(0.0, 0.0, 0.0);
    color.r = electro_circle(pos, r_eff, amp_eff, 0.0 * 3.14159265 * color_split, bands_count, band, thickness * 0.01, sz, anim_time);
    color.g = electro_circle(pos, r_eff, amp_eff, 0.3 * 3.14159265 * color_split, bands_count, band, thickness * 0.01, sz, anim_time);
    color.b = electro_circle(pos, r_eff, amp_eff, 0.6 * 3.14159265 * color_split, bands_count, band, thickness * 0.01, sz, anim_time);

    float3 theme = get_base_theme_color();
    color = lerp(color, color * theme, 0.5);
    color *= (1.0 + bass_energy * 0.60 * pulse);
    color *= glow;
    return float4(saturate(color), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 13: GlobularCluster (mojovideotech)
        // ---------------------------------------------------------------------
        effect_str += R"(
float map_gc(float3 p, float m_time, float warpx, float warpy, float warpz, float melt, float tilt, float horizon, float freq)
{
    float3 q = glsl_mod3(p + freq, 4.0) - 2.0;
    float d1 = length(q) - 1.0;
    d1 += 0.1 * sin(warpx * p.x) * sin(warpy * p.y + 1.0 * m_time) * sin(warpz * p.z);
    float d2 = lerp(p.x, p.y, tilt) + horizon;
    float h = saturate(0.5 + 0.5 * (d1 - d2) / max(melt, 0.001));
    return lerp(d1, d2, h) - melt * h * (1.0 - h);
}

float4 render_globular_cluster(float2 uv)
{
    float zoom = max(p0, 0.1);
    float cluster_density = clamp(p1, 0.5, 20.0);
    float warp_val = max(p2, 0.1);
    float melt = max(p3, 0.1);
    float pulse = p4;
    float glow = max(p5, 0.1);
    float core_radius = max(p6, 0.1);
    float contrast_val = max(p7, 0.1);

    float m_time = anim_time * 0.6;
    float2 p = (-1.0 + 2.0 * uv) * (1.5 / zoom);
    p.x *= 1.77777;

    float3 r0 = float3(5.0, 0.425, 39.5);
    float3 rd = normalize(float3(p, -1.0));
    float3 col = float3(1.0, 1.0, 1.0);
    float tmax = 80.0;
    float h = 1.0;
    float t = 0.0;

    for (int i = 0; i < 64; ++i) {
        if (h < 0.0005 || t > tmax) break;
        h = map_gc(r0 + t * rd, m_time, warp_val, warp_val * 0.75, warp_val * 0.5, melt, 0.5, 4.0, 5.5);
        t += max(h, 0.01);
    }

    float f = 0.0;
    if (t < tmax) {
        float3 pos = r0 + t * rd;
        float s = pos.x + 0.5 * pos.y + 3.0 * pos.z;
        f = pos.z;
        float band = glsl_mod(abs(floor(s * cluster_density * (1.0 + bass_energy * 0.2 * pulse))), 1.33);
        col = float3(band, band, band);
    }

    float3 base_color = float3(0.8, 0.15 * f / 5.0, 0.8) - col * contrast_val;
    float3 theme = get_base_theme_color();
    float3 final_col = lerp(base_color, base_color * theme, 0.6);
    float luma13 = dot(final_col, float3(0.299, 0.587, 0.114));
    final_col *= (1.0 + bass_energy * 0.55 * pulse);
    final_col += theme * (bass_energy * 0.35 * luma13 * core_radius);
    final_col *= glow;
    return float4(saturate(final_col), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // SHADER 14: HexTruchWarpFlow2 (mojovideotech)
        // ---------------------------------------------------------------------
        effect_str += R"(
float2 PixToHex_hlsl(float2 p)
{
    const float s3 = 1.7320508;
    float cx = (1.0 / s3) * p.x - (1.0 / 3.0) * p.y;
    float cz = (2.0 / 3.0) * p.y;
    float cy = -cx - cz;
    float3 c = float3(cx, cy, cz);
    float3 r = floor(c + 0.5);
    float3 dr = abs(r - c);
    r -= step(dr.yzx, dr) * step(dr.zxy, dr) * dot(r, float3(1.0, 1.0, 1.0));
    return r.xz;
}

float2 HexToPix_hlsl(float2 h)
{
    const float s3 = 1.7320508;
    return float2(s3 * (h.x + 0.5 * h.y), 1.5 * h.y);
}

float HexEdgeDist_hlsl(float2 p)
{
    const float s3 = 1.7320508;
    const float hs3 = 0.8660254;
    p = abs(p);
    return hs3 - p.x + 0.5 * min(p.x - s3 * p.y, 0.0);
}

float2 Rot2D_hlsl(float2 q, float a)
{
    float2 cs = sin(a + float2(1.5707963, 0.0));
    return float2(dot(q, float2(cs.x, -cs.y)), dot(q.yx, cs));
}

float Hashfv2_hlsl(float2 p)
{
    return frac(sin(dot(p, float2(37.0, 39.0))) * 14449.97975);
}

float3 ShowScene_hlsl(float2 p, float T, float thickness, float arrows, float dashes, float3 c1, float3 c2, float3 c3)
{
    const float hs3 = 0.8660254;
    float3 col = float3(0.0, 0.0, 0.0);
    float3 w = float3(0.0, 0.0, 0.0);
    float2 cId = PixToHex_hlsl(p);
    float2 pc = HexToPix_hlsl(cId);
    float dir = 2.0 * step(Hashfv2_hlsl(cId), 0.5) - 1.0;
    w.xy = pc + float2(0.0, -dir);
    w.z = dot(w.xy - p, w.xy - p);
    float2 q = pc + float2(hs3, 0.5 * dir);
    float d = dot(q - p, q - p);
    if (d < w.z) w = float3(q, d);
    q = pc + float2(-hs3, 0.5 * dir);
    d = dot(q - p, q - p);
    if (d < w.z) w = float3(q, d);
    w.z = abs(sqrt(max(w.z, 0.0)) - 0.5);
    d = HexEdgeDist_hlsl(p - pc);
    float o = thickness * 0.1;
    col = c1 * lerp(1.0, 0.7 + 0.3 * smoothstep(0.2, 0.8, d), smoothstep(0.02, 0.03, d));
    if (w.z < thickness - o) {
        col = c2 * (1.0 - 0.5 * smoothstep(0.05, thickness - o, w.z));
        w.xy = Rot2D_hlsl(w.xy - p, 0.5 * dir * abs(T));
        float a = glsl_mod(3.0 * atan2(dir * w.y, -w.x) / 3.14159265, 1.0) - 0.5;
        float s = 0.01;
        float d_iter = 1.0;
        if (abs(glsl_mod(2.0 * a + 0.5, 1.0) - 0.5) - 0.33 < s)
            d_iter = lerp(d_iter, min(d_iter, smoothstep(-0.025 * o, 0.25 * o, abs(w.z - clamp(0.5 * thickness + o, 0.025, 0.33)) - 0.01 - s)), dashes);
        if (abs(a) - 0.15 < s)
            d_iter = lerp(d_iter, min(d_iter, smoothstep(-0.05 + o, 0.5 * o, w.z - min(0.02, o) * (1.0 - a / 0.05) - 0.05 * s)), arrows);
        if (abs(a + 0.42) - 0.25 < s)
            d_iter = lerp(d_iter, min(d_iter, smoothstep(-0.05 + o, 0.5 * o, w.z - min(0.02, o) - s)), arrows);
        col = lerp(c3, col - c3 * s, d_iter);
    }
    return col;
}

float4 render_hex_truch_warp(float2 uv)
{
    float scale = max(p0, 0.2);
    float thickness = clamp(p1, 0.005, 0.3);
    float warp = p2;
    float pulse = p3;
    float dashes = saturate(p4);
    float arrows = saturate(p5);
    float glow = max(p6, 0.1);
    float motion_drift = p7;

    float T = anim_time * 0.6;
    float2 uvn = 2.0 * uv - 1.0;
    uvn.x *= 1.77777;
    float2 m = float2(cos(0.05 * anim_time), sin(0.05 * anim_time));
    uvn += m * (motion_drift * 0.2);
    float2 tc = uvn * (10.0 / scale);

    float3 c1 = float3(0.2431, 0.0235, 0.2274);
    float3 c2 = float3(0.1843, 0.1294, 0.5451);
    float3 c3 = float3(0.5176, 0.7608, 0.1451);
    float3 theme = get_base_theme_color();
    c2 = lerp(c2, theme, 0.5);

    float th_eff = thickness * (1.0 + bass_energy * 0.40 * pulse);
    float2 rot_warp = Rot2D_hlsl(float2(0.5 / 1080.0, length(m) / scale), length(uvn) * 3.14159265) * warp;
    float3 col = ShowScene_hlsl(tc + rot_warp, T, th_eff, arrows, dashes, c1, c2, c3);
    col *= (col + float3(1.75, 1.75, 1.75)) * 0.75;
    float luma14 = dot(col, float3(0.299, 0.587, 0.114));
    col *= (1.0 + bass_energy * pulse * 0.55);
    col += theme * (bass_energy * pulse * 0.35 * luma14);
    col *= glow;
    return float4(saturate(col), 1.0);
}
)";

        // ---------------------------------------------------------------------
        // Dinamikus Felhasználói Egyedi ISF Shaderek Függvényei
        // ---------------------------------------------------------------------
        effect_str += CustomShaderManager::instance().getCustomShadersHlslDeclarations();

        // ---------------------------------------------------------------------
        // Pixel Shader Dispatcher & Technique (BPM-Szinkronizált Áttűnés Támogatással)
        // ---------------------------------------------------------------------
        effect_str += R"(
float4 sample_visualizer_mode(float mode, float2 uv)
{
    if (mode < 0.5) {
        return render_fluid_flow(uv);
    } else if (mode < 1.5) {
        return render_truchet(uv);
    } else if (mode < 2.5) {
        return render_hypno_worm(uv);
    } else if (mode < 3.5) {
        return render_triangle_twist(uv);
    } else if (mode < 4.5) {
        return render_string_art(uv);
    } else if (mode < 5.5) {
        return render_dodecahedron(uv);
    } else if (mode < 6.5) {
        return render_kali_circuits(uv);
    } else if (mode < 7.5) {
        return render_diamond_vision(uv);
    } else if (mode < 8.5) {
        return render_pupila(uv);
    } else if (mode < 9.5) {
        return render_mandala_scope(uv);
    } else if (mode < 10.5) {
        return render_searching_light(uv);
    } else if (mode < 11.5) {
        return render_cube_field(uv);
    } else if (mode < 12.5) {
        return render_electro_bands(uv);
    } else if (mode < 13.5) {
        return render_globular_cluster(uv);
    } else if (mode < 14.5) {
        return render_hex_truch_warp(uv);
)";
        effect_str += CustomShaderManager::instance().getCustomShadersDispatchHlsl();
        effect_str += R"(
    } else {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
}

float4 PSVisualizer(VertData v) : TARGET
{
    float4 col_curr = sample_visualizer_mode(visualizer_mode, v.uv);
    [branch]
    if (visualizer_trans_t < 0.999) {
        float4 col_prev = sample_visualizer_mode(visualizer_mode_prev, v.uv);
        return lerp(col_prev, col_curr, smoothstep(0.0, 1.0, visualizer_trans_t));
    }
    return col_curr;
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
    }
    return effect_str.c_str();
}

#define VJ_VISUALIZER_EFFECT getVjVisualizerEffect()
