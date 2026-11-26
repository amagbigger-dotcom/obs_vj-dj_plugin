#include "track/track_cache.hpp"
#include "audio/audio_analyzer.hpp"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <algorithm>

QString TrackCache::filePath(const QString &media_file_path)
{
    return media_file_path + QStringLiteral(".vdj.json");
}

static QJsonObject serializeVjConfig(const VjDeckConfig &config)
{
    QJsonObject vj_obj;
    vj_obj.insert(QStringLiteral("shader_idx"), config.shader_idx);
    vj_obj.insert(QStringLiteral("color_r"), std::round(config.custom_color_r * 1000.0) / 1000.0);
    vj_obj.insert(QStringLiteral("color_g"), std::round(config.custom_color_g * 1000.0) / 1000.0);
    vj_obj.insert(QStringLiteral("color_b"), std::round(config.custom_color_b * 1000.0) / 1000.0);
    vj_obj.insert(QStringLiteral("color_mod"), config.color_mod_mode);
    vj_obj.insert(QStringLiteral("rgb_inverted"), config.rgb_inverted);
    vj_obj.insert(QStringLiteral("audio_reactivity"), std::round(config.audio_reactivity * 1000.0) / 1000.0);

    QJsonObject all_s;
    for (int s = 0; s < 64; ++s) {
        QJsonArray s_arr;
        for (int i = 0; i < 8; ++i) {
            QJsonObject p_obj;
            p_obj.insert(QStringLiteral("val"), std::round(config.all_shader_params[s][i].value * 10000.0) / 10000.0);
            p_obj.insert(QStringLiteral("mod"), config.all_shader_params[s][i].mod_mode);
            p_obj.insert(QStringLiteral("depth"), std::round(config.all_shader_params[s][i].mod_depth * 1000.0) / 1000.0);
            s_arr.append(p_obj);
        }
        all_s.insert(QString::number(s), s_arr);
    }
    vj_obj.insert(QStringLiteral("all_shaders"), all_s);

    QJsonArray p_arr;
    for (int i = 0; i < 8; ++i) {
        QJsonObject p_obj;
        p_obj.insert(QStringLiteral("val"), std::round(config.params[i].value * 10000.0) / 10000.0);
        p_obj.insert(QStringLiteral("mod"), config.params[i].mod_mode);
        p_obj.insert(QStringLiteral("depth"), std::round(config.params[i].mod_depth * 1000.0) / 1000.0);
        p_arr.append(p_obj);
    }
    vj_obj.insert(QStringLiteral("params"), p_arr);
    return vj_obj;
}

static VjDeckConfig deserializeVjConfig(const QJsonObject &vj_obj)
{
    const int s_idx = std::clamp(vj_obj.value(QStringLiteral("shader_idx")).toInt(0), 0, 63);
    VjDeckConfig cfg = getDefaultVjConfig(s_idx);
    cfg.shader_idx = s_idx;
    cfg.custom_color_r = static_cast<float>(vj_obj.value(QStringLiteral("color_r")).toDouble(-1.0));
    cfg.custom_color_g = static_cast<float>(vj_obj.value(QStringLiteral("color_g")).toDouble(-1.0));
    cfg.custom_color_b = static_cast<float>(vj_obj.value(QStringLiteral("color_b")).toDouble(-1.0));
    cfg.color_mod_mode = vj_obj.value(QStringLiteral("color_mod")).toInt(0);
    cfg.rgb_inverted = vj_obj.value(QStringLiteral("rgb_inverted")).toBool(false);
    cfg.audio_reactivity = static_cast<float>(vj_obj.value(QStringLiteral("audio_reactivity")).toDouble(1.0));

    if (vj_obj.contains(QStringLiteral("all_shaders"))) {
        QJsonObject all_s = vj_obj.value(QStringLiteral("all_shaders")).toObject();
        for (int s = 0; s < 64; ++s) {
            const QString key = QString::number(s);
            if (all_s.contains(key)) {
                QJsonArray s_arr = all_s.value(key).toArray();
                for (int i = 0; i < std::min<int>(8, static_cast<int>(s_arr.size())); ++i) {
                    QJsonObject p_obj = s_arr[i].toObject();
                    cfg.all_shader_params[s][i].value = static_cast<float>(p_obj.value(QStringLiteral("val")).toDouble(1.0));
                    cfg.all_shader_params[s][i].mod_mode = p_obj.value(QStringLiteral("mod")).toInt(0);
                    cfg.all_shader_params[s][i].mod_depth = static_cast<float>(p_obj.value(QStringLiteral("depth")).toDouble(0.0));
                }
            }
        }
    }

    QJsonArray p_arr = vj_obj.value(QStringLiteral("params")).toArray();
    for (int i = 0; i < std::min<int>(8, static_cast<int>(p_arr.size())); ++i) {
        QJsonObject p_obj = p_arr[i].toObject();
        cfg.params[i].value = static_cast<float>(p_obj.value(QStringLiteral("val")).toDouble(1.0));
        cfg.params[i].mod_mode = p_obj.value(QStringLiteral("mod")).toInt(0);
        cfg.params[i].mod_depth = static_cast<float>(p_obj.value(QStringLiteral("depth")).toDouble(0.0));
    }
    cfg.all_shader_params[s_idx] = cfg.params;
    return cfg;
}

