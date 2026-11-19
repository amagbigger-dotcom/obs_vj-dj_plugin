#include "effects/custom_shader_manager.hpp"
#include <obs.h>
#include <graphics/graphics.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace {

// Built-in 15 shaders definitions baseline
const std::vector<VjShaderDef> &getBuiltInShaderDefinitions()
{
    static const std::vector<VjShaderDef> built_ins = {
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
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.2f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 3.0f, 1.0f, 0.05f},
                {"lines", "Vonal Sűrűség / Lines", 4.0f, 32.0f, 12.0f, 1.0f},
                {"twist", "Csavarás / Twist", -3.14f, 3.14f, 0.5f, 0.05f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.05f},
                {"glow", "Neon Fényerő / Glow", 0.2f, 3.0f, 1.0f, 0.05f},
                {"fade", "Elhalványulás / Fade", 0.1f, 2.0f, 0.9f, 0.05f},
                {"speed2", "Másodlagos Forgás / Speed 2", -2.0f, 2.0f, 0.3f, 0.05f}
            }
        },
        {
            5, "Dodecahedron 3D", "Raymarching Kristály",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.5f, 4.0f, 1.8f, 0.05f},
                {"explode", "Szétnyílás / Explode", 0.0f, 2.0f, 0.2f, 0.02f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"refract", "Fénytörés / Refract", 0.0f, 1.0f, 0.4f, 0.02f},
                {"glow", "Kristály Izzás / Glow", 0.2f, 3.0f, 1.2f, 0.05f},
                {"rot_x", "X Forgás / Rot X", -2.0f, 2.0f, 0.4f, 0.05f},
                {"rot_y", "Y Forgás / Rot Y", -2.0f, 2.0f, 0.6f, 0.05f}
            }
        },
        {
            6, "Kali Circuits", "Fraktál Áramkörök",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 4.0f, 1.5f, 0.05f},
                {"iter", "Iteráció / Iter", 4.0f, 18.0f, 11.0f, 1.0f},
                {"param_a", "Áramkör Ív A / Curve A", 0.1f, 2.0f, 0.75f, 0.02f},
                {"param_b", "Áramkör Ív B / Curve B", 0.1f, 2.0f, 0.85f, 0.02f},
                {"pulse", "Zenei Lüktetés / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"glow", "Vezeték Izzás / Glow", 0.2f, 3.0f, 1.0f, 0.05f},
                {"color_shift", "Színtolás / Color Shift", 0.0f, 1.0f, 0.0f, 0.02f}
            }
        },
        {
            7, "Diamond Vision", "36-Lépéses Tórusz Sugárkövetés",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.5f, 4.0f, 2.2f, 0.05f},
                {"torus_r", "Tórusz Sugár / Radius", 0.2f, 2.0f, 0.8f, 0.02f},
                {"twist", "Sodrás / Twist", -3.14f, 3.14f, 1.0f, 0.05f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"specular", "Csillogás / Specular", 0.1f, 3.0f, 1.2f, 0.05f},
                {"glow", "Neon Aura / Glow", 0.2f, 3.0f, 1.0f, 0.05f},
                {"density", "Sűrűség / Density", 1.0f, 10.0f, 4.0f, 0.5f}
            }
        },
        {
            8, "Pupila Eye", "Fotorealisztikus Szem & Írisz",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"pupil", "Pupilla Tágulás / Pupil", 0.1f, 0.9f, 0.35f, 0.02f},
                {"fibers", "Írisz Rostok / Fibers", 2.0f, 20.0f, 8.0f, 0.5f},
                {"pulse", "Lüktetés / Pulse", 0.0f, 2.0f, 0.6f, 0.02f},
                {"ring", "Peremfény / Ring", 0.1f, 2.0f, 0.8f, 0.05f},
                {"glow", "Izzás / Glow", 0.2f, 3.0f, 1.0f, 0.05f},
                {"twist", "Írisz Csavarás / Twist", -2.0f, 2.0f, 0.2f, 0.05f},
                {"noise", "Zaj Réteg / Noise", 0.0f, 1.0f, 0.3f, 0.02f}
            }
        },
        {
            9, "Mandala Scope", "72-Lépéses Fraktál Kaleidoszkóp",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 4.0f, 1.2f, 0.05f},
                {"petals", "Szirmok Száma / Petals", 3.0f, 16.0f, 8.0f, 1.0f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"rot_speed", "Belső Fázis / Phase", -2.0f, 2.0f, 0.0f, 0.05f},
                {"detail", "Részletesség / Detail", 1.0f, 8.0f, 4.0f, 0.5f},
                {"glow", "Mandala Izzás / Glow", 0.2f, 3.0f, 1.0f, 0.05f},
                {"contrast", "Kontraszt / Contrast", 0.5f, 2.5f, 1.2f, 0.05f}
            }
        },
        {
            10, "Searching Light", "Kiber Fényalagút & Sugarak",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.2f, 0.05f},
                {"tunnel_w", "Alagút Szélesség / Width", 0.2f, 3.0f, 1.0f, 0.05f},
                {"rays", "Fénysugarak / Rays", 4.0f, 32.0f, 16.0f, 1.0f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.6f, 0.02f},
                {"speed_z", "Z-Áramlás / Speed Z", -3.0f, 3.0f, 1.5f, 0.05f},
                {"glow", "Sugarak Ragyogása / Glow", 0.2f, 3.0f, 1.2f, 0.05f},
                {"fade", "Mélységi Elmosás / Fade", 0.1f, 2.0f, 0.8f, 0.05f},
                {"density", "Sugársűrűség / Density", 0.5f, 3.0f, 1.0f, 0.05f}
            }
        },
        {
            11, "Cube Field 3D", "Végtelen Kockamező Raymarching",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"spacing", "Kocka Távolság / Spacing", 0.5f, 3.0f, 1.4f, 0.05f},
                {"height", "Kocka Magasság / Height", 0.1f, 2.5f, 0.8f, 0.02f},
                {"pulse", "Zenei Lüktetés / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"cam_h", "Kamera Magasság / Cam H", 0.2f, 4.0f, 1.5f, 0.05f},
                {"glow", "Élvilágítás / Edge Glow", 0.2f, 3.0f, 1.2f, 0.05f},
                {"speed_z", "Előre Haladás / Speed Z", -3.0f, 3.0f, 1.2f, 0.05f},
                {"shadows", "Árnyék Kontraszt / Shadows", 0.1f, 1.0f, 0.7f, 0.02f}
            }
        },
        {
            12, "Electro Bands", "18-Lépéses Kvantum Spektrum",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 4.0f, 1.0f, 0.05f},
                {"bands", "Sávok Száma / Bands", 4.0f, 32.0f, 14.0f, 1.0f},
                {"thickness", "Vonal Vastagság / Thickness", 0.005f, 0.1f, 0.025f, 0.002f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"glow", "Neon Fényerő / Glow", 0.2f, 3.0f, 1.1f, 0.05f},
                {"dispersion", "Frekvencia Szórás / Dispersion", 0.1f, 2.0f, 0.8f, 0.05f},
                {"phase", "Belső Fázis / Phase", -3.14f, 3.14f, 0.0f, 0.05f}
            }
        },
        {
            13, "Globular Cluster", "Kvantum Csillagköd Raymarching",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 4.0f, 1.5f, 0.05f},
                {"stars", "Csillag Sűrűség / Density", 10.0f, 100.0f, 45.0f, 2.0f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.6f, 0.02f},
                {"core_r", "Mag Sugara / Core Radius", 0.1f, 2.0f, 0.6f, 0.02f},
                {"glow", "Gázköd Izzás / Nebula Glow", 0.2f, 3.0f, 1.3f, 0.05f},
                {"rot_speed", "Ködfelhő Forgás / Rot Speed", -2.0f, 2.0f, 0.4f, 0.05f},
                {"dispersion", "Szín Tágulás / Color Dispersion", 0.0f, 1.0f, 0.3f, 0.02f}
            }
        },
        {
            14, "Hex Truch Warp", "Hexagonális Warp Raymarching",
            {
                {"rate", "Sebesség / Rate", 0.1f, 3.0f, 1.0f, 0.05f},
                {"zoom", "Nagyítás / Zoom", 0.2f, 4.0f, 1.6f, 0.05f},
                {"warp", "Térhajlítás / Warp", 0.0f, 2.0f, 0.7f, 0.02f},
                {"pulse", "Zenei Pulzus / Pulse", 0.0f, 2.0f, 0.5f, 0.02f},
                {"thickness", "Hex Fal Vastagság / Thickness", 0.01f, 0.2f, 0.05f, 0.005f},
                {"glow", "Hex Ragyogás / Glow", 0.2f, 3.0f, 1.0f, 0.05f},
                {"speed_z", "Z-Áramlás / Speed Z", -2.0f, 2.0f, 0.6f, 0.05f},
                {"contrast", "Kontraszt / Contrast", 0.5f, 2.5f, 1.2f, 0.05f}
            }
        }
    };
    return built_ins;
}

} // namespace

