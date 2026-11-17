#pragma once

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

struct CrossfadeFxParamDef {
    const char *id;
    const char *name;
    float min_val;
    float max_val;
    float def_val;
    float step;
};

struct CrossfadeFxDef {
    int id;
    const char *name;
    const char *description;
    std::vector<CrossfadeFxParamDef> params;
};

struct CrossfadeFxParamConfig {
    float value = 1.0f;
    int mod_mode = 0; // 0: Kézi (Off), 1: 🥁 Kick Pulzus, 2: 📈 Zenei Tónus, 3: ⚡ Mindkettő
};

inline const std::vector<CrossfadeFxDef>& getCrossfadeFxDefinitions() {
    static const std::vector<CrossfadeFxDef> defs = {
        {
            0, "Ki (Normál Átúsztatás)", "Standard Crossfade",
            {
                {"curve", "Átúsztatás Görbülete / Curve", 0.2f, 3.0f, 1.0f, 0.05f},
                {"smoothness", "S-Curve Lágyság / Smooth", 0.0f, 1.0f, 0.0f, 0.02f}
            }
        },
        {
            1, "Spirális Örvény (Swirl Vortex)", "Középponti Örvénylés",
            {
                {"twist", "Csavarás Szöge / Twist", 0.5f, 10.0f, 3.14f, 0.1f},
                {"radius", "Hatósugár / Radius", 0.2f, 3.0f, 1.0f, 0.05f},
                {"dir", "Forgásirány / Direction", -1.0f, 1.0f, 1.0f, 0.1f},
                {"zoom", "Nagyítás / Zoom", 0.5f, 2.5f, 1.0f, 0.05f},
                {"center_x", "Középpont X", 0.0f, 1.0f, 0.5f, 0.01f},
                {"center_y", "Középpont Y", 0.0f, 1.0f, 0.5f, 0.01f}
            }
        },
        {
            2, "Halszem Optika (Fisheye Lens)", "Dinamikus Lencsetorzulás",
            {
                {"distortion", "Görbület / Distortion", 0.1f, 4.0f, 0.9f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.5f, 3.0f, 1.0f, 0.05f},
                {"vignette", "Peremsötétedés / Vignette", 0.0f, 2.0f, 0.0f, 0.05f},
                {"center_x", "Középpont X", 0.0f, 1.0f, 0.5f, 0.01f},
                {"center_y", "Középpont Y", 0.0f, 1.0f, 0.5f, 0.01f}
            }
        },
        {
            3, "RGB Split (Chromatic Aberration)", "Színcsatornák Szétválása",
            {
                {"distance", "Szétválás / Split Dist", 0.005f, 0.15f, 0.035f, 0.002f},
                {"angle", "Szög / Angle", 0.0f, 6.28f, 0.0f, 0.05f},
                {"green_shift", "Zöld Eltolás / Green Shift", -0.05f, 0.05f, 0.0f, 0.002f},
                {"blur", "Színfoltosság / Aberr Blur", 0.0f, 1.0f, 0.0f, 0.05f},
                {"sat_boost", "Színtelítettség / Saturation", 0.5f, 2.5f, 1.0f, 0.05f}
            }
        },
        {
            4, "Radial Zoom Blur (Suhanó nagyítás)", "Sugárirányú Sebességelmosás",
            {
                {"amount", "Elmosás Erőssége / Amount", 0.005f, 0.12f, 0.035f, 0.002f},
                {"samples", "Mintavétel / Samples", 2.0f, 8.0f, 6.0f, 1.0f},
                {"center_x", "Középpont X", 0.0f, 1.0f, 0.5f, 0.01f},
                {"center_y", "Középpont Y", 0.0f, 1.0f, 0.5f, 0.01f},
                {"decay", "Csillapodás / Falloff", 0.2f, 2.0f, 1.0f, 0.05f},
                {"glow", "Ragyogás / Glow", 0.0f, 2.0f, 0.0f, 0.05f}
            }
        },
        {
            5, "Pixelesedés (Pixelate / Mosaic)", "Mozaik Rács és Kvantálás",
            {
                {"max_grid", "Max Felbontás / Max Grid", 40.0f, 320.0f, 160.0f, 5.0f},
                {"min_grid", "Min Felbontás / Min Grid", 4.0f, 40.0f, 16.0f, 2.0f},
                {"aspect", "Képarány / Aspect", 0.2f, 2.0f, 0.5625f, 0.05f},
                {"grid_border", "Rácskeret / Grid Border", 0.0f, 0.3f, 0.0f, 0.01f},
                {"color_quant", "Színlépcsőzés / Color Steps", 0.0f, 16.0f, 0.0f, 1.0f}
            }
        },
        {
            6, "Luma Dissolve (Fényerő áttűnés)", "Fényesség Feloszlás & Parázs",
            {
                {"softness", "Lágyság / Softness", 0.01f, 0.40f, 0.12f, 0.01f},
                {"glow", "Parázs / Glow Intensity", 0.0f, 3.0f, 1.4f, 0.05f},
                {"glow_r", "Parázs Szín Vörös / Glow R", 0.0f, 1.0f, 1.0f, 0.02f},
                {"glow_g", "Parázs Szín Zöld / Glow G", 0.0f, 1.0f, 0.75f, 0.02f},
                {"glow_b", "Parázs Szín Kék / Glow B", 0.0f, 1.0f, 0.30f, 0.02f},
                {"invert", "Irány Invertálás / Invert", 0.0f, 1.0f, 0.0f, 1.0f}
            }
        },
        {
            7, "Folyadék Hullámzás (Liquid Ripple Warp)", "Folyadék Hullám és Törés",
            {
                {"wave_count", "Hullámsűrűség / Frequency", 5.0f, 60.0f, 28.0f, 1.0f},
                {"speed", "Hullámsebesség / Speed", 1.0f, 25.0f, 8.0f, 0.5f},
                {"amplitude", "Hullámmagasság / Amplitude", 0.005f, 0.10f, 0.035f, 0.002f},
                {"aspect_x", "Horizontális Arány / Aspect", 0.5f, 2.5f, 1.7778f, 0.05f},
                {"decay", "Csillapodás / Decay", 0.05f, 1.0f, 0.25f, 0.02f},
                {"surge", "Ugrási Lökés / Surge", 0.0f, 20.0f, 10.0f, 0.5f}
            }
        },
        {
            8, "Kaleidoszkóp (Mirror Fold)", "Többtükrös Poláris Tükrözés",
            {
                {"sides", "Tükrök Száma / Mirrors (Sides)", 2.0f, 16.0f, 6.0f, 1.0f},
                {"rot_speed", "Forgási Sebesség / Speed", 0.0f, 5.0f, 1.0f, 0.1f},
                {"zoom", "Nagyítás / Zoom", 0.3f, 3.0f, 1.0f, 0.05f},
                {"fold_angle", "Hajtási Szög / Angle", 0.2f, 3.14f, 1.5708f, 0.05f},
                {"center_x", "Középpont X", 0.0f, 1.0f, 0.5f, 0.01f},
                {"center_y", "Középpont Y", 0.0f, 1.0f, 0.5f, 0.01f},
                {"blend", "Keverés Erőssége / Blend", 0.2f, 2.0f, 1.25f, 0.05f}
            }
        },
        {
            9, "Glitch Displacement (Zajelcsúszás)", "Digitális Adáshiba és Rángás",
            {
                {"bands", "Sávok Száma / Band Count", 8.0f, 120.0f, 36.0f, 2.0f},
                {"speed", "Sebesség / Jitter Speed", 2.0f, 50.0f, 18.0f, 1.0f},
                {"shift", "Elcsúszás Mértéke / Shift", 0.01f, 0.25f, 0.05f, 0.005f},
                {"jitter_freq", "Gyakoriság / Frequency", 0.1f, 0.9f, 0.4f, 0.05f},
                {"color_split", "Szín Szétcsúszás / RGB Drift", 0.0f, 0.05f, 0.01f, 0.002f}
            }
        },
        {
            10, "CRT / VHS Scanline (Analóg TV)", "Katódcsöves TV & Képsorok",
            {
                {"density", "Sorok Sűrűsége / Scanlines", 100.0f, 1080.0f, 540.0f, 10.0f},
                {"intensity", "Sorok Erőssége / Line Darken", 0.0f, 1.0f, 0.35f, 0.02f},
                {"curvature", "Képernyő Görbület / Curve", 0.0f, 1.0f, 0.35f, 0.02f},
                {"flash", "CRT Villánás / Tube Flash", 0.0f, 2.0f, 1.0f, 0.05f},
                {"vignette", "Peremsötét / Edge Fade", 0.0f, 1.0f, 0.3f, 0.02f}
            }
        },
        {
            11, "Pop-Art Halftone (Képregény Pontrács)", "Nyomdai Rácspontok & Kvantálás",
            {
                {"grid_x", "Rácssűrűség X / Grid X", 20.0f, 200.0f, 96.0f, 4.0f},
                {"grid_y", "Rácssűrűség Y / Grid Y", 10.0f, 120.0f, 54.0f, 2.0f},
                {"dot_size", "Pontméret / Dot Size", 0.1f, 1.0f, 0.48f, 0.02f},
                {"softness", "Pontok Lágysága / Softness", 0.01f, 0.20f, 0.06f, 0.005f},
                {"posterize", "Színkvantálás / Color Levels", 1.0f, 8.0f, 3.5f, 0.5f},
                {"brightness", "Fényerőkiemelés / Brightness", 0.8f, 2.5f, 1.35f, 0.05f}
            }
        },
        {
            12, "Neon Kontúr (Sobel Edge Glow)", "Éldetektált Színes Neon Ragyogás",
            {
                {"thickness", "Él Vastagság / Thickness", 0.5f, 5.0f, 1.5f, 0.1f},
                {"glow", "Parázslás / Glow Intensity", 0.5f, 4.0f, 1.8f, 0.05f},
                {"neon_color", "Színtelítettség / Saturation", 0.5f, 3.0f, 1.5f, 0.05f},
                {"threshold", "Küszöb / Edge Sensitivity", 0.0f, 0.5f, 0.05f, 0.01f},
                {"blend_base", "Alap Videó Arány / Base Blend", 0.0f, 1.0f, 0.20f, 0.02f}
            }
        }
    };
    return defs;
}

inline float computeCrossfadeEffectiveParam(const CrossfadeFxParamDef &def, float base_val, int mod_mode,
                                            float bass_energy, float macro_tonus = 0.5f)
{
    float val = base_val;
    const float span = def.max_val - def.min_val;
    if (mod_mode == 1) {
        // 🥁 Kick Pulzus (Beat pulse)
        val += (bass_energy - 0.20f) * (span * 0.40f);
    } else if (mod_mode == 2) {
        // 📈 Zenei Tónus (Macro-Tonus build-up)
        val += (macro_tonus - 0.50f) * (span * 0.55f);
    } else if (mod_mode == 3) {
        // ⚡ Mindkettő: Beat + Macro-Tonus
        val += (bass_energy - 0.20f) * (span * 0.25f) + (macro_tonus - 0.50f) * (span * 0.35f);
    }
    return std::clamp(val, def.min_val, def.max_val);
}
