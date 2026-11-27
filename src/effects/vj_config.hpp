#pragma once

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

struct VjParamDef {
    const char *id;
    const char *name;
    float min_val;
    float max_val;
    float def_val;
    float step;
};

struct VjShaderDef {
    int id;
    const char *name;
    const char *category;
    std::vector<VjParamDef> params;
};

struct VjParamConfig {
    float value = 1.0f;
    float mod_depth = 0.0f; // -1.0f .. +1.0f (0.0 = Középállás / Inaktív)
    int mod_mode = 0;       // 0: Kézi, 1: Bass, 2: Mid, 3: High, 4: Tónus, 5: LFO 1, 6: LFO 2, 7: Bass+Tónus
};

struct VjDeckConfig {
    int shader_idx = 0;
    // Mind a 15 beépített + egyedi ISF shader potméter-beállításai külön-külön eltárolva
    std::array<std::array<VjParamConfig, 8>, 64> all_shader_params;
    // Az éppen aktív shader paraméterei
    std::array<VjParamConfig, 8> params;

    // Globális zenei reaktivitási / beat sensitivity szorzó (0.0 .. 2.0, Alapértelmezett: 1.0 = 100%)
    float audio_reactivity = 1.0f;

    // Egyedi színbeállítások (szabadon módosítható, nem kötelező a deck-színhez rendelni)
    float custom_color_r = -1.0f; // -1.0 = Alapértelmezett Deck szín, 0.0..1.0 = Egyedi szín
    float custom_color_g = -1.0f;
    float custom_color_b = -1.0f;
    int color_mod_mode = 0; // 0: Fix, 1: Beat Pulse (Kick), 2: Zenei Tónus (Macro-Build-up), 3: Színkör (Rainbow)

    // BGR / RGB hangszín és szaturáció inverzió
    bool rgb_inverted = false;
};

#include "effects/crossfade_fx_config.hpp"
#include "engine/lfo_engine.hpp"

struct MasterFxTrackConfig {
    bool valid = false;
    int crossfade_fx_mode = 0;
    float intensity = 1.0f;
    int intensity_mod = 0;
    float rate = 1.0f;
    int rate_mod = 0;
    float blend_boost = 1.0f;
    std::array<std::array<CrossfadeFxParamConfig, 8>, 16> fx_params{};
    LfoConfig lfo1{};
    LfoConfig lfo2{};
};

#include "effects/vj_user_defaults.hpp"

// VJ Shader definíciók lekérdezése (15 beépített + dinamikus felhasználói ISF shaderek)
const std::vector<VjShaderDef>& getVjShaderDefinitions();