CustomShaderManager &CustomShaderManager::instance()
{
    static CustomShaderManager s_mgr;
    if (!s_mgr.is_loaded_) {
        s_mgr.loadCustomShaders();
        s_mgr.is_loaded_ = true;
    }
    return s_mgr;
}

CustomShaderManager::CustomShaderManager()
{
    rebuildCombinedDefinitions();
}

QString CustomShaderManager::getConfigFilePath() const
{
    QString app_data = QString::fromLocal8Bit(qgetenv("APPDATA"));
    if (app_data.isEmpty()) {
        app_data = QDir::homePath() + QStringLiteral("/.config");
    }
    const QString target_dir = QDir(app_data).filePath(QStringLiteral("obs-studio/plugin_config/obs_vj_dj_plugin"));
    QDir().mkpath(target_dir);
    return QDir(target_dir).filePath(QStringLiteral("custom_shaders.json"));
}

void CustomShaderManager::loadCustomShaders()
{
    custom_shaders_.clear();
    const QString path = getConfigFilePath();
    if (!QFile::exists(path)) {
        rebuildCombinedDefinitions();
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        blog(LOG_WARNING, "[custom_shaders] Failed to open config file: %s", path.toUtf8().constData());
        rebuildCombinedDefinitions();
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parse_err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isArray()) {
        blog(LOG_WARNING, "[custom_shaders] JSON parse error: %s", parse_err.errorString().toUtf8().constData());
        rebuildCombinedDefinitions();
        return;
    }

    const QJsonArray arr = doc.array();
    for (int i = 0; i < arr.size(); ++i) {
        if (!arr[i].isObject()) continue;
        const QJsonObject obj = arr[i].toObject();

        CustomShaderEntry entry;
        entry.id = obj.value(QStringLiteral("id")).toString(QStringLiteral("custom_%1").arg(i)).toStdString();
        entry.name = obj.value(QStringLiteral("name")).toString(QStringLiteral("Custom Shader %1").arg(i + 1)).toStdString();
        entry.category = obj.value(QStringLiteral("category")).toString(QStringLiteral("ISF / Custom")).toStdString();
        entry.hlsl_function_name = std::string("render_custom_") + std::to_string(i);
        entry.raw_user_code = obj.value(QStringLiteral("raw_user_code")).toString().toStdString();
        entry.hlsl_code = obj.value(QStringLiteral("hlsl_code")).toString().toStdString();

        const QJsonArray p_arr = obj.value(QStringLiteral("params")).toArray();
        entry.param_names.resize(8);
        entry.param_ids.resize(8);
        entry.params.resize(8);

        for (int p = 0; p < 8; ++p) {
            if (p < p_arr.size() && p_arr[p].isObject()) {
                const QJsonObject p_obj = p_arr[p].toObject();
                entry.param_ids[p] = p_obj.value(QStringLiteral("id")).toString(QStringLiteral("p%1").arg(p)).toStdString();
                entry.param_names[p] = p_obj.value(QStringLiteral("name")).toString(QStringLiteral("Param %1").arg(p + 1)).toStdString();
                const float min_v = static_cast<float>(p_obj.value(QStringLiteral("min")).toDouble(0.0));
                const float max_v = static_cast<float>(p_obj.value(QStringLiteral("max")).toDouble(2.0));
                const float def_v = static_cast<float>(p_obj.value(QStringLiteral("def")).toDouble(1.0));
                const float step_v = static_cast<float>(p_obj.value(QStringLiteral("step")).toDouble(0.05));

                entry.params[p] = {
                    entry.param_ids[p].c_str(),
                    entry.param_names[p].c_str(),
                    min_v, max_v, def_v, step_v
                };
            } else {
                entry.param_ids[p] = "p" + std::to_string(p);
                entry.param_names[p] = "Param " + std::to_string(p + 1);
                entry.params[p] = {
                    entry.param_ids[p].c_str(),
                    entry.param_names[p].c_str(),
                    0.0f, 2.0f, 1.0f, 0.05f
                };
            }
        }

        custom_shaders_.push_back(std::move(entry));
    }

    rebuildCombinedDefinitions();
    blog(LOG_INFO, "[custom_shaders] Loaded %zu custom shaders from %s", custom_shaders_.size(), path.toUtf8().constData());
}