static QJsonObject serializeMasterFx(const MasterFxTrackConfig &cfg)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("cf_mode"), cfg.crossfade_fx_mode);
    obj.insert(QStringLiteral("intensity"), std::round(cfg.intensity * 1000.0) / 1000.0);
    obj.insert(QStringLiteral("intensity_mod"), cfg.intensity_mod);
    obj.insert(QStringLiteral("rate"), std::round(cfg.rate * 1000.0) / 1000.0);
    obj.insert(QStringLiteral("rate_mod"), cfg.rate_mod);
    obj.insert(QStringLiteral("blend_boost"), std::round(cfg.blend_boost * 1000.0) / 1000.0);

    QJsonObject all_p;
    for (int m = 0; m < 16; ++m) {
        QJsonArray p_arr;
        for (int i = 0; i < 8; ++i) {
            QJsonObject p_obj;
            p_obj.insert(QStringLiteral("val"), std::round(cfg.fx_params[m][i].value * 10000.0) / 10000.0);
            p_obj.insert(QStringLiteral("mod"), cfg.fx_params[m][i].mod_mode);
            p_obj.insert(QStringLiteral("depth"), std::round(cfg.fx_params[m][i].mod_depth * 1000.0) / 1000.0);
            p_arr.append(p_obj);
        }
        all_p.insert(QString::number(m), p_arr);
    }
    obj.insert(QStringLiteral("fx_params"), all_p);

    QJsonObject lfo1_obj;
    lfo1_obj.insert(QStringLiteral("waveform"), cfg.lfo1.waveform);
    lfo1_obj.insert(QStringLiteral("rate_idx"), cfg.lfo1.rate_idx);
    lfo1_obj.insert(QStringLiteral("depth"), std::round(cfg.lfo1.depth * 1000.0) / 1000.0);
    lfo1_obj.insert(QStringLiteral("offset"), std::round(cfg.lfo1.offset * 1000.0) / 1000.0);
    lfo1_obj.insert(QStringLiteral("phase_deg"), std::round(cfg.lfo1.phase_deg * 10.0) / 10.0);
    obj.insert(QStringLiteral("lfo1"), lfo1_obj);

    QJsonObject lfo2_obj;
    lfo2_obj.insert(QStringLiteral("waveform"), cfg.lfo2.waveform);
    lfo2_obj.insert(QStringLiteral("rate_idx"), cfg.lfo2.rate_idx);
    lfo2_obj.insert(QStringLiteral("depth"), std::round(cfg.lfo2.depth * 1000.0) / 1000.0);
    lfo2_obj.insert(QStringLiteral("offset"), std::round(cfg.lfo2.offset * 1000.0) / 1000.0);
    lfo2_obj.insert(QStringLiteral("phase_deg"), std::round(cfg.lfo2.phase_deg * 10.0) / 10.0);
    obj.insert(QStringLiteral("lfo2"), lfo2_obj);

    return obj;
}