/*
inline const std::vector<VjShaderDef>& getVjShaderDefinitions_legacy() {
    static const std::vector<VjShaderDef> defs = {
        {
            0, "Fluid Flow 3D", "Organikus 3D Folyadék",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.5f, 0.05f},
                {"zrate", "Z-Áramlás / Z-Rate", -3.0f, 3.0f, -1.2f, 0.05f},
                {"depth", "Mélység / Depth", 0.70f, 0.95f, 0.82f, 0.01f},
                {"blur", "Elmosás / Blur", 0.05f, 0.35f, 0.15f, 0.005f},
                {"freq", "Frekvencia 1 / Freq 1", 0.005f, 0.10f, 0.025f, 0.002f},
                {"freq2", "Frekvencia 2 / Freq 2", 0.10f, 1.0f, 0.45f, 0.01f},
                {"detail", "Részletesség / Detail", 1.0f, 6.0f, 3.0f, 0.5f},
                {"glow", "Fényerő / Glow", 0.5f, 3.0f, 1.2f, 0.05f}
            }
        },
        {
            1, "Truchet Spin 3D", "3D Raymarching Labirintus",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.5f, 4.0f, 2.0f, 0.05f},
                {"thickness", "Vastagság / Thickness", 0.01f, 0.20f, 0.05f, 0.005f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"twist", "Csavarás / Twist", 0.0f, 3.14f, 0.5f, 0.05f},
                {"glow", "Neon Ragyogás / Glow", 0.1f, 3.0f, 1.0f, 0.05f},
                {"speed_z", "Z-Áramlás / Speed Z", -2.0f, 2.0f, 0.5f, 0.05f},
                {"roughness", "Érdesség / Roughness", 0.0f, 1.0f, 0.2f, 0.02f}
            }
        },
        {
            2, "Hypno Worm", "24-Lépéses OpArt Alagút",
            {
                {"rate", "Sebesség / Rate", -5.0f, 5.0f, 1.6f, 0.1f},
                {"size", "Méret / Size", 1.0f, 15.0f, 8.95f, 0.1f},
                {"edge", "Élesség / Edge", 3.0f, 30.0f, 23.0f, 0.5f},
                {"amp1", "Hullám 1 / Amp 1", 0.01f, 0.35f, 0.12f, 0.005f},
                {"amp2", "Hullám 2 / Amp 2", 0.01f, 0.30f, 0.08f, 0.005f},
                {"twist", "Alagút Csavarás / Twist", -2.0f, 2.0f, 0.0f, 0.05f},
                {"rings", "Gyűrű Sűrűség / Rings", 5.0f, 40.0f, 18.0f, 1.0f},
                {"glow", "Neon Peremfény / Glow", 0.2f, 3.0f, 1.0f, 0.05f}
            }
        },
        {
            3, "Shape Twist", "18-Lépéses Elektro Hullám",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.5f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 3.0f, 1.0f, 0.05f},
                {"amplitude", "Amplitúdó / Amp", 0.0f, 80.0f, 25.0f, 1.0f},
                {"thickness", "Vastagság / Thickness", 0.001f, 0.03f, 0.005f, 0.001f},
                {"twist", "Csavarás / Twist", 0.0f, 0.1f, 0.025f, 0.002f},
                {"freq", "Hullám Frekvencia / Freq", 0.5f, 5.0f, 2.0f, 0.1f},
                {"glow", "Neon Izzás / Glow", 0.1f, 3.0f, 1.0f, 0.05f},
                {"decay", "Csillapodás / Decay", 0.1f, 2.0f, 0.8f, 0.05f}
            }
        },
        {
            4, "String Art", "Geometrikus Szálművészet",
            {
                {"rate", "Sebesség / Rate", -2.0f, 2.0f, 0.5f, 0.05f},
                {"scale", "Méretarány / Scale", 0.25f, 3.0f, 1.5f, 0.05f},
                {"freq1", "Frekvencia 1", 0.0f, 1.0f, 0.5f, 0.02f},
                {"freq2", "Frekvencia 2", 0.0f, 2.0f, 1.25f, 0.02f},
                {"radius1", "Belső Sugár / R1", -0.5f, 1.5f, 0.0f, 0.02f},
                {"radius2", "Külső Sugár / R2", 0.5f, 2.5f, 1.0f, 0.02f},
                {"thickness", "Szálvastagság", 0.0001f, 0.005f, 0.001f, 0.0001f},
                {"phase", "Fáziseltolás / Phase", 0.0f, 3.14f, 0.5f, 0.05f}
            }
        },
        {
            5, "Dodecahedron 3D", "3D Raymarching Kristály",
            {
                {"Zoom", "Nagyítás / Zoom", 0.2f, 2.0f, 1.0f, 0.05f},
                {"een", "Forma 1", 0.0f, 10.0f, 1.0f, 0.1f},
                {"twee", "Forma 2", 0.0f, 10.0f, 5.0f, 0.1f},
                {"drie", "Forma 3", 0.0f, 10.0f, 5.0f, 0.1f},
                {"vijf", "Fényerő / Brightness", 0.0f, 8.0f, 4.4f, 0.1f},
                {"acht", "Aranyarány / Golden", 0.1f, 3.0f, 1.0f, 0.05f},
                {"rot_spd", "Forgási Ráta / Rot Speed", -3.0f, 3.0f, 0.8f, 0.05f},
                {"edge_glow", "Él Ragyogás / Edge Glow", 0.1f, 3.0f, 1.2f, 0.05f}
            }
        },
        {
            6, "Kali Circuits", "Fraktál Áramkörök",
            {
                {"rate", "Sebesség / Rate", 0.01f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.1f, 1.0f, 0.67f, 0.02f},
                {"intensity", "Intenzitás", 0.01f, 0.05f, 0.03f, 0.002f},
                {"focus", "Fókusz / Focus", 0.5f, 5.0f, 2.14f, 0.05f},
                {"pulse", "Pulzus / Pulse", 6.0f, 60.0f, 30.0f, 1.0f},
                {"glow", "Izzás / Glow", -50.0f, 50.0f, 10.0f, 1.0f},
                {"iter_steps", "Fraktál Mélység / Steps", 4.0f, 20.0f, 11.0f, 1.0f},
                {"shift_y", "Térbeli Eltolás / Shift Y", -1.0f, 1.0f, 0.0f, 0.05f}
            }
        },
        {
            7, "Diamond Vision", "Tórusz Sugárkövetés",
            {
                {"Speed", "Sebesség / Speed", -1.0f, 1.0f, 0.1f, 0.02f},
                {"Bat", "Tóruszok Száma", 0.5f, 1.3f, 1.0f, 0.02f},
                {"W", "Sugárkövetés / Ray", 0.0f, 2.0f, 1.0f, 0.05f},
                {"R", "Színárnyalat / HUE", -1.0f, 1.0f, 1.0f, 0.05f},
                {"Rot", "Forgatás / Rotation", 0.1f, 2.0f, 1.0f, 0.05f},
                {"Rat", "Ráta / Ratio", -1.0f, 2.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 3.0f, 1.0f, 0.05f},
                {"glow", "Fényerő / Glow", 0.2f, 3.0f, 1.1f, 0.05f}
            }
        },
        {
            8, "Pupila", "Organikus Szem & Írisz",
            {
                {"zoom", "Nagyítás / Zoom", 0.2f, 2.0f, 0.5f, 0.05f},
                {"irisR", "Írisz Vörös (R)", 0.0f, 1.0f, 0.42f, 0.02f},
                {"irisG", "Írisz Zöld (G)", 0.0f, 1.0f, 0.52f, 0.02f},
                {"irisB", "Írisz Kék (B)", 0.0f, 1.0f, 0.78f, 0.02f},
                {"borderR", "Keret Vörös", 0.0f, 1.0f, 0.0f, 0.02f},
                {"borderG", "Keret Zöld", 0.0f, 1.0f, 0.53f, 0.02f},
                {"borderB", "Keret Kék", 0.0f, 1.0f, 1.0f, 0.02f},
                {"pupilPulse", "Lábdob Pupilla Pulzus", 0.0f, 2.0f, 1.0f, 0.05f}
            }
        },
        {
            9, "MandalaScope", "72-Lépéses Fraktál Mandala",
            {
                {"rate", "Sebesség / Rate", -5.0f, 5.0f, 1.5f, 0.1f},
                {"sides", "Oldalak Száma / Sides", 2.0f, 32.0f, 4.0f, 1.0f},
                {"scale", "Lépték / Scale", 0.5f, 5.0f, 2.0f, 0.1f},
                {"multiply", "Szorzó / Multiply", 0.0f, 6.0f, 4.0f, 0.2f},
                {"loops", "Iterációk / Loops", 6.0f, 72.0f, 28.0f, 2.0f},
                {"mX", "Forma X / mX", 0.0f, 2.0f, 0.25f, 0.02f},
                {"mY", "Forma Y / mY", -2.0f, 1.0f, -0.75f, 0.02f},
                {"pulse", "Zenei Lüktetés / Pulse", 0.0f, 2.0f, 0.5f, 0.05f}
            }
        },
        {
            10, "Searching Light", "Kiber Fényalagút",
            {
                {"scale", "Fénysugár Vastagság", 0.0005f, 0.015f, 0.002f, 0.0002f},
                {"modVal", "Térbeli Osztás / Mod", 0.2f, 4.0f, 1.0f, 0.05f},
                {"zAdder", "Z-Áramlás / Speed", -1.0f, 1.0f, 0.7f, 0.05f},
                {"cosinAdder", "Koszinusz Torzítás", -1.0f, 1.0f, 0.7f, 0.05f},
                {"sinAdder", "Szinusz Torzítás", -1.0f, 1.0f, 0.7f, 0.05f},
                {"finalMulti", "Fényerő / Glow", 0.1f, 2.5f, 0.8f, 0.05f},
                {"pulse", "Kick Basszus Pulzus", 0.0f, 2.0f, 0.8f, 0.05f},
                {"rot_spd", "Forgási Ráta / Rot Speed", -2.0f, 2.0f, 0.4f, 0.05f}
            }
        },
        {
            11, "CubeField 3D", "Végtelen Kockamező Raymarching",
            {
                {"speed", "Kamera Sebesség", -2.0f, 2.0f, 0.5f, 0.05f},
                {"rot_z", "Forgatás Z / Rotation", 0.0f, 6.28f, 0.5f, 0.05f},
                {"repeat", "Kockák Távolsága", 0.2f, 2.0f, 0.5f, 0.02f},
                {"box_size", "Kockaméret / Box Size", 0.05f, 0.5f, 0.2f, 0.01f},
                {"squiggle", "Hullámcsavarás", -1.0f, 3.14f, 0.5f, 0.05f},
                {"dist1", "Térbeli Hullám 1", -1.0f, 1.0f, 0.5f, 0.05f},
                {"dist3", "Örvényhullám", 0.0f, 1.0f, 0.5f, 0.05f},
                {"termThres", "Köd / Term Küszöb", 1.0f, 8.0f, 5.0f, 0.1f}
            }
        },
        {
            12, "Electro Bands", "Pulzáló Neon Szalagok",
            {
                {"radius", "Sugár / Radius", 0.001f, 0.10f, 0.02f, 0.002f},
                {"amp", "Amplitúdó / Amp", 0.0f, 30.0f, 11.54f, 0.5f},
                {"band", "Sáv / Band", -1.0f, 1.0f, 0.75f, 0.05f},
                {"speed", "Sebesség / Speed", 0.0f, 2.0f, 0.33f, 0.05f},
                {"thickness", "Vastagság / Thick", 0.1f, 10.0f, 1.05f, 0.1f},
                {"size", "Méretarány / Size", 5.0f, 100.0f, 47.5f, 1.0f},
                {"twist", "Szalagcsavarás / Twist", -2.0f, 2.0f, 0.0f, 0.05f},
                {"glow", "Neon Izzás / Glow", 0.2f, 3.0f, 1.2f, 0.05f}
            }
        },
        {
            13, "GlobularCluster", "3D Gömbhalmaz Torzulás",
            {
                {"rate", "Forgási Ráta / Rate", -5.0f, 5.0f, 2.5f, 0.1f},
                {"warpx", "Warp X", 1.0f, 20.0f, 4.0f, 0.2f},
                {"warpy", "Warp Y", 1.0f, 20.0f, 3.0f, 0.2f},
                {"warpz", "Warp Z", 1.0f, 10.0f, 1.0f, 0.1f},
                {"melt", "Olvadás / Melt", 0.5f, 5.0f, 2.5f, 0.1f},
                {"density", "Sűrűség / Density", 0.5f, 20.0f, 4.67f, 0.2f},
                {"depth", "Mélység / Depth", 0.5f, 4.0f, 1.33f, 0.1f},
                {"tint", "Színárnyalat / Tint", -0.5f, 0.5f, 0.0f, 0.02f}
            }
        },
        {
            14, "HexTruchWarpFlow2", "Hexagonális Truchet Áramlás",
            {
                {"scale", "Méretarány / Scale", 0.5f, 4.0f, 2.36f, 0.05f},
                {"rate", "Forgási Ráta / Rate", -5.0f, 5.0f, 4.0f, 0.1f},
                {"motion", "Mozgás / Motion", -5.0f, 5.0f, -4.75f, 0.1f},
                {"thickness", "Vastagság / Thickness", 0.1f, 0.65f, 0.215f, 0.01f},
                {"warp", "Warp Csavarás", 0.0f, 3.0f, 1.0f, 0.05f},
                {"arrows", "Nyilak / Arrows", 0.0f, 1.0f, 0.75f, 0.05f},
                {"dashes", "Szaggatások / Dashes", 0.0f, 1.0f, 0.35f, 0.05f},
                {"pulse", "Kick Basszus Pulzus", 0.0f, 2.0f, 0.5f, 0.05f}
            }
        }
    };
    return defs;
}
*/

