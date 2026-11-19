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

    // Egyedi színbeállítások (szabadon módosítható, nem kötelező a deck-színhez rendelni)
    float custom_color_r = -1.0f; // -1.0 = Alapértelmezett Deck szín, 0.0..1.0 = Egyedi szín
    float custom_color_g = -1.0f;
    float custom_color_b = -1.0f;
    int color_mod_mode = 0; // 0: Fix, 1: Beat Pulse (Kick), 2: Zenei Tónus (Macro-Build-up), 3: Színkör (Rainbow)

    // BGR / RGB hangszín és szaturáció inverzió
    bool rgb_inverted = false;
};

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

inline VjDeckConfig getDefaultVjConfig(int shader_idx) {
    VjDeckConfig cfg;
    cfg.shader_idx = std::clamp(shader_idx, 0, 63);
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
