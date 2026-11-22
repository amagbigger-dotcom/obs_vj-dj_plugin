#pragma once

#include <QString>
#include <cstdint>
#include "effects/vj_config.hpp"

struct TrackAnalysisResult;

class TrackCache {
public:
    static QString filePath(const QString &media_file_path);
    static bool load(const QString &media_file_path, TrackAnalysisResult &out_res, int deck_side = -1);
    static bool save(const QString &media_file_path, const TrackAnalysisResult &res);
    static bool updateUserEdits(const QString &media_file_path, double bpm, int64_t grid_offset_ms, int64_t cue_point_ms, int lock_bpm = -1, int lock_grid = -1);
    static bool updateHotCues(const QString &media_file_path, const int64_t hot_cues[4]);
    static bool updateVjConfig(const QString &media_file_path, const VjDeckConfig &config, int deck_side = -1);
    static bool updateMasterFxConfig(const QString &media_file_path, const MasterFxTrackConfig &cfg);
    static bool loadMasterFxConfig(const QString &media_file_path, MasterFxTrackConfig &out_cfg);
};
