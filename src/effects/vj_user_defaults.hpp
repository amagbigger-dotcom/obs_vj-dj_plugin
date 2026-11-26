#pragma once

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <array>

struct VjParamConfig;

inline QString getVjUserDefaultsFilePath()
{
    QString app_data = QString::fromLocal8Bit(qgetenv("APPDATA"));
    if (app_data.isEmpty()) {
        app_data = QDir::homePath() + QStringLiteral("/.config");
    }
    const QString target_dir = QDir(app_data).filePath(QStringLiteral("obs-studio/plugin_config/obs_vj_dj_plugin"));
    QDir().mkpath(target_dir);
    return QDir(target_dir).filePath(QStringLiteral("vj_user_defaults.json"));
}

inline bool saveUserShaderDefault(int shader_idx, const std::array<VjParamConfig, 8> &params)
{
    if (shader_idx < 0 || shader_idx >= 64) return false;
    const QString path = getVjUserDefaultsFilePath();
    QJsonObject root;
    if (QFile::exists(path)) {
        QFile in(path);
        if (in.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(in.readAll());
            if (doc.isObject()) {
                root = doc.object();
            }
            in.close();
        }
    }

    QJsonArray p_arr;
    for (int i = 0; i < 8; ++i) {
        QJsonObject p_obj;
        p_obj[QStringLiteral("val")] = static_cast<double>(params[i].value);
        p_obj[QStringLiteral("mod")] = params[i].mod_mode;
        p_obj[QStringLiteral("depth")] = static_cast<double>(params[i].mod_depth);
        p_arr.append(p_obj);
    }
    root[QStringLiteral("shader_%1").arg(shader_idx)] = p_arr;

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    out.close();
    return true;
}

inline bool loadUserShaderDefaults(std::array<std::array<VjParamConfig, 8>, 64> &all_shader_params)
{
    const QString path = getVjUserDefaultsFilePath();
    if (!QFile::exists(path)) return false;

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll());
    in.close();
    if (!doc.isObject()) return false;

    const QJsonObject root = doc.object();
    bool any_loaded = false;
    for (int s = 0; s < 64; ++s) {
        const QString key = QStringLiteral("shader_%1").arg(s);
        if (root.contains(key) && root[key].isArray()) {
            const QJsonArray p_arr = root[key].toArray();
            for (int p = 0; p < 8 && p < p_arr.size(); ++p) {
                if (p_arr[p].isObject()) {
                    const QJsonObject p_obj = p_arr[p].toObject();
                    all_shader_params[s][p].value = static_cast<float>(p_obj.value(QStringLiteral("val")).toDouble(all_shader_params[s][p].value));
                    all_shader_params[s][p].mod_mode = p_obj.value(QStringLiteral("mod")).toInt(all_shader_params[s][p].mod_mode);
                    all_shader_params[s][p].mod_depth = static_cast<float>(p_obj.value(QStringLiteral("depth")).toDouble(all_shader_params[s][p].mod_depth));
                    any_loaded = true;
                }
            }
        }
    }
    return any_loaded;
}

inline bool resetUserShaderDefault(int shader_idx)
{
    if (shader_idx < 0 || shader_idx >= 64) return false;
    const QString path = getVjUserDefaultsFilePath();
    if (!QFile::exists(path)) return true;

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll());
    in.close();
    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    const QString key = QStringLiteral("shader_%1").arg(shader_idx);
    if (!root.contains(key)) return true;

    root.remove(key);
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    out.close();
    return true;
}
