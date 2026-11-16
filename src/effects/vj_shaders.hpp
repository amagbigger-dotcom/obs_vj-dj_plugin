#pragma once
#include <string>

// =============================================================================
// VJ Visualizer Shader Suite (Direct3D 11 / OpenGL HLSL)
// 8 Választható Mód:
//   0: FluidFlowSNoise (mojovideotech folyadék áramlás - Default)
//   1: TruchetDualLevelSpin (mojovideotech kettős szintű forgó labirintus)
//   2: HypnoWorm (mojovideotech 30-lépéses hipnotikus féregalagút)
//   3: TriangleSquareTwist (Joseph Fiola / xLights elektro hullám csavarás)
//   4: StringArt (mojovideotech geometrikus szálművészet)
//   5: Dodecahedron (OLAV / gltracy 3D raymarching kristály)
//   6: KaliCircuitsExplorer (mojovideotech fraktál áramkörök)
//   7: DiamondVision (Axiom-Crux / mojovideotech tórusz sugárkövetés)
// =============================================================================

inline const char *getVjVisualizerEffect() {
    static std::string effect_str;
    if (effect_str.empty()) {
        effect_str.reserve(32768);
        effect_str += R"(
uniform float4x4 ViewProj;
uniform texture2d dummy_texture;

uniform float visualizer_mode;      // 0..7
uniform float anim_time;            // Másodperc alapú futásidő
uniform float bpm;                  // Deck BPM sebesség
uniform float audio_amp;            // RMS pillanatnyi hangerő [0.0 .. 1.0]
uniform float bass_energy;          // Lábdob / Mélyhang transient lüktetés [0.0 .. 1.0]
uniform float mid_energy;           // Középtartomány energia [0.0 .. 1.0]
uniform float high_energy;          // Magashang energia [0.0 .. 1.0]
uniform float is_deck_b;            // 0.0 = Deck A, 1.0 = Deck B

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

float3 hsv2rgb(float h, float s, float v)
{
    return lerp(float3(1.0, 1.0, 1.0), clamp(abs(frac(h + float3(3.0, 2.0, 1.0) / 3.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * v;
}

// -----------------------------------------------------------------------------
// SHADER 0: FluidFlowSNoise (Organikus Folyadék Áramlás)
// -----------------------------------------------------------------------------
float snoise_fluid(float3 v, float d_param, float b_param)
{
    const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);
    const float4 D = float4(0.0, 0.5, 1.0, 2.0);

    float3 i = floor(v + dot(v, C.yyy));
    float3 x0 = v - i + dot(i, C.xxx);

    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    float3 x1 = x0 - i1 + C.xxx;
    float3 x2 = x0 - i2 + C.yyy;
    float3 x3 = x0 - D.yyy;

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

float4 render_fluid_flow(float2 uv)
{
    float rate = p0;
    float zrate = p1;
    float depth = p2;
    float blur = p3;
    float freq = p4;

    float t_flow = anim_time * rate;
    float t_z = anim_time * zrate;

    float2 centered = uv - float2(0.5, 0.5);
    float2 flow_uv = centered * (1.0 - bass_energy * 0.15) + float2(0.5, 0.5);

    float3 c1, c2;
    if (is_deck_b > 0.5) {
        c1 = float3(0.20, 0.04, 0.01);
        c2 = float3(1.00, 0.55, 0.05);
    } else {
        c1 = float3(0.01, 0.04, 0.20);
        c2 = float3(0.05, 0.75, 1.00);
    }

    float h_boost = high_energy * 0.35;
    float m_boost = mid_energy * 0.25;
    c2 += float3(h_boost, m_boost, h_boost * 1.2);

    float n1 = 0.0;
    float3 flow_pos = float3(flow_uv * (3.0 + freq * 10.0), t_flow * 0.2 + t_z * 0.05);
    for (int k = 0; k < 12; ++k) {
        float f_step = (float)k;
        n1 += snoise_fluid(flow_pos + float3(0.0, 0.0, f_step * 0.08), 2.2, 1.1) * (1.0 / (f_step + 1.0));
    }

    float f = smoothstep(depth - blur, depth + blur, n1);
    float3 final_color = lerp(c1, c2, f);
    final_color += c2 * (bass_energy * 0.30);

    return float4(saturate(final_color), 1.0);
}
)";
        effect_str += R"(
// -----------------------------------------------------------------------------
// SHADER 1: TruchetDualLevelSpin (Forgó Labirintus)
// -----------------------------------------------------------------------------
float2 hash22_truchet(float2 p)
{
    float n = sin(dot(p, float2(41.0, 289.0)));
    return frac(float2(262144.0, 32768.0) * n);
}

float4 render_truchet(float2 uv)
{
    float rate = p0;
    float zoom = p1;
    float thickness = p2;
    float pulse = p3;

    float t = anim_time * rate;
    float2 p = (uv - 0.5) * float2(1.77777, 1.0) * (zoom * 4.0);

    p = rotate2D(p, t * 0.2);

    float2 ip = floor(p);
    float2 fp = frac(p) - 0.5;

    float2 rnd = hash22_truchet(ip);
    if (rnd.x > 0.5) fp = float2(-fp.y, fp.x);

    float d1 = abs(length(fp - 0.5) - 0.5);
    float d2 = abs(length(fp + 0.5) - 0.5);
    float d = min(d1, d2);

    float pulse_width = thickness * (1.0 + bass_energy * pulse * 1.5);
    float line_val = smoothstep(pulse_width, 0.0, d);

    float3 col_base = (is_deck_b > 0.5) ? float3(1.0, 0.5, 0.1) : float3(0.0, 0.8, 1.0);
    float3 col_glow = (is_deck_b > 0.5) ? float3(0.3, 0.05, 0.0) : float3(0.0, 0.1, 0.3);

    col_base += float3(high_energy * 0.4, mid_energy * 0.3, high_energy * 0.5);

    float3 out_rgb = lerp(col_glow, col_base, line_val);
    out_rgb += col_base * (bass_energy * 0.4);

    return float4(saturate(out_rgb), 1.0);
}

// -----------------------------------------------------------------------------
// SHADER 2: HypnoWorm (Hipnotikus Féregalagút)
// -----------------------------------------------------------------------------
float4 render_hypno_worm(float2 uv)
{
    float rate = p0;
    float size = p1;
    float edge = p2;
    float amp1 = p3;
    float amp2 = p4;

    float2 p = (uv - 0.5) * float2(1.77777, 1.0);
    float t = anim_time * rate;

    float r = length(p);
    float a = atan2(p.y, p.x);

    float worm = sin(r * size * 8.0 - t * 4.0 + sin(a * 3.0 + t) * amp1 * 10.0);
    worm += cos(r * size * 4.0 + a * 2.0 - t * 2.0) * amp2 * 10.0;

    float rings = smoothstep(0.0, edge * 0.05, abs(worm));

    float3 c1 = (is_deck_b > 0.5) ? float3(1.0, 0.25, 0.0) : float3(0.0, 0.3, 1.0);
    float3 c2 = (is_deck_b > 0.5) ? float3(1.0, 0.8, 0.2)  : float3(0.0, 0.9, 0.9);

    c2 += float3(high_energy * 0.3, mid_energy * 0.3, high_energy * 0.4);

    float3 out_rgb = lerp(c1, c2, rings);
    out_rgb += c2 * (bass_energy * 0.45);

    return float4(saturate(out_rgb), 1.0);
}

// -----------------------------------------------------------------------------
// SHADER 3: TriangleSquareTwist (Elektro Hullám Csavarás)
// -----------------------------------------------------------------------------
float electro_wave(float2 p, float d, float t)
{
    return abs(sin(d * 12.0 - t * 6.0) * 0.05);
}

float4 render_triangle_twist(float2 uv)
{
    float zoom = p0;
    float rot = p1;
    float twist = p2;
    float tunnel = p3;
    float thickness = p4;
    float amplitude = p5;

    float2 p = (uv - 0.5) * float2(1.77777, 1.0) * zoom;
    float t_sec = anim_time * (bpm > 20.0 ? bpm / 60.0 : 2.0);

    p = rotate2D(p, rot * 6.28318);

    float grey = 0.0;
    for (int i = 0; i < 18; ++i) {
        float fi = (float)i;
        float d = length(p);
        float w = electro_wave(p, d, t_sec);
        grey += 1.0 - smoothstep(0.0, thickness * (1.0 + bass_energy * 0.8), abs(d - fi * 0.08 * tunnel) - w * (amplitude * 0.01));

        p *= 1.06;
        p = rotate2D(p, twist * -6.28318);
    }

    float3 theme_col = (is_deck_b > 0.5) ? float3(1.0, 0.55, 0.1) : float3(0.0, 0.8, 1.0);
    theme_col += float3(mid_energy * 0.3, high_energy * 0.4, high_energy * 0.4);

    float3 out_rgb = theme_col * grey;
    out_rgb += theme_col * (bass_energy * 0.4);

    return float4(saturate(out_rgb), 1.0);
}
)";
        effect_str += R"(
// -----------------------------------------------------------------------------
// SHADER 4: StringArt (Geometrikus Szálművészet)
// -----------------------------------------------------------------------------
float cyc_hlsl(float a) {
    float c = cos(a), s = sin(a);
    return dot(float2(c, s), float2(s, -c));
}

float2 rotate_string(float2 st, float a, float f1) {
    float b = cyc_hlsl(a);
    a = cyc_hlsl(lerp(a, b, f1));
    float c = cos(a), s = sin(a);
    return float2(st.x * c - st.y * s, st.x * s + st.y * c);
}

float segment_string(float2 p, float2 a, float2 b, float r1, float r2, float T, float th) {
    float2 ab = b - a, ap = p - a;
    float2 rr = rotate_string(float2(r1, r2), T * 3.14159265, 0.5);
    float k = clamp(dot(ap, ab) / max(0.00001, dot(ab, ab)), rr.x, rr.y);
    return smoothstep(0.0, 5.0 / 1080.0, length(ap - k * ab) - th);
}

float4 render_string_art(float2 uv) {
    float rate = p0;
    float sc = p1;
    float f1 = p2;
    float f2 = p3;
    float r1 = p4;
    float r2 = p5;
    float th = p6;

    float T = 0.01 * (anim_time * rate);
    float2 cc = (uv - 0.5) * float2(1.77777, 1.0) * 2.0;

    float d = 1.0;
    float angle = f1 + cos(T) * 6.28318;
    float2 a = float2(sc, 0.0);
    float2 rot = float2(cos(angle), sin(angle) * f2);

    for (int i = 0; i < 5; ++i) {
        float2 b = a;
        for (int j = 0; j < 16; ++j) {
            b = float2(b.x * rot.x - b.y * rot.y, b.x * rot.y + b.y * rot.x);
            d = min(d, segment_string(abs(cc), a, b, r1, r2, T, th * (1.0 + bass_energy * 1.5)));
        }
        a = float2(a.x * rot.x - a.y * rot.y, a.x * rot.y + a.y * rot.x);
    }

    float val = pow(max(0.0, 1.0 - d), 2.0);
    float3 col = (is_deck_b > 0.5) ? float3(1.0, 0.6, 0.1) : float3(0.0, 0.85, 1.0);
    col += float3(high_energy * 0.4, mid_energy * 0.3, high_energy * 0.5);
    col += col * (bass_energy * 0.5);
    return float4(saturate(col * val * 2.2), 1.0);
}

// -----------------------------------------------------------------------------
// SHADER 5: Dodecahedron (3D Kristály Raymarching)
// -----------------------------------------------------------------------------
float dist_triXY(float3 p, float2 h, float vier) {
    float3 q = abs(p);
    return max(q.z - h.y, max(q.x * 0.866025 + p.y * 0.5, -p.y) - h.x * 0.5 * vier);
}

float map_dodecahedron(float3 p, float een, float twee, float drie, float vier, float acht) {
    float r = 1.0 * 2.0 * 1.414214 / 1.732051;
    float2 h = float2(r, 0.01);
    float2 h2 = h * float2(0.73, 4.0);

    float3 c0 = float3(0.333333 * een, -0.333333, -0.333333);
    float3 c1 = float3(-0.333333, -0.333333 * een, 0.333333);
    float3 c2 = float3(-0.333333, 0.333333, -0.333333 * een);

    float d0 = dist_triXY(p - c0, h, vier);
    float d1 = dist_triXY(p - c1, h2, vier);
    float d2 = dist_triXY(p - c2, h, vier);

    return min(d0, min(d1, d2));
}

float4 render_dodecahedron(float2 uv) {
    float zoom = p0;
    float een = p1;
    float twee = p2;
    float drie = p3;
    float vijf = p4;
    float acht = p5;

    float2 p_ndc = (uv - 0.5) * float2(1.77777, 1.0) * (2.0 / max(0.1, zoom));
    float t_rot = anim_time * 0.4;
    float3 ro = float3(sin(t_rot) * 3.5, cos(t_rot * 0.7) * 2.0, cos(t_rot) * 3.5);
    float3 ta = float3(0.0, 0.0, 0.0);
    float3 ww = normalize(ta - ro);
    float3 uu = normalize(cross(ww, float3(0.0, 1.0, 0.0)));
    float3 vv = normalize(cross(uu, ww));
    float3 rd = normalize(p_ndc.x * uu + p_ndc.y * vv + 1.8 * ww);

    float t = 0.0;
    float d = 0.0;
    for (int i = 0; i < 48; ++i) {
        float3 pos = ro + rd * t;
        d = map_dodecahedron(pos, een, twee, drie, 1.0 + bass_energy * 0.5, acht);
        if (d < 0.002 || t > 20.0) break;
        t += d;
    }

    float3 col = float3(0.02, 0.03, 0.06);
    if (t < 20.0) {
        float glow = vijf / (1.0 + t * t * 0.05);
        float3 theme = (is_deck_b > 0.5) ? float3(1.0, 0.5, 0.1) : float3(0.0, 0.8, 1.0);
        theme += float3(high_energy * 0.4, mid_energy * 0.3, high_energy * 0.4);
        col = theme * glow * (0.35 + bass_energy * 0.4);
    }
    return float4(saturate(col), 1.0);
}
)";
        effect_str += R"(
// -----------------------------------------------------------------------------
// SHADER 6: KaliCircuitsExplorer (Fraktál Áramkörök)
// -----------------------------------------------------------------------------
float4 render_kali_circuits(float2 uv) {
    float rate = p0;
    float zoom = p1;
    float intensity = p2;
    float focus = p3;
    float pulse = p4;
    float glow = p5;

    float T = anim_time * 0.08 * rate;
    float Z = (1.05 - clamp(zoom, 0.1, 1.0));
    float2 pos = (uv - 0.5) * float2(1.77777, 1.0);
    float2 p = pos * Z;

    float a = T * 3.14159;
    p = rotate2D(p, a);

    float S = (101.0 + glow) * intensity;
    float3 color = float3(0.0, 0.0, 0.0);

    float2 z = p;
    float ot = 1000.0;
    float ot2 = 1000.0;
    float M = 0.0;

    for (int i = 0; i < 9; ++i) {
        z = abs(z) / clamp(dot(z, z), 0.1, 0.5) - T;
        float l = length(z);
        float o = min(max(abs(min(z.x, z.y)), -l + 0.25), abs(l - 0.25));
        ot = min(ot, o);
        ot2 = min(l * 0.1, ot2);
        M = max(M, float(i) * (1.0 - abs(sign(ot - o))));
    }
    M += 1.0;
    float w = (intensity * zoom) * M;
    float circ = pow(max(0.0, (w - ot2) / max(0.0001, w)), 6.0);
    S += max(pow(max(0.0, (w - ot) / max(0.0001, w)), 0.25), circ);

    float3 theme = (is_deck_b > 0.5) ? float3(1.0, 0.5, 0.1) : float3(0.0, 0.8, 1.0);
    float3 col = normalize(0.1 + float3(0.45, 0.75, M * 0.1));
    color += col * (0.4 + glsl_mod(M / 9.0 - T * pulse + ot2 * 2.0, 1.0));
    color += theme * circ * (10.0 - M) * 2.5;

    float3 out_rgb = lerp(float3(0.04, 0.05, 0.08), color, clamp(S * 0.15, 0.0, 1.0));
    out_rgb += theme * (bass_energy * 0.45);
    out_rgb += float3(high_energy * 0.2, mid_energy * 0.2, high_energy * 0.3);
    return float4(saturate(out_rgb), 1.0);
}

// -----------------------------------------------------------------------------
// SHADER 7: DiamondVision (Tórusz Sugárkövetés)
// -----------------------------------------------------------------------------
float torus_dv(float3 pos, float2 tor) {
    float2 qt = abs(float2(max(abs(pos.x), abs(pos.z)) - tor.x, pos.y));
    return max(qt.x, qt.y) - tor.y;
}

float trap_dv(float3 tp) {
    return abs(min(torus_dv(tp, float2(0.865, 0.125)), max(abs(tp.z) - 0.125, abs(tp.x) - 0.125))) - 0.005;
}

float map_dv(float3 pm, float W, float Bat, float Rot, float Rat, float time2) {
    float c = dot(abs(pm.yz), float2(0.5, 0.5)) - 0.05;
    float3 m = abs(1.0 - glsl_mod(pm, 2.0));
    m.yz = rotate2D(m.yz, sqrt(time2));
    float e = 9999.0, f = 1.0 * W;
    for (float i = 0.0; i < 4.0; i += 1.0) {
        m.xz = rotate2D(m.xz, (i * 0.333 + time2) * 0.01745);
        m.zy = rotate2D(m.zy, ((i + i) * 0.667 + time2 * 1.618) * 0.01745);
        m = abs(1.0 - glsl_mod(m + i / 3.0, 2.0));
        m *= abs(sqrt(m) * 1.77245);
        f *= 0.5;
        e = min(e, trap_dv(m) * f);
    }
    return max(e, -c);
}

float4 render_diamond_vision(float2 uv) {
    float Speed = p0;
    float Bat = p1;
    float W = p2;
    float R = p3;
    float Rot = p4;
    float Rat = p5;

    float time2 = anim_time * Speed * 10.0 + 50.0;
    float2 ps = (uv - 0.5) * float2(1.77777, 1.0) * 2.0;

    float3 co = float3(time2 * 0.05, 0.0, 0.0);
    float3 rd = normalize(float3(ps.x, ps.y, 1.0 - length(ps) * 0.5));

    float d = 1.0, it = 0.0;
    float3 p_ray = co;
    float md = 1.618 + sin(time2 * 0.5) * 0.25;

    for (int i = 0; i < 36; ++i) {
        if (d < 0.001) break;
        d = map_dv(p_ray, W, Bat, Rot, Rat, time2);
        p_ray += d * rd;
        md = min(md, d);
        it += 1.0;
    }

    float3 col = float3(0.0, 0.0, 0.0);
    if (d < 0.002) {
        float q = 1.0 - (it / 35.0);
        col = hsv2rgb(q * 0.2 + 0.5 * R, 1.0 - q * 0.865, q);
    }
    col += hsv2rgb(d, 1.0, 1.0) * md * 20.0;

    float3 theme = (is_deck_b > 0.5) ? float3(1.0, 0.6, 0.2) : float3(0.2, 0.8, 1.0);
    col = lerp(col, col * theme, 0.6);
    col += theme * (bass_energy * 0.45);
    return float4(saturate(col), 1.0);
}

// -----------------------------------------------------------------------------
// Pixel Shader Főbelépési Pont (Main Entry Point with 8 Selectable Modes)
// -----------------------------------------------------------------------------
float4 PSVisualizer(VertData v) : TARGET
{
    if (visualizer_mode < 0.5) {
        return render_fluid_flow(v.uv);
    } else if (visualizer_mode < 1.5) {
        return render_truchet(v.uv);
    } else if (visualizer_mode < 2.5) {
        return render_hypno_worm(v.uv);
    } else if (visualizer_mode < 3.5) {
        return render_triangle_twist(v.uv);
    } else if (visualizer_mode < 4.5) {
        return render_string_art(v.uv);
    } else if (visualizer_mode < 5.5) {
        return render_dodecahedron(v.uv);
    } else if (visualizer_mode < 6.5) {
        return render_kali_circuits(v.uv);
    } else {
        return render_diamond_vision(v.uv);
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
    }
    return effect_str.c_str();
}

#define VJ_VISUALIZER_EFFECT getVjVisualizerEffect()
