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
    std::vector<float> detailed_peaks;   // 100 points/sec (10ms resolution) for compact high-res scrolling wave
    std::vector<float> waveform_peaks;   // alias for overview
    std::vector<float> audio_pcm_l;      // RAM stereo float PCM buffer (Left)
    std::vector<float> audio_pcm_r;      // RAM stereo float PCM buffer (Right)
    uint32_t audio_sample_rate = 44100;
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

    static bool extractAudioOnly(const QString &file_path,
                                 std::vector<float> &out_l,
                                 std::vector<float> &out_r,
                                 uint32_t &out_sr);
    static void extractAudioOnlyAsync(const QString &file_path,
                                      std::function<void(std::vector<float>, std::vector<float>, uint32_t)> callback);
};