void CustomShaderManager::saveCustomShaders()
{
    const QString path = getConfigFilePath();
    QJsonArray arr;

    for (size_t i = 0; i < custom_shaders_.size(); ++i) {
        const auto &entry = custom_shaders_[i];
        QJsonObject obj;
        obj[QStringLiteral("id")] = QString::fromStdString(entry.id);
        obj[QStringLiteral("name")] = QString::fromStdString(entry.name);
        obj[QStringLiteral("category")] = QString::fromStdString(entry.category);
        obj[QStringLiteral("hlsl_code")] = QString::fromStdString(entry.hlsl_code);
        obj[QStringLiteral("raw_user_code")] = QString::fromStdString(entry.raw_user_code);

        QJsonArray p_arr;
        for (size_t p = 0; p < entry.params.size() && p < 8; ++p) {
            QJsonObject p_obj;
            p_obj[QStringLiteral("id")] = QString::fromStdString(entry.param_ids[p]);
            p_obj[QStringLiteral("name")] = QString::fromStdString(entry.param_names[p]);
            p_obj[QStringLiteral("min")] = entry.params[p].min_val;
            p_obj[QStringLiteral("max")] = entry.params[p].max_val;
            p_obj[QStringLiteral("def")] = entry.params[p].def_val;
            p_obj[QStringLiteral("step")] = entry.params[p].step;
            p_arr.append(p_obj);
        }
        obj[QStringLiteral("params")] = p_arr;
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        blog(LOG_INFO, "[custom_shaders] Saved %zu custom shaders to %s", custom_shaders_.size(), path.toUtf8().constData());
    } else {
        blog(LOG_ERROR, "[custom_shaders] Failed to save custom shaders to: %s", path.toUtf8().constData());
    }
}

