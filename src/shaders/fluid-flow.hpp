#pragma once

static const char *FLUID_FLOW_EFFECT = R"(
uniform float4x4 ViewProj;
uniform float anim_time;
uniform float bpm;
uniform float audio_amp;
uniform float bass_energy;
uniform float mid_energy;
uniform float high_energy;
uniform float is_deck_b;

struct VertData {
    float4 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

VertData VSDefault(VertData v)
{
    VertData o;
    o.pos = mul(float4(v.pos.xyz, 1.0), ViewProj);
    o.uv  = v.uv;
    return o;
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

float3 mod289_v3(float3 x)
{
    return x - floor(x * 0.003460207612457) * 289.0;
}

float4 mod289_v4(float4 x)
{
    return x - floor(x * 0.003460207612457) * 289.0;
}

float4 mutate_v4(float4 x)
{
    return mod289_v4(((x * 34.0) + 1.0) * x);
}

float4 TInSq_v4(float4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise_fluid(float3 v, float freq, float freq2)
{
    const float2 C = float2(0.166666667, 0.333333333);
    float4 D = float4(0.0, 0.5, 1.0 - freq, 2.0 + freq2);
    float3 i = floor(v + dot(v, float3(C.y, C.y, C.y)));
    float3 x0 = v - i + dot(i, float3(C.x, C.x, C.x));
    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);
    float3 x1 = x0 - i1 + float3(C.x, C.x, C.x);
    float3 x2 = x0 - i2 + float3(C.y, C.y, C.y);
    float3 x3 = x0 - D.yyy;
    i = mod289_v3(i);
    float4 p = mutate_v4(mutate_v4(mutate_v4(
               i.z + float4(0.0, i1.z, i2.z, 1.0))
             + i.y + float4(0.0, i1.y, i2.y, 1.0))
             + i.x + float4(0.0, i1.x, i2.x, 1.0));
    float N = 0.142857142857;
    float3 ns = N * D.wyz - D.xzx;
    float4 j = p - 49.0 * floor(p * ns.z * ns.z);
    float4 xn = floor(j * ns.z);
    float4 yn = floor(j - 7.0 * xn);
    float4 x = xn * ns.x + ns.yyyy;
    float4 y = yn * ns.x + ns.yyyy;
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
    float4 nm = TInSq_v4(float4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= nm.x;
    p1 *= nm.y;
    p2 *= nm.z;
    p3 *= nm.w;
    float4 M = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    M = M * M;
    return 42.0 * dot(M * M, float4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

float4 PSFluidFlow(VertData v) : TARGET
{
    float2 p = (v.uv - 0.5);
    p.x *= (16.0 / 9.0); // 16:9 aranytartas

    float pi_val = 3.14159265;
    float2 m = 0.5 * pi_val * float2(2.0, 1.0);
    float3 pos = normalize(rotate_vec(float3(p, 0.5), m));

    // Rate tied to music tempo (BPM)
    float safe_bpm = (bpm > 30.0) ? bpm : 128.0;
    float rate = clamp(safe_bpm / 120.0, 0.6, 2.4) * 1.5;
    float zrate = -1.2;

    // Depth modulated smoothly by Bass / Kick transient energy (0.76 to 0.89)
    float depth = clamp(0.77 + bass_energy * 0.08, 0.75, 0.89);

    // Blur modulated by overall Audio Amplitude (0.11 to 0.22)
    float blur = clamp(0.12 + audio_amp * 0.09, 0.101, 0.24);

    // Freq and Freq2 remain stable to prevent jarring screen jerks
    float freq = 0.025;
    float freq2 = 0.45;

    float T = anim_time * rate;
    float dist = 0.0;

    [unroll]
    for (float i = 1.0; i <= 14.0; i += 1.0) {
        float3 flow_pos = pos * i + float3(sin(T - atan2(T, anim_time + 0.001)), anim_time * zrate, cos(T)) * (rate * 0.1);
        float flow = abs(snoise_fluid(flow_pos, freq, freq2));
        flow = smoothstep(blur, 0.1 * sqrt(depth), flow);
        dist = max(dist, flow * (1.0 - (i * (1.0 - depth))));
    }

    // Dynamic Color Synthesis:
    // Deck A: Deep Ocean Navy / Vivid Cyan
    // Deck B: Deep Crimson Mahogany / Warm Flame Orange
    float3 c1 = is_deck_b > 0.5 ? float3(0.24, 0.04, 0.01) : float3(0.04, 0.08, 0.32);
    float3 c2 = is_deck_b > 0.5 ? float3(1.0, 0.46, 0.0) : float3(0.0, 0.85, 1.0);

    // Modulate highlights with Mid & High frequency energy
    c2 = lerp(c2, float3(1.0, 1.0, 1.0), clamp(high_energy * 0.6, 0.0, 0.85));
    c1 = lerp(c1, is_deck_b > 0.5 ? float3(0.7, 0.12, 0.05) : float3(0.3, 0.08, 0.5), clamp(mid_energy * 0.45, 0.0, 0.75));

    float3 col = lerp(c2, c1, 1.0 - dist);
    return float4(col, 1.0);
}

technique Draw
{
    pass
    {
        vertex_shader = VSDefault(v);
        pixel_shader  = PSFluidFlow(v);
    }
}
)";