static MasterFxTrackConfig deserializeMasterFx(const QJsonObject &obj)
{
    MasterFxTrackConfig cfg;
    cfg.valid = true;
    cfg.crossfade_fx_mode = std::clamp(obj.value(QStringLiteral("cf_mode")).toInt(0), 0, 15);
    cfg.intensity = static_cast<float>(obj.value(QStringLiteral("intensity")).toDouble(1.0));
    cfg.intensity_mod = std::clamp(obj.value(QStringLiteral("intensity_mod")).toInt(0), 0, 7);
    cfg.rate = static_cast<float>(obj.value(QStringLiteral("rate")).toDouble(1.0));
    cfg.rate_mod = std::clamp(obj.value(QStringLiteral("rate_mod")).toInt(0), 0, 7);
    cfg.blend_boost = static_cast<float>(obj.value(QStringLiteral("blend_boost")).toDouble(1.0));

    if (obj.contains(QStringLiteral("fx_params"))) {
        QJsonObject all_p = obj.value(QStringLiteral("fx_params")).toObject();
        for (int m = 0; m < 16; ++m) {
            const QString k = QString::number(m);
            if (all_p.contains(k)) {
                QJsonArray p_arr = all_p.value(k).toArray();
                for (int i = 0; i < std::min<int>(8, static_cast<int>(p_arr.size())); ++i) {
                    QJsonObject p_obj = p_arr[i].toObject();
                    cfg.fx_params[m][i].value = static_cast<float>(p_obj.value(QStringLiteral("val")).toDouble(1.0));
                    cfg.fx_params[m][i].mod_mode = std::clamp(p_obj.value(QStringLiteral("mod")).toInt(0), 0, 7);
                    cfg.fx_params[m][i].mod_depth = static_cast<float>(p_obj.value(QStringLiteral("depth")).toDouble(0.0));
                }
            }
        }
    }

    if (obj.contains(QStringLiteral("lfo1"))) {
        QJsonObject l1 = obj.value(QStringLiteral("lfo1")).toObject();
        cfg.lfo1.waveform = std::clamp(l1.value(QStringLiteral("waveform")).toInt(0), 0, 4);
        cfg.lfo1.rate_idx = std::clamp(l1.value(QStringLiteral("rate_idx")).toInt(6), 0, 10);
        cfg.lfo1.depth = static_cast<float>(l1.value(QStringLiteral("depth")).toDouble(1.0));
        cfg.lfo1.offset = static_cast<float>(l1.value(QStringLiteral("offset")).toDouble(0.0));
        cfg.lfo1.phase_deg = static_cast<float>(l1.value(QStringLiteral("phase_deg")).toDouble(0.0));
    }
    if (obj.contains(QStringLiteral("lfo2"))) {
        QJsonObject l2 = obj.value(QStringLiteral("lfo2")).toObject();
        cfg.lfo2.waveform = std::clamp(l2.value(QStringLiteral("waveform")).toInt(0), 0, 4);
        cfg.lfo2.rate_idx = std::clamp(l2.value(QStringLiteral("rate_idx")).toInt(6), 0, 10);
        cfg.lfo2.depth = static_cast<float>(l2.value(QStringLiteral("depth")).toDouble(1.0));
        cfg.lfo2.offset = static_cast<float>(l2.value(QStringLiteral("offset")).toDouble(0.0));
        cfg.lfo2.phase_deg = static_cast<float>(l2.value(QStringLiteral("phase_deg")).toDouble(0.0));
    }

    return cfg;
}