void CustomShaderManager::rebuildCombinedDefinitions()
{
    combined_defs_ = getBuiltInShaderDefinitions();

    for (size_t i = 0; i < custom_shaders_.size(); ++i) {
        auto &cs = custom_shaders_[i];
        // Re-wire parameter pointers to guarantee memory stability
        for (size_t p = 0; p < cs.params.size() && p < 8; ++p) {
            cs.params[p].id = cs.param_ids[p].c_str();
            cs.params[p].name = cs.param_names[p].c_str();
        }

        VjShaderDef def;
        def.id = static_cast<int>(15 + i);
        def.name = cs.name.c_str();
        def.category = cs.category.c_str();
        def.params = cs.params;
        combined_defs_.push_back(def);
    }
}

const std::vector<VjShaderDef> &CustomShaderManager::getAllShaderDefinitions()
{
    return combined_defs_;
}

bool CustomShaderManager::testCompileShader(const std::string &hlsl_function_code, std::string &out_error)
{
    std::string test_source;
    test_source.reserve(16384);

    test_source += R"(
uniform float4x4 ViewProj;
uniform texture2d dummy_texture;
uniform float visualizer_mode;
uniform float anim_time;

#define TIME anim_time
#define time anim_time
#define iTime anim_time
#define shader_time anim_time
#define u_time anim_time
#define RENDERSIZE float2(1920.0, 1080.0)
#define iResolution float3(1920.0, 1080.0, 1.0)

uniform float bpm;
uniform float audio_amp;
uniform float bass_energy;
uniform float mid_energy;
uniform float high_energy;
uniform float is_deck_b;
uniform float4 custom_theme_color;

float3 get_base_theme_color()
{
    float3 def_col = (is_deck_b > 0.5) ? float3(1.0, 0.46, 0.0) : float3(0.0, 0.85, 1.0);
    return lerp(def_col, custom_theme_color.rgb, saturate(custom_theme_color.a));
}

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

float glsl_mod(float x, float y) { return x - y * floor(x / y); }
float2 glsl_mod2(float2 x, float y) { return x - y * floor(x / y); }
float3 glsl_mod3(float3 x, float y) { return x - y * floor(x / y); }
float4 glsl_mod4(float4 x, float y) { return x - y * floor(x / y); }

float2 Rot2D_hlsl(float2 p, float a)
{
    float c = cos(a), s = sin(a);
    return float2(p.x * c - p.y * s, p.x * s + p.y * c);
}

float2 rotate2D(float2 p, float a)
{
    return Rot2D_hlsl(p, a);
}
)";

    test_source += "\n// --- USER SHADER TEST CODE ---\n";
    test_source += hlsl_function_code;
    test_source += "\n// --- END USER SHADER CODE ---\n";

    // Determine function to call
    std::string call_name = "render_custom";
    if (hlsl_function_code.find("render_custom(") == std::string::npos) {
        // Find if user provided another float4 func name
        const QRegularExpression re(QStringLiteral("float4\\s+([A-Za-z0-9_]+)\\s*\\(\\s*float2"));
        const auto match = re.match(QString::fromStdString(hlsl_function_code));
        if (match.hasMatch()) {
            call_name = match.captured(1).toStdString();
        }
    }

    test_source += R"(
float4 PSTest(VertData v) : TARGET
{
    return )" + call_name + R"((v.uv);
}

technique Draw
{
    pass
    {
        vertex_shader = VSDefault(v);
        pixel_shader  = PSTest(v);
    }
}
)";

    obs_enter_graphics();
    char *errors = nullptr;
    gs_effect_t *eff = gs_effect_create(test_source.c_str(), "custom_test_effect", &errors);
    if (eff) {
        gs_effect_destroy(eff);
        obs_leave_graphics();
        out_error.clear();
        return true;
    }

    if (errors && strlen(errors) > 0) {
        out_error = errors;
        bfree(errors);
    } else {
        out_error = "Direct3D 11 fordítási hiba: a shader nem fordult le. Ellenőrizd a szintaktikát és a zárójeleket!";
    }
    obs_leave_graphics();
    return false;
}