inline void applyDefaultShaderModulations(VjDeckConfig &cfg) {
    auto set_mod = [&](int s, int p, int mode, float depth) {
        if (s >= 0 && s < 64 && p >= 0 && p < 8) {
            cfg.all_shader_params[s][p].mod_mode = mode;
            cfg.all_shader_params[s][p].mod_depth = depth;
        }
    };

    // 0: Fluid Flow 3D (scale, viscosity, sharpness, density, layers, glow, color_mix, edge_rim)
    set_mod(0, 0, 0, 0.0f);  // Scale -> Kézi
    set_mod(0, 1, 0, 0.0f);  // Viscosity -> Kézi
    set_mod(0, 2, 0, 0.0f);  // Sharpness -> Kézi
    set_mod(0, 3, 1, 0.40f); // Density -> Bass Pulse +40%
    set_mod(0, 4, 0, 0.0f);  // Layers -> Kézi
    set_mod(0, 5, 4, 0.30f); // Glow -> Macro-Tonus +30%
    set_mod(0, 6, 6, 0.25f); // Color Mix -> LFO 2 +25%
    set_mod(0, 7, 3, 0.45f); // Edge Rim -> High Pulse +45%

    // 1: Truchet Spin 3D (zoom, thickness, pulse, twist, roughness, glow, fog_depth, symmetry)
    set_mod(1, 0, 1, 0.35f); // Zoom -> Bass Pulse +35%
    set_mod(1, 1, 2, 0.30f); // Thickness -> Mid Pulse +30%
    set_mod(1, 2, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(1, 3, 5, 0.35f); // Twist -> LFO 1 +35%
    set_mod(1, 4, 3, 0.25f); // Roughness -> High Pulse +25%
    set_mod(1, 5, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(1, 6, 4, 0.30f); // Fog Depth -> Macro-Tonus +30%
    set_mod(1, 7, 6, 0.20f); // Symmetry -> LFO 2 +20%

    // 2: Hypno Worm (size, rings, amp_x, amp_y, pulse, twist, edge_contrast, glow)
    set_mod(2, 0, 1, 0.40f); // Size -> Bass Pulse +40%
    set_mod(2, 1, 2, 0.30f); // Rings -> Mid Pulse +30%
    set_mod(2, 2, 5, 0.35f); // Amp X -> LFO 1 +35%
    set_mod(2, 3, 6, 0.35f); // Amp Y -> LFO 2 +35%
    set_mod(2, 4, 1, 0.55f); // Pulse -> Bass Pulse +55%
    set_mod(2, 5, 4, 0.30f); // Twist -> Macro-Tonus +30%
    set_mod(2, 6, 3, 0.30f); // Edge Contrast -> High Pulse +30%
    set_mod(2, 7, 7, 0.40f); // Glow -> Bass + Tonus +40%

    // 3: Shape Twist (zoom, amplitude, thickness, freq, twist, pulse, glow, decay)
    set_mod(3, 0, 1, 0.35f); // Zoom -> Bass Pulse +35%
    set_mod(3, 1, 1, 0.45f); // Amplitude -> Bass Pulse +45%
    set_mod(3, 2, 2, 0.30f); // Thickness -> Mid Pulse +30%
    set_mod(3, 3, 5, 0.25f); // Freq -> LFO 1 +25%
    set_mod(3, 4, 6, 0.35f); // Twist -> LFO 2 +35%
    set_mod(3, 5, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(3, 6, 3, 0.35f); // Glow -> High Pulse +35%
    set_mod(3, 7, 4, 0.30f); // Decay -> Macro-Tonus +30%

    // 4: String Art (scale, lines, inner_r, outer_r, thickness, pulse, twist, glow)
    set_mod(4, 0, 1, 0.35f); // Scale -> Bass Pulse +35%
    set_mod(4, 1, 2, 0.30f); // Lines -> Mid Pulse +30%
    set_mod(4, 2, 5, 0.35f); // Inner R -> LFO 1 +35%
    set_mod(4, 3, 6, 0.35f); // Outer R -> LFO 2 +35%
    set_mod(4, 4, 3, 0.25f); // Thickness -> High Pulse +25%
    set_mod(4, 5, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(4, 6, 4, 0.35f); // Twist -> Macro-Tonus +35%
    set_mod(4, 7, 7, 0.40f); // Glow -> Bass + Tonus +40%

    // 5: Dodecahedron 3D (zoom, facet_cut, explode, pulse, refraction, glow, rot_tilt, dispersion)
    set_mod(5, 0, 1, 0.30f); // Zoom -> Bass Pulse +30%
    set_mod(5, 1, 2, 0.35f); // Facet Cut -> Mid Pulse +35%
    set_mod(5, 2, 1, 0.45f); // Explode -> Bass Pulse +45%
    set_mod(5, 3, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(5, 4, 3, 0.30f); // Refraction -> High Pulse +30%
    set_mod(5, 5, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(5, 6, 5, 0.35f); // Rot Tilt -> LFO 1 +35%
    set_mod(5, 7, 4, 0.30f); // Dispersion -> Macro-Tonus +30%

    // 6: Kali Circuits (zoom, complexity, trace_width, pulse, branching, glow, invert, core_aura)
    set_mod(6, 0, 1, 0.35f); // Zoom -> Bass Pulse +35%
    set_mod(6, 1, 2, 0.30f); // Complexity -> Mid Pulse +30%
    set_mod(6, 2, 3, 0.30f); // Trace Width -> High Pulse +30%
    set_mod(6, 3, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(6, 4, 5, 0.35f); // Branching -> LFO 1 +35%
    set_mod(6, 5, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(6, 6, 4, 0.20f); // Invert -> Macro-Tonus +20%
    set_mod(6, 7, 1, 0.40f); // Core Aura -> Bass Pulse +40%

    // 7: Diamond Vision (zoom, torus_size, thickness, twist, pulse, specular, glow, hue_shift)
    set_mod(7, 0, 1, 0.30f); // Zoom -> Bass Pulse +30%
    set_mod(7, 1, 2, 0.35f); // Torus Size -> Mid Pulse +35%
    set_mod(7, 2, 3, 0.25f); // Thickness -> High Pulse +25%
    set_mod(7, 3, 5, 0.40f); // Twist -> LFO 1 +40%
    set_mod(7, 4, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(7, 5, 3, 0.35f); // Specular -> High Pulse +35%
    set_mod(7, 6, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(7, 7, 6, 0.30f); // Hue Shift -> LFO 2 +30%

    // 8: Pupila Eye (zoom, pupil_size, pupil_pulse, iris_fibers, border_width, reflection, iris_blend, glow)
    set_mod(8, 0, 1, 0.30f); // Zoom -> Bass Pulse +30%
    set_mod(8, 1, 1, 0.50f); // Pupil Size -> Bass Pulse +50%
    set_mod(8, 2, 1, 0.60f); // Pupil Pulse -> Bass Pulse +60%
    set_mod(8, 3, 2, 0.35f); // Iris Fibers -> Mid Pulse +35%
    set_mod(8, 4, 4, 0.30f); // Border Width -> Macro-Tonus +30%
    set_mod(8, 5, 3, 0.40f); // Reflection -> High Pulse +40%
    set_mod(8, 6, 5, 0.30f); // Iris Blend -> LFO 1 +30%
    set_mod(8, 7, 7, 0.35f); // Glow -> Bass + Tonus +35%

    // 9: Mandala Scope (scale, sides, iterations, pulse, petal_spread, glow, contrast, rotation_speed)
    set_mod(9, 0, 1, 0.35f); // Scale -> Bass Pulse +35%
    set_mod(9, 1, 2, 0.25f); // Sides -> Mid Pulse +25%
    set_mod(9, 2, 4, 0.30f); // Iterations -> Macro-Tonus +30%
    set_mod(9, 3, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(9, 4, 5, 0.35f); // Petal Spread -> LFO 1 +35%
    set_mod(9, 5, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(9, 6, 3, 0.30f); // Contrast -> High Pulse +30%
    set_mod(9, 7, 6, 0.30f); // Rotation Speed -> LFO 2 +30%

    // 10: Searching Light (tunnel_w, ray_count, ray_sharpness, pulse, glow, twist, speed_z, depth_fade)
    set_mod(10, 0, 1, 0.35f); // Tunnel Width -> Bass Pulse +35%
    set_mod(10, 1, 2, 0.30f); // Ray Count -> Mid Pulse +30%
    set_mod(10, 2, 3, 0.30f); // Ray Sharpness -> High Pulse +30%
    set_mod(10, 3, 1, 0.55f); // Pulse -> Bass Pulse +55%
    set_mod(10, 4, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(10, 5, 5, 0.35f); // Twist -> LFO 1 +35%
    set_mod(10, 6, 4, 0.30f); // Speed Z -> Macro-Tonus +30%
    set_mod(10, 7, 6, 0.25f); // Depth Fade -> LFO 2 +25%

    // 11: Cube Field 3D (spacing, box_size, height_wave, pulse, edge_glow, camera_height, fog_distance, twist)
    set_mod(11, 0, 2, 0.30f); // Spacing -> Mid Pulse +30%
    set_mod(11, 1, 1, 0.40f); // Box Size -> Bass Pulse +40%
    set_mod(11, 2, 1, 0.45f); // Height Wave -> Bass Pulse +45%
    set_mod(11, 3, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(11, 4, 3, 0.35f); // Edge Glow -> High Pulse +35%
    set_mod(11, 5, 5, 0.30f); // Camera Height -> LFO 1 +30%
    set_mod(11, 6, 4, 0.30f); // Fog Distance -> Macro-Tonus +30%
    set_mod(11, 7, 6, 0.35f); // Twist -> LFO 2 +35%

    // 12: Electro Bands (radius, bands_count, amplitude, thickness, pulse, glow, twist, color_split)
    set_mod(12, 0, 1, 0.40f); // Radius -> Bass Pulse +40%
    set_mod(12, 1, 2, 0.30f); // Bands Count -> Mid Pulse +30%
    set_mod(12, 2, 1, 0.45f); // Amplitude -> Bass Pulse +45%
    set_mod(12, 3, 3, 0.25f); // Thickness -> High Pulse +25%
    set_mod(12, 4, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(12, 5, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(12, 6, 5, 0.35f); // Twist -> LFO 1 +35%
    set_mod(12, 7, 6, 0.30f); // Color Split -> LFO 2 +30%

    // 13: Globular Cluster (zoom, cluster_density, warp, melt, pulse, glow, core_radius, contrast)
    set_mod(13, 0, 1, 0.30f); // Zoom -> Bass Pulse +30%
    set_mod(13, 1, 2, 0.35f); // Cluster Density -> Mid Pulse +35%
    set_mod(13, 2, 5, 0.35f); // Warp -> LFO 1 +35%
    set_mod(13, 3, 6, 0.30f); // Melt -> LFO 2 +30%
    set_mod(13, 4, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(13, 5, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(13, 6, 1, 0.45f); // Core Radius -> Bass Pulse +45%
    set_mod(13, 7, 4, 0.30f); // Contrast -> Macro-Tonus +30%

    // 14: Hex Truch Warp (scale, thickness, warp, pulse, dashes, arrows, glow, motion_drift)
    set_mod(14, 0, 1, 0.35f); // Scale -> Bass Pulse +35%
    set_mod(14, 1, 2, 0.30f); // Thickness -> Mid Pulse +30%
    set_mod(14, 2, 5, 0.35f); // Warp -> LFO 1 +35%
    set_mod(14, 3, 1, 0.50f); // Pulse -> Bass Pulse +50%
    set_mod(14, 4, 3, 0.30f); // Dashes -> High Pulse +30%
    set_mod(14, 5, 6, 0.25f); // Arrows -> LFO 2 +25%
    set_mod(14, 6, 7, 0.40f); // Glow -> Bass + Tonus +40%
    set_mod(14, 7, 4, 0.30f); // Motion Drift -> Macro-Tonus +30%
}

inline VjDeckConfig getDefaultVjConfig(int shader_idx) {
    VjDeckConfig cfg;
    cfg.shader_idx = std::clamp(shader_idx, 0, 63);
    cfg.audio_reactivity = 1.0f;
    const auto &defs = getVjShaderDefinitions();
    for (size_t s = 0; s < 64 && s < defs.size(); ++s) {
        const auto &p_defs = defs[s].params;
        for (size_t i = 0; i < 8; ++i) {
            if (i < p_defs.size()) {
                cfg.all_shader_params[s][i].value = p_defs[i].def_val;
                cfg.all_shader_params[s][i].mod_depth = 0.0f;
                cfg.all_shader_params[s][i].mod_mode = 0;
            } else {
                cfg.all_shader_params[s][i].value = 0.0f;
                cfg.all_shader_params[s][i].mod_depth = 0.0f;
                cfg.all_shader_params[s][i].mod_mode = 0;
            }
        }
    }
    applyDefaultShaderModulations(cfg);
    loadUserShaderDefaults(cfg.all_shader_params);
    cfg.params = cfg.all_shader_params[cfg.shader_idx];
    cfg.custom_color_r = -1.0f;
    cfg.custom_color_g = -1.0f;
    cfg.custom_color_b = -1.0f;
    cfg.color_mod_mode = 0;
    cfg.rgb_inverted = false;
    return cfg;
}

inline float computeEffectiveParam(const VjParamDef &def, float base_val, float mod_depth, int mod_mode,
                                   float bass_energy, float mid_energy, float high_energy,
                                   float macro_tonus = 0.5f, float lfo1 = 0.0f, float lfo2 = 0.0f)
{
    if (std::abs(mod_depth) < 0.001f || mod_mode == 0) {
        return std::clamp(base_val, def.min_val, def.max_val);
    }

    float sig = 0.0f;
    switch (mod_mode) {
    case 1: // 🥁 Bass Pulse (<250Hz)
        sig = std::clamp((bass_energy - 0.20f) * 1.35f, -0.5f, 1.0f);
        break;
    case 2: // 🎯 Mid Pulse (250-5kHz)
        sig = std::clamp((mid_energy - 0.20f) * 1.35f, -0.5f, 1.0f);
        break;
    case 3: // ✨ High Pulse (>5kHz)
        sig = std::clamp((high_energy - 0.20f) * 1.35f, -0.5f, 1.0f);
        break;
    case 4: // 📈 Zenei Tónus (Macro-Build-up)
        sig = std::clamp((macro_tonus - 0.50f) * 2.0f, -1.0f, 1.0f);
        break;
    case 5: // 〰 LFO 1 (BPM Synced)
        sig = lfo1;
        break;
    case 6: // ∿ LFO 2 (BPM Synced)
        sig = lfo2;
        break;
    case 7: // ⚡ Bass + Tónus
        sig = std::clamp(((bass_energy - 0.20f) * 0.8f) + ((macro_tonus - 0.50f) * 1.0f), -1.0f, 1.0f);
        break;
    default:
        sig = 0.0f;
        break;
    }

    const float span = def.max_val - def.min_val;
    float val = base_val + (mod_depth * sig * span * 0.5f);
    return std::clamp(val, def.min_val, def.max_val);
}

inline float computeEffectiveParamSimple(float base_val, float min_val, float max_val,
                                         float mod_depth, int mod_mode,
                                         float bass_energy, float mid_energy, float high_energy,
                                         float macro_tonus = 0.5f, float lfo1 = 0.0f, float lfo2 = 0.0f)
{
    if (std::abs(mod_depth) < 0.001f || mod_mode == 0) {
        return std::clamp(base_val, min_val, max_val);
    }

    float sig = 0.0f;
    switch (mod_mode) {
    case 1: sig = std::clamp((bass_energy - 0.20f) * 1.35f, -0.5f, 1.0f); break;
    case 2: sig = std::clamp((mid_energy - 0.20f) * 1.35f, -0.5f, 1.0f); break;
    case 3: sig = std::clamp((high_energy - 0.20f) * 1.35f, -0.5f, 1.0f); break;
    case 4: sig = std::clamp((macro_tonus - 0.50f) * 2.0f, -1.0f, 1.0f); break;
    case 5: sig = lfo1; break;
    case 6: sig = lfo2; break;
    case 7: sig = std::clamp(((bass_energy - 0.20f) * 0.8f) + ((macro_tonus - 0.50f) * 1.0f), -1.0f, 1.0f); break;
    default: sig = 0.0f; break;
    }

    const float span = max_val - min_val;
    float val = base_val + (mod_depth * sig * span * 0.5f);
    return std::clamp(val, min_val, max_val);
}

inline void computeThemeColor(const VjDeckConfig &cfg, bool is_deck_b, float anim_time, float bpm,
                              float bass_energy, float macro_tonus,
                              float &out_r, float &out_g, float &out_b, float &out_active)
{
    float r = is_deck_b ? 1.0f : 0.0f;
    float g = is_deck_b ? 0.45f : 0.8f;
    float b = is_deck_b ? 0.0f : 1.0f;
    const bool is_custom = (cfg.custom_color_r >= 0.0f && cfg.custom_color_g >= 0.0f && cfg.custom_color_b >= 0.0f);
    if (is_custom) {
        r = std::clamp(cfg.custom_color_r, 0.0f, 1.0f);
        g = std::clamp(cfg.custom_color_g, 0.0f, 1.0f);
        b = std::clamp(cfg.custom_color_b, 0.0f, 1.0f);
    }

    if (!is_custom && cfg.color_mod_mode == 0) {
        out_r = r;
        out_g = g;
        out_b = b;
        out_active = 0.0f; // Alapértelmezett Deck A (Kék) / Deck B (Narancs) shader téma
        return;
    }

    out_active = 1.0f; // Egyedi szín vagy zenei moduláció aktív

    switch (cfg.color_mod_mode) {
    case 1: { // 🥁 Kick / Basszus pulzus (lüktetés a lábdob energiájára)
        const float pulse = 0.60f + 0.80f * std::clamp(bass_energy, 0.0f, 1.0f);
        out_r = std::clamp(r * pulse, 0.0f, 1.0f);
        out_g = std::clamp(g * pulse, 0.0f, 1.0f);
        out_b = std::clamp(b * pulse, 0.0f, 1.0f);
        break;
    }
    case 2: { // 📈 Zenei Tónus / Felépülés (átmenet a hűvös kiállásból a tüzes dropig)
        const float t = std::clamp(macro_tonus, 0.0f, 1.0f);
        const float cool_r = 0.0f, cool_g = 0.5f, cool_b = 1.0f;
        const float hot_r  = 1.0f, hot_g  = 0.2f, hot_b  = 0.05f;
        if (is_custom) {
            const float factor = 0.45f + 0.75f * t;
            out_r = std::clamp(r * factor + (1.0f - r) * t * 0.35f, 0.0f, 1.0f);
            out_g = std::clamp(g * factor, 0.0f, 1.0f);
            out_b = std::clamp(b * (1.15f - 0.4f * t), 0.0f, 1.0f);
        } else {
            out_r = (1.0f - t) * cool_r + t * hot_r;
            out_g = (1.0f - t) * cool_g + t * hot_g;
            out_b = (1.0f - t) * cool_b + t * hot_b;
        }
        break;
    }
    case 3: { // 🌈 Színkör Ciklus (BPM tempóra forgó szivárvány)
        const float speed_factor = (bpm > 30.0f ? bpm : 120.0f) / 120.0f;
        const float hue = std::fmod(anim_time * 0.12f * speed_factor, 1.0f);
        const float h6 = hue * 6.0f;
        const int i = static_cast<int>(h6);
        const float f = h6 - static_cast<float>(i);
        const float q = 1.0f - f;
        switch (i % 6) {
        case 0: out_r = 1.0f; out_g = f;    out_b = 0.0f; break;
        case 1: out_r = q;    out_g = 1.0f; out_b = 0.0f; break;
        case 2: out_r = 0.0f; out_g = 1.0f; out_b = f;    break;
        case 3: out_r = 0.0f; out_g = q;    out_b = 1.0f; break;
        case 4: out_r = f;    out_g = 0.0f; out_b = 1.0f; break;
        default: out_r = 1.0f; out_g = 0.0f; out_b = q;   break;
        }
        break;
    }
    default: // 0: Fix egyedi szín
        out_r = r;
        out_g = g;
        out_b = b;
        break;
    }
}