bool TrackCache::load(const QString &media_file_path, TrackAnalysisResult &out_res, int deck_side)
{
    const QString cache_path = filePath(media_file_path);
    QFile file(cache_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const int ver = root.value(QStringLiteral("version")).toInt(0);
    if (ver < 2)
        return false;

    out_res.detected_bpm = root.value(QStringLiteral("bpm")).toDouble(128.0);
    out_res.grid_offset_ms = static_cast<int64_t>(root.value(QStringLiteral("grid_offset_ms")).toVariant().toLongLong());
    out_res.cue_point_ms = static_cast<int64_t>(root.value(QStringLiteral("cue_point_ms")).toVariant().toLongLong());
    out_res.duration_ms = static_cast<int64_t>(root.value(QStringLiteral("duration_ms")).toVariant().toLongLong());
    out_res.user_bpm_locked = root.value(QStringLiteral("bpm_locked")).toBool(false);
    out_res.user_grid_locked = root.value(QStringLiteral("grid_locked")).toBool(false);
    out_res.detected_key = root.value(QStringLiteral("key")).toString(QStringLiteral("--"));
    out_res.camelot_key = root.value(QStringLiteral("camelot")).toString(QStringLiteral("--"));
    out_res.replay_gain_db = static_cast<float>(root.value(QStringLiteral("replay_gain_db")).toDouble(0.0));

    const QJsonArray hc_arr = root.value(QStringLiteral("hot_cues")).toArray();
    for (int i = 0; i < 4; ++i) {
        if (i < hc_arr.size()) {
            out_res.hot_cues[i] = static_cast<int64_t>(hc_arr[i].toVariant().toLongLong());
        } else {
            out_res.hot_cues[i] = -1;
        }
    }

    const QJsonArray ov_arr = root.value(QStringLiteral("overview_peaks")).toArray();
    out_res.overview_peaks.clear();
    out_res.overview_peaks.reserve(ov_arr.size());
    for (const auto &val : ov_arr) {
        out_res.overview_peaks.push_back(static_cast<float>(val.toDouble(0.1)));
    }

    const QJsonArray ov_low_arr = root.value(QStringLiteral("overview_low")).toArray();
    const QJsonArray ov_mid_arr = root.value(QStringLiteral("overview_mid")).toArray();
    const QJsonArray ov_high_arr = root.value(QStringLiteral("overview_high")).toArray();
    if (!ov_low_arr.empty()) {
        out_res.overview_low.clear();
        out_res.overview_mid.clear();
        out_res.overview_high.clear();
        for (const auto &v : ov_low_arr) out_res.overview_low.push_back(static_cast<float>(v.toDouble(0.1)));
        for (const auto &v : ov_mid_arr) out_res.overview_mid.push_back(static_cast<float>(v.toDouble(0.1)));
        for (const auto &v : ov_high_arr) out_res.overview_high.push_back(static_cast<float>(v.toDouble(0.1)));
    } else {
        out_res.overview_low = out_res.overview_peaks;
        out_res.overview_mid.resize(out_res.overview_peaks.size());
        out_res.overview_high.resize(out_res.overview_peaks.size());
        for (size_t i = 0; i < out_res.overview_peaks.size(); ++i) {
            out_res.overview_mid[i] = out_res.overview_peaks[i] * 0.75f;
            out_res.overview_high[i] = out_res.overview_peaks[i] * 0.5f;
        }
    }

    const QJsonArray det_arr = root.value(QStringLiteral("detailed_peaks")).toArray();
    out_res.detailed_peaks.clear();
    out_res.detailed_peaks.reserve(det_arr.size());
    for (const auto &val : det_arr) {
        out_res.detailed_peaks.push_back(static_cast<float>(val.toDouble(0.1)));
    }

    const QJsonArray ton_arr = root.value(QStringLiteral("overview_tonus")).toArray();
    if (!ton_arr.empty()) {
        out_res.overview_tonus.clear();
        out_res.overview_tonus.reserve(ton_arr.size());
        for (const auto &val : ton_arr) {
            out_res.overview_tonus.push_back(static_cast<float>(val.toDouble(0.5)));
        }
    } else {
        out_res.overview_tonus.assign(out_res.overview_peaks.size(), 0.5f);
    }

    if (root.contains(QStringLiteral("vj_shader_deck_a"))) {
        out_res.vj_config_deck_a = deserializeVjConfig(root.value(QStringLiteral("vj_shader_deck_a")).toObject());
        out_res.has_vj_config_deck_a = true;
    }
    if (root.contains(QStringLiteral("vj_shader_deck_b"))) {
        out_res.vj_config_deck_b = deserializeVjConfig(root.value(QStringLiteral("vj_shader_deck_b")).toObject());
        out_res.has_vj_config_deck_b = true;
    }
    if (root.contains(QStringLiteral("vj_shader"))) {
        out_res.vj_config = deserializeVjConfig(root.value(QStringLiteral("vj_shader")).toObject());
        out_res.has_vj_config = true;
    }

    if (deck_side == 0) {
        if (out_res.has_vj_config_deck_a) {
            out_res.vj_config = out_res.vj_config_deck_a;
            out_res.has_vj_config = true;
        }
    } else if (deck_side == 1) {
        if (out_res.has_vj_config_deck_b) {
            out_res.vj_config = out_res.vj_config_deck_b;
            out_res.has_vj_config = true;
        }
    } else {
        if (out_res.has_vj_config_deck_a) {
            out_res.vj_config = out_res.vj_config_deck_a;
            out_res.has_vj_config = true;
        } else if (out_res.has_vj_config_deck_b) {
            out_res.vj_config = out_res.vj_config_deck_b;
            out_res.has_vj_config = true;
        }
    }

    if (root.contains(QStringLiteral("master_fx"))) {
        out_res.master_fx = deserializeMasterFx(root.value(QStringLiteral("master_fx")).toObject());
        out_res.has_master_fx = true;
    }

    out_res.waveform_peaks = out_res.overview_peaks;
    out_res.success = !out_res.overview_peaks.empty() && (out_res.detected_bpm > 30.0);
    return out_res.success;
}

bool TrackCache::save(const QString &media_file_path, const TrackAnalysisResult &res)
{
    if (!res.success)
        return false;

    const QString cache_path = filePath(media_file_path);
    QJsonObject root;
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("bpm"), res.detected_bpm);
    root.insert(QStringLiteral("bpm_locked"), res.user_bpm_locked);
    root.insert(QStringLiteral("grid_offset_ms"), static_cast<qint64>(res.grid_offset_ms));
    root.insert(QStringLiteral("grid_locked"), res.user_grid_locked);
    root.insert(QStringLiteral("cue_point_ms"), static_cast<qint64>(res.cue_point_ms));
    root.insert(QStringLiteral("duration_ms"), static_cast<qint64>(res.duration_ms));
    root.insert(QStringLiteral("key"), res.detected_key);
    root.insert(QStringLiteral("camelot"), res.camelot_key);
    root.insert(QStringLiteral("replay_gain_db"), std::round(res.replay_gain_db * 100.0) / 100.0);

    QJsonArray hc_arr;
    for (int i = 0; i < 4; ++i) {
        hc_arr.append(static_cast<qint64>(res.hot_cues[i]));
    }
    root.insert(QStringLiteral("hot_cues"), hc_arr);

    QJsonArray ov_arr;
    for (float v : res.overview_peaks) {
        ov_arr.append(std::round(v * 1000.0) / 1000.0);
    }
    root.insert(QStringLiteral("overview_peaks"), ov_arr);

    QJsonArray ov_l_arr, ov_m_arr, ov_h_arr, ov_t_arr;
    for (float v : res.overview_low) ov_l_arr.append(std::round(v * 1000.0) / 1000.0);
    for (float v : res.overview_mid) ov_m_arr.append(std::round(v * 1000.0) / 1000.0);
    for (float v : res.overview_high) ov_h_arr.append(std::round(v * 1000.0) / 1000.0);
    for (float v : res.overview_tonus) ov_t_arr.append(std::round(v * 1000.0) / 1000.0);
    root.insert(QStringLiteral("overview_low"), ov_l_arr);
    root.insert(QStringLiteral("overview_mid"), ov_m_arr);
    root.insert(QStringLiteral("overview_high"), ov_h_arr);
    root.insert(QStringLiteral("overview_tonus"), ov_t_arr);

    QJsonArray det_arr;
    for (float v : res.detailed_peaks) {
        det_arr.append(std::round(v * 1000.0) / 1000.0);
    }
    root.insert(QStringLiteral("detailed_peaks"), det_arr);

    if (res.has_vj_config_deck_a) {
        root.insert(QStringLiteral("vj_shader_deck_a"), serializeVjConfig(res.vj_config_deck_a));
    }
    if (res.has_vj_config_deck_b) {
        root.insert(QStringLiteral("vj_shader_deck_b"), serializeVjConfig(res.vj_config_deck_b));
    }
    if (res.has_vj_config) {
        root.insert(QStringLiteral("vj_shader"), serializeVjConfig(res.vj_config));
    }
    if (res.has_master_fx) {
        root.insert(QStringLiteral("master_fx"), serializeMasterFx(res.master_fx));
    }

    QFile file(cache_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

bool TrackCache::updateVjConfig(const QString &media_file_path, const VjDeckConfig &config, int deck_side)
{
    const QString cache_path = filePath(media_file_path);
    QFile file(cache_path);
    QJsonObject root;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonObject vj_obj = serializeVjConfig(config);
    if (deck_side == 0) {
        root.insert(QStringLiteral("vj_shader_deck_a"), vj_obj);
        root.insert(QStringLiteral("vj_shader"), vj_obj);
    } else if (deck_side == 1) {
        root.insert(QStringLiteral("vj_shader_deck_b"), vj_obj);
        root.insert(QStringLiteral("vj_shader"), vj_obj);
    } else {
        root.insert(QStringLiteral("vj_shader"), vj_obj);
    }

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
        return true;
    }
    return false;
}

bool TrackCache::updateMasterFxConfig(const QString &media_file_path, const MasterFxTrackConfig &cfg)
{
    if (!cfg.valid || media_file_path.isEmpty()) return false;
    const QString cache_path = filePath(media_file_path);
    QFile file(cache_path);
    QJsonObject root;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    root.insert(QStringLiteral("master_fx"), serializeMasterFx(cfg));

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
        return true;
    }
    return false;
}