bool CustomShaderManager::parseAiShaderText(const std::string &text,
                                            std::string &out_name,
                                            std::string &out_category,
                                            std::vector<VjParamDef> &out_params,
                                            std::vector<std::string> &out_param_names,
                                            std::vector<std::string> &out_param_ids,
                                            std::string &out_hlsl_code,
                                            std::string &out_error)
{
    const QString qtext = QString::fromStdString(text);

    // 1. Try Tag-based parsing: [NAME], [CATEGORY], [PARAMS], [CODE]
    if (qtext.contains(QStringLiteral("[NAME]"), Qt::CaseInsensitive) ||
        qtext.contains(QStringLiteral("[CODE]"), Qt::CaseInsensitive)) {

        // Extract [NAME]
        const QRegularExpression name_re(QStringLiteral("\\[NAME\\]\\s*\\r?\\n([^\\r\\n\\[]+)"), QRegularExpression::CaseInsensitiveOption);
        const auto match_name = name_re.match(qtext);
        out_name = match_name.hasMatch() ? match_name.captured(1).trimmed().toStdString() : "Új ISF Shader";

        // Extract [CATEGORY]
        const QRegularExpression cat_re(QStringLiteral("\\[CATEGORY\\]\\s*\\r?\\n([^\\r\\n\\[]+)"), QRegularExpression::CaseInsensitiveOption);
        const auto match_cat = cat_re.match(qtext);
        out_category = match_cat.hasMatch() ? match_cat.captured(1).trimmed().toStdString() : "ISF / Procedural";

        // Extract [CODE]
        int code_idx = qtext.indexOf(QStringLiteral("[CODE]"), 0, Qt::CaseInsensitive);
        if (code_idx >= 0) {
            QString code_str = qtext.mid(code_idx + 6).trimmed();
            // Remove markdown code fences if present (```hlsl ... ```)
            if (code_str.startsWith(QStringLiteral("```"))) {
                int first_nl = code_str.indexOf(QLatin1Char('\n'));
                if (first_nl >= 0) code_str = code_str.mid(first_nl + 1);
            }
            if (code_str.endsWith(QStringLiteral("```"))) {
                code_str.chop(3);
                code_str = code_str.trimmed();
            }
            out_hlsl_code = code_str.toStdString();
        } else {
            out_error = "Nem található a [CODE] szekció az AI szövegben!";
            return false;
        }

        // Extract [PARAMS]
        out_params.clear();
        out_param_names.clear();
        out_param_ids.clear();
        out_params.resize(8);
        out_param_names.resize(8);
        out_param_ids.resize(8);

        int params_idx = qtext.indexOf(QStringLiteral("[PARAMS]"), 0, Qt::CaseInsensitive);
        if (params_idx >= 0) {
            int end_params = code_idx > params_idx ? code_idx : qtext.length();
            QString params_block = qtext.mid(params_idx + 8, end_params - (params_idx + 8)).trimmed();
            QStringList lines = params_block.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);

            int p_count = 0;
            for (const QString &line : lines) {
                if (p_count >= 8) break;
                if (!line.contains(QLatin1Char('|'))) continue;
                QStringList parts = line.split(QLatin1Char('|'));
                if (parts.size() >= 2) {
                    out_param_ids[p_count] = parts[0].trimmed().toStdString();
                    out_param_names[p_count] = parts[1].trimmed().toStdString();
                    float min_v = parts.size() > 2 ? parts[2].trimmed().toFloat() : 0.0f;
                    float max_v = parts.size() > 3 ? parts[3].trimmed().toFloat() : 2.0f;
                    float def_v = parts.size() > 4 ? parts[4].trimmed().toFloat() : 1.0f;
                    float step_v = parts.size() > 5 ? parts[5].trimmed().toFloat() : 0.05f;

                    out_params[p_count] = {
                        nullptr, nullptr, min_v, max_v, def_v, step_v
                    };
                    p_count++;
                }
            }

            // Fill remainder with defaults if fewer than 8 specified
            for (int p = p_count; p < 8; ++p) {
                out_param_ids[p] = "p" + std::to_string(p);
                out_param_names[p] = "Paraméter " + std::to_string(p + 1);
                out_params[p] = { nullptr, nullptr, 0.0f, 2.0f, 1.0f, 0.05f };
            }
        } else {
            // Default 8 params
            for (int p = 0; p < 8; ++p) {
                out_param_ids[p] = "p" + std::to_string(p);
                out_param_names[p] = "Paraméter " + std::to_string(p + 1);
                out_params[p] = { nullptr, nullptr, 0.0f, 2.0f, 1.0f, 0.05f };
            }
        }

        return true;
    }

    // 2. Try JSON parsing
    QJsonParseError j_err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(text), &j_err);
    if (j_err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        out_name = obj.value(QStringLiteral("name")).toString(QStringLiteral("Custom ISF Shader")).toStdString();
        out_category = obj.value(QStringLiteral("category")).toString(QStringLiteral("ISF / Custom")).toStdString();
        out_hlsl_code = obj.value(QStringLiteral("code")).toString().toStdString();

        out_params.resize(8);
        out_param_names.resize(8);
        out_param_ids.resize(8);

        QJsonArray p_arr = obj.value(QStringLiteral("params")).toArray();
        for (int p = 0; p < 8; ++p) {
            if (p < p_arr.size() && p_arr[p].isObject()) {
                QJsonObject p_obj = p_arr[p].toObject();
                out_param_ids[p] = p_obj.value(QStringLiteral("id")).toString(QStringLiteral("p%1").arg(p)).toStdString();
                out_param_names[p] = p_obj.value(QStringLiteral("name")).toString(QStringLiteral("Param %1").arg(p + 1)).toStdString();
                float min_v = static_cast<float>(p_obj.value(QStringLiteral("min")).toDouble(0.0));
                float max_v = static_cast<float>(p_obj.value(QStringLiteral("max")).toDouble(2.0));
                float def_v = static_cast<float>(p_obj.value(QStringLiteral("def")).toDouble(1.0));
                float step_v = static_cast<float>(p_obj.value(QStringLiteral("step")).toDouble(0.05));
                out_params[p] = { nullptr, nullptr, min_v, max_v, def_v, step_v };
            } else {
                out_param_ids[p] = "p" + std::to_string(p);
                out_param_names[p] = "Paraméter " + std::to_string(p + 1);
                out_params[p] = { nullptr, nullptr, 0.0f, 2.0f, 1.0f, 0.05f };
            }
        }
        return true;
    }

    // 3. Fallback: treat as raw HLSL code
    QString cleaned_code = qtext.trimmed();
    if (cleaned_code.startsWith(QStringLiteral("```"))) {
        int first_nl = cleaned_code.indexOf(QLatin1Char('\n'));
        if (first_nl >= 0) cleaned_code = cleaned_code.mid(first_nl + 1);
    }
    if (cleaned_code.endsWith(QStringLiteral("```"))) {
        cleaned_code.chop(3);
        cleaned_code = cleaned_code.trimmed();
    }

    if (cleaned_code.contains(QStringLiteral("float4")) && cleaned_code.contains(QStringLiteral("float2"))) {
        out_name = "Importált ISF Shader";
        out_category = "ISF / Web";
        out_hlsl_code = cleaned_code.toStdString();

        out_params.resize(8);
        out_param_names.resize(8);
        out_param_ids.resize(8);
        const char *def_names[8] = {
            "Sebesség / Rate", "Nagyítás / Zoom", "Részletesség / Detail", "Szimmetria / Symmetry",
            "Zenei Pulzus / Pulse", "Fényerő / Glow", "Színharmónia / Color", "Kontraszt / Contrast"
        };
        for (int p = 0; p < 8; ++p) {
            out_param_ids[p] = "p" + std::to_string(p);
            out_param_names[p] = def_names[p];
            out_params[p] = { nullptr, nullptr, 0.0f, 2.0f, 1.0f, 0.05f };
        }
        return true;
    }

    out_error = "A megadott szöveg nem tartalmaz érvényes HLSL shader kódot vagy szabványos [NAME]/[CODE] struktúrát.";
    return false;
}

