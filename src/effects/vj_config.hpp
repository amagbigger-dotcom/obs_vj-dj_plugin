#pragma once

#include <string>
#include <vector>
#include <array>
#include <algorithm>

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
    int mod_mode = 0; // 0: Kézi (Manual/Off), 1: Zene Üteme (Beat Pulse), 2: Tónusos (Spectral), 3: Mindkettő (Both)
};

struct VjDeckConfig {
    int shader_idx = 0;
    std::array<VjParamConfig, 8> params;
};

inline const std::vector<VjShaderDef>& getVjShaderDefinitions() {
    static const std::vector<VjShaderDef> defs = {
        {
            0, "Fluid Flow 3D", "Organikus 3D Folyadék",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.5f, 0.05f},
                {"zrate", "Z-Áramlás / Z-Rate", -3.0f, 3.0f, -1.2f, 0.05f},
                {"depth", "Mélység / Depth", 0.70f, 0.95f, 0.82f, 0.01f},
                {"blur", "Elmosás / Blur", 0.05f, 0.35f, 0.15f, 0.005f},
                {"freq", "Frekvencia 1 / Freq 1", 0.005f, 0.10f, 0.025f, 0.002f},
                {"freq2", "Frekvencia 2 / Freq 2", 0.10f, 1.0f, 0.45f, 0.01f}
            }
        },
        {
            1, "Truchet Spin 3D", "3D Raymarching Labirintus",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.5f, 4.0f, 2.0f, 0.05f},
                {"thickness", "Vastagság / Thickness", 0.01f, 0.20f, 0.05f, 0.005f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.02f}
            }
        },
        {
            2, "Hypno Worm", "24-Lépéses OpArt Alagút",
            {
                {"rate", "Sebesség / Rate", -5.0f, 5.0f, 1.6f, 0.1f},
                {"size", "Méret / Size", 1.0f, 15.0f, 8.95f, 0.1f},
                {"edge", "Élesség / Edge", 3.0f, 30.0f, 23.0f, 0.5f},
                {"amp1", "Hullám 1 / Amp 1", 0.01f, 0.35f, 0.12f, 0.005f},
                {"amp2", "Hullám 2 / Amp 2", 0.01f, 0.30f, 0.08f, 0.005f}
            }
        },
        {
            3, "Shape Twist", "18-Lépéses Elektro Hullám",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.5f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 3.0f, 1.0f, 0.05f},
                {"amplitude", "Amplitúdó / Amp", 0.0f, 80.0f, 25.0f, 1.0f},
                {"thickness", "Vastagság / Thickness", 0.001f, 0.03f, 0.005f, 0.001f},
                {"twist", "Csavarás / Twist", 0.0f, 0.1f, 0.025f, 0.002f}
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
                {"thickness", "Szálvastagság", 0.0001f, 0.005f, 0.001f, 0.0001f}
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
                {"acht", "Aranyarány / Golden", 0.1f, 3.0f, 1.0f, 0.05f}
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
                {"glow", "Izzás / Glow", -50.0f, 50.0f, 10.0f, 1.0f}
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
                {"Rat", "Ráta / Ratio", -1.0f, 2.0f, 1.0f, 0.05f}
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
                {"pulse", "Kick Basszus Pulzus", 0.0f, 2.0f, 0.8f, 0.05f}
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
        }
    };
    return defs;
}

inline VjDeckConfig getDefaultVjConfig(int shader_idx) {
    VjDeckConfig cfg;
    cfg.shader_idx = std::clamp(shader_idx, 0, 11);
    const auto &defs = getVjShaderDefinitions();
    if (cfg.shader_idx >= 0 && cfg.shader_idx < static_cast<int>(defs.size())) {
        const auto &p_defs = defs[cfg.shader_idx].params;
        for (size_t i = 0; i < p_defs.size() && i < 8; ++i) {
            cfg.params[i].value = p_defs[i].def_val;
            cfg.params[i].mod_mode = 0;
        }
    }
    return cfg;
}

inline float computeEffectiveParam(const VjParamDef &def, float base_val, int mod_mode,
                                   float bass_energy, float tone_balance)
{
    float val = base_val;
    const float span = def.max_val - def.min_val;
    if (mod_mode == 1) {
        // Beat / Kick pulse modulation (lábdob leütésre tágul / lüktet)
        val += (bass_energy - 0.20f) * (span * 0.40f);
    } else if (mod_mode == 2) {
        // Tone / Spectral balance modulation (magashang vs mélyhang arány)
        val += (tone_balance - 0.50f) * (span * 0.45f);
    } else if (mod_mode == 3) {
        // Both Beat + Tone
        val += (bass_energy - 0.20f) * (span * 0.25f) + (tone_balance - 0.50f) * (span * 0.25f);
    }
    return std::clamp(val, def.min_val, def.max_val);
}