bool TrackCache::loadMasterFxConfig(const QString &media_file_path, MasterFxTrackConfig &out_cfg)
{
    if (media_file_path.isEmpty()) return false;
    const QString cache_path = filePath(media_file_path);
    QFile file(cache_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    if (root.contains(QStringLiteral("master_fx"))) {
        out_cfg = deserializeMasterFx(root.value(QStringLiteral("master_fx")).toObject());
        return true;
    }
    return false;
}

bool TrackCache::updateUserEdits(const QString &media_file_path, double bpm, int64_t grid_offset_ms, int64_t cue_point_ms, int lock_bpm, int lock_grid)
{
    const QString cache_path = filePath(media_file_path);
    QFile file(cache_path);
    QJsonObject root;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    if (bpm > 30.0)
        root.insert(QStringLiteral("bpm"), bpm);
    if (grid_offset_ms >= 0)
        root.insert(QStringLiteral("grid_offset_ms"), static_cast<qint64>(grid_offset_ms));
    if (cue_point_ms >= 0)
        root.insert(QStringLiteral("cue_point_ms"), static_cast<qint64>(cue_point_ms));

    if (lock_bpm == 1)
        root.insert(QStringLiteral("bpm_locked"), true);
    else if (lock_bpm == 0)
        root.insert(QStringLiteral("bpm_locked"), false);

    if (lock_grid == 1)
        root.insert(QStringLiteral("grid_locked"), true);
    else if (lock_grid == 0)
        root.insert(QStringLiteral("grid_locked"), false);

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
        return true;
    }
    return false;
}

bool TrackCache::updateHotCues(const QString &media_file_path, const int64_t hot_cues[4])
{
    const QString cache_path = filePath(media_file_path);
    QFile file(cache_path);
    QJsonObject root;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonArray hc_arr;
    for (int i = 0; i < 4; ++i) {
        hc_arr.append(static_cast<qint64>(hot_cues[i]));
    }
    root.insert(QStringLiteral("hot_cues"), hc_arr);

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
        return true;
    }
    return false;
}