bool CustomShaderManager::addOrUpdateCustomShader(const std::string &id,
                                                  const std::string &name,
                                                  const std::string &category,
                                                  const std::vector<VjParamDef> &params,
                                                  const std::vector<std::string> &param_names,
                                                  const std::vector<std::string> &param_ids,
                                                  const std::string &hlsl_code,
                                                  const std::string &raw_user_code,
                                                  std::string &out_id,
                                                  std::string &out_error)
{
    // 1. Live test compilation on GPU first!
    if (!testCompileShader(hlsl_code, out_error)) {
        return false;
    }

    int existing_idx = -1;
    if (!id.empty()) {
        for (size_t i = 0; i < custom_shaders_.size(); ++i) {
            if (custom_shaders_[i].id == id) {
                existing_idx = static_cast<int>(i);
                break;
            }
        }
    }

    const int target_idx = (existing_idx >= 0) ? existing_idx : static_cast<int>(custom_shaders_.size());
    const std::string assigned_id = (existing_idx >= 0) ? id : ("custom_" + std::to_string(target_idx));
    const std::string func_name = "render_custom_" + std::to_string(target_idx);

    // Transform code to unique function name render_custom_<idx>
    QString qcode = QString::fromStdString(hlsl_code);
    if (qcode.contains(QStringLiteral("float4 render_custom("))) {
        qcode.replace(QStringLiteral("float4 render_custom("),
                      QString::fromStdString("float4 " + func_name + "("));
    } else {
        // Replace whatever primary float4 func with func_name
        const QRegularExpression re(QStringLiteral("float4\\s+([A-Za-z0-9_]+)\\s*\\(\\s*float2"));
        const auto match = re.match(qcode);
        if (match.hasMatch()) {
            const QString old_fn = match.captured(1);
            qcode.replace(QStringLiteral("float4 ") + old_fn + QStringLiteral("("),
                          QString::fromStdString("float4 " + func_name + "("));
        }
    }

    CustomShaderEntry entry;
    entry.id = assigned_id;
    entry.name = name.empty() ? ("Custom Shader " + std::to_string(target_idx + 1)) : name;
    entry.category = category.empty() ? "ISF / Custom" : category;
    entry.param_names = param_names;
    entry.param_ids = param_ids;
    entry.params = params;
    entry.hlsl_function_name = func_name;
    entry.hlsl_code = qcode.toStdString();
    entry.raw_user_code = raw_user_code;

    // Safety: ensure exactly 8 params
    entry.param_names.resize(8);
    entry.param_ids.resize(8);
    entry.params.resize(8);
    for (int p = 0; p < 8; ++p) {
        if (entry.param_ids[p].empty()) entry.param_ids[p] = "p" + std::to_string(p);
        if (entry.param_names[p].empty()) entry.param_names[p] = "Param " + std::to_string(p + 1);
        entry.params[p].id = entry.param_ids[p].c_str();
        entry.params[p].name = entry.param_names[p].c_str();
    }

    if (existing_idx >= 0) {
        custom_shaders_[existing_idx] = std::move(entry);
    } else {
        custom_shaders_.push_back(std::move(entry));
    }

    rebuildCombinedDefinitions();
    saveCustomShaders();
    incrementVersion();

    out_id = assigned_id;
    out_error.clear();
    return true;
}

