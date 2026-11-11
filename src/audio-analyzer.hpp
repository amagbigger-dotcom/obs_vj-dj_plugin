#pragma once

#include <QString>
#include <vector>
#include <functional>
#include <cstdint>

struct TrackAnalysisResult {
    bool success = false;
    double detected_bpm = 128.0;
    int64_t grid_offset_ms = 0;
    int64_t cue_point_ms = 0;
    int64_t duration_ms = 0;
    std::vector<float> overview_peaks;   // ~500 points for full track overview strip
    std::vector<float> detailed_peaks;   // 50 points/sec (20ms resolution) for compact high-res scrolling wave
    std::vector<float> waveform_peaks;   // alias for overview
};

class AudioAnalyzer {
public:
    static QString cacheFilePath(const QString &file_path);
    static bool loadCache(const QString &file_path, TrackAnalysisResult &out_res);
    static bool saveCache(const QString &file_path, const TrackAnalysisResult &res);
    static bool updateCacheUserEdits(const QString &file_path, double bpm, int64_t grid_offset_ms, int64_t cue_point_ms);

    static TrackAnalysisResult analyzeFile(const QString &file_path);
    static void analyzeFileAsync(const QString &file_path,
                                 std::function<void(TrackAnalysisResult)> callback);
};