bool CustomShaderManager::deleteCustomShader(const std::string &id)
{
    auto it = std::find_if(custom_shaders_.begin(), custom_shaders_.end(),
                           [&id](const CustomShaderEntry &e) { return e.id == id; });
    if (it == custom_shaders_.end()) {
        return false;
    }

    custom_shaders_.erase(it);

    // Re-index remaining shaders so their function names match their array index
    for (size_t i = 0; i < custom_shaders_.size(); ++i) {
        auto &cs = custom_shaders_[i];
        const std::string old_fn = cs.hlsl_function_name;
        const std::string new_fn = "render_custom_" + std::to_string(i);
        cs.hlsl_function_name = new_fn;
        QString qcode = QString::fromStdString(cs.hlsl_code);
        qcode.replace(QString::fromStdString(old_fn), QString::fromStdString(new_fn));
        cs.hlsl_code = qcode.toStdString();
    }

    rebuildCombinedDefinitions();
    saveCustomShaders();
    incrementVersion();
    return true;
}

std::string CustomShaderManager::getCustomShadersHlslDeclarations() const
{
    std::string decls;
    if (custom_shaders_.empty()) return decls;

    decls += "\n// =============================================================================\n";
    decls += "// DYNAMIC USER CUSTOM ISF SHADERS\n";
    decls += "// =============================================================================\n";

    for (const auto &cs : custom_shaders_) {
        decls += "\n// Shader: " + cs.name + " (" + cs.category + ")\n";
        decls += cs.hlsl_code;
        decls += "\n";
    }

    return decls;
}

std::string CustomShaderManager::getCustomShadersDispatchHlsl() const
{
    std::string dispatch;
    for (size_t i = 0; i < custom_shaders_.size(); ++i) {
        const int mode_id = static_cast<int>(15 + i);
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<float>(mode_id) + 0.5f);
        dispatch += "    } else if (visualizer_mode < " + ss.str() + ") {\n";
        dispatch += "        return " + custom_shaders_[i].hlsl_function_name + "(v.uv);\n";
    }
    return dispatch;
}

const QString &CustomShaderManager::getAiPromptTemplate()
{
    static const QString s_prompt = QStringLiteral(
R"(Te egy professzionális HLSL/GLSL Shader Fejlesztő AI vagy.
A feladatod: a mellékelt ISF (Interactive Shader Format), GLSL vagy ShaderToy shader kódot átültetni egy Direct3D 11 kompatibilis HLSL renderelő függvénnyé a 2-Deckes OBS VJ/DJ Plugin számára.

A válaszodat KIZÁRÓLAG a következő szabványos blokkokban add meg, minden egyéb bevezető vagy magyarázó szöveg nélkül:

[NAME]
<Kifejező név magyarul vagy angolul, pl. Cyber Tunnel 3D>

[CATEGORY]
ISF / <Stílus: pl. Raymarching, Tunnel, Psychedelic, Geometry, Fluid, Minimal>

[PARAMS]
p0 | <Paraméter 0 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>
p1 | <Paraméter 1 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>
p2 | <Paraméter 2 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>
p3 | <Paraméter 3 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>
p4 | <Paraméter 4 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>
p5 | <Paraméter 5 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>
p6 | <Paraméter 6 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>
p7 | <Paraméter 7 neve> | <Min> | <Max> | <Alapértelmezett> | <Lépésköz>

[CODE]
float4 render_custom(float2 uv)
{
    // Ide jön a Direct3D 11 HLSL kód
    return float4(saturate(col), 1.0);
}

KÖTELEZŐ SZABÁLYOK ÉS IRÁNYELVEK:
1. TÖKÉLETES SZIMMETRIA ÉS 16:9 KÉPARÁNY:
   - A vászon UV koordinátáit mindig így alakítsd át középpontos 16:9 koordinátákra:
     float2 uvn = (uv - 0.5) * float2(16.0 / 9.0, 1.0);
   - A koordinátarendszer origója (0.0, 0.0) MINDIG a képernyő mértani közepe.
   - TILOS aszimmetrikus középpont-eltolást, koordináta-pásztázást vagy elcsúszó dőlésszögeket használni! Kizárólag szimmetrikus, középpontos vizuális formákat és sugarakat hozz létre.
   - Forgás helyett belső fázis- és alaktolást használj, vagy ha van szögelfordulás, az legyen finom lüktetés vagy oda-vissza szimmetrikus.

2. MÉLY FEKETE HÁTTÉR ÉS MAGAS KONTRASZT (KÖD- ÉS TELÍTŐDÉSVÉDELEM):
   - TILOS a teljes képet ködös, tejszerű, szürke vagy egyszínű fehér fátyollal elárasztani! A háttérnek mély, tiszta feketének kell maradnia (0.0, 0.0, 0.0).
   - Alkalmazz finom zajszűrést és vágást a háttérzajra:
     col = smoothstep(0.015, 1.0, col);
   - A színek legyenek élénkek, neonosak és kontrasztosak, ne mosódjanak össze pasztell köddé.

3. ZENEI LÜKTETÉS ÉS TRANZIENS REAKCIÓ (LÁBDOB / BASS):
   - Használd a beépített 'bass_energy' változót (0.0..1.0 lábdob lüktetés) a formák méretének, ragyogásának vagy reakciójának vezérlésére, de CSAK ott, ahol van képi tartalom (pl. col *= (1.0 + bass_energy * p4 * 0.5);). Így a fekete háttér a lábdobra sem világosodik ki, csak az elemek pulzálnak!
   - Rendelkezésre álló beépített változók és aliasok:
     * float anim_time: másodperc alapú folyamatos futásidő (a TIME, time, iTime, u_time aliasok is automatikusan működnek!)
     * float2 RENDERSIZE vagy float3 iResolution: (1920.0, 1080.0) vászonfelbontás
     * float bpm: a deck zenei tempója
     * float bass_energy: lábdob / mélyhang lüktetés (0.0 .. 1.0)
     * float mid_energy: középtartomány energia (0.0 .. 1.0)
     * float high_energy: magas hangok / cintányér energia (0.0 .. 1.0)
     * float audio_amp: összegzett pillanatnyi hangerő (0.0 .. 1.0)
     * float3 theme = get_base_theme_color(): a deck kék/narancs témaszíne

4. DECK SZÍNTÉMA INTEGRÁCIÓ:
   - float3 theme = get_base_theme_color();
   - Ez automatikusan biztosítja a Deck A ciánkék vagy Deck B narancs (vagy a felhasználó által választott egyedi) színvilágát. Szorozd fel vagy színezd ezzel a geometrikus elemeket.

5. PONTOSAN 8 DARAB KIFORDÍTOTT PARAMÉTER (p0 .. p7):
   - p0: Fő Sebesség / Rate (pl. 0.1 .. 4.0, def: 1.0, step: 0.05)
   - p1: Nagyítás / Zoom vagy Skála (pl. 0.2 .. 5.0, def: 1.0, step: 0.05)
   - p2: Részletesség / Alak / Sugár sűrűség (pl. 1.0 .. 10.0, def: 3.0, step: 0.5)
   - p3: Szimmetria / Tükrözés / Fokok (pl. 2.0 .. 16.0, def: 6.0, step: 1.0)
   - p4: Zenei Pulzus Érzékenység (pl. 0.0 .. 2.0, def: 0.5, step: 0.05)
   - p5: Fényerő / Neon Ragyogás (pl. 0.1 .. 3.0, def: 1.0, step: 0.05)
   - p6: Színharmónia / Színeltolás (pl. 0.0 .. 1.0, def: 0.0, step: 0.02)
   - p7: Másodlagos Alakváltozás / Kontraszt (pl. 0.5 .. 3.0, def: 1.2, step: 0.05)

6. HLSL SZINTAKTIKA (Direct3D 11):
   - float2, float3, float4 (NEM vec2, vec3, vec4)
   - lerp(a, b, t) (NEM mix)
   - frac(x) (NEM fract)
   - atan2(y, x) (NEM atan(y, x))
   - glsl_mod(x, y), glsl_mod2, glsl_mod3, glsl_mod4, Rot2D_hlsl elérhető
   - saturate(x) használata
   - A fő függvény neve MINDIG: float4 render_custom(float2 uv)
   - Visszatérési érték: float4(saturate(col), 1.0)
)");
    return s_prompt;
}

const std::vector<VjShaderDef>& getVjShaderDefinitions()
{
    return CustomShaderManager::instance().getAllShaderDefinitions();
}
