#pragma once

#include <QString>
#include <vector>
#include <functional>
#include <cstdint>
#include "effects/vj_config.hpp"

struct TrackAnalysisResult {
    bool success = false;
    double detected_bpm = 128.0;
    int64_t grid_offset_ms = 0;
    int64_t cue_point_ms = 0;
    int64_t hot_cues[4] = {-1, -1, -1, -1};
    int64_t duration_ms = 0;
    QString detected_key = QStringLiteral("--");
    QString camelot_key = QStringLiteral("--");
    float replay_gain_db = 0.0f;
    float peak_amplitude = 1.0f;
    std::vector<float> overview_peaks;   // ~500 points for full track overview strip
    std::vector<float> overview_low;     // Low frequencies (<250Hz, Bass/Kick)
    std::vector<float> overview_mid;     // Mid frequencies (250Hz - 2.5kHz, Vocals/Snare)
    std::vector<float> overview_high;    // High frequencies (>2.5kHz, Hats/Air)
    std::vector<float> overview_tonus;   // Lassú makro-struktúra zenei tónus és fokozódás [0.0 .. 1.0]
    std::vector<float> detailed_peaks;   // 100 points/sec (10ms resolution) for compact high-res scrolling wave
    std::vector<float> detailed_low;     // Detailed 100pts/sec Low
    std::vector<float> detailed_mid;     // Detailed 100pts/sec Mid
    std::vector<float> detailed_high;    // Detailed 100pts/sec High
    std::vector<float> waveform_peaks;   // alias for overview
    std::vector<float> audio_pcm_l;      // RAM stereo float PCM buffer (Left)
    std::vector<float> audio_pcm_r;      // RAM stereo float PCM buffer (Right)
    uint32_t audio_sample_rate = 44100;

    bool has_vj_config = false;
    VjDeckConfig vj_config;
};

class AudioAnalyzer {
public:
    static QString cacheFilePath(const QString &file_path);
    static bool loadCache(const QString &file_path, TrackAnalysisResult &out_res);
    static bool saveCache(const QString &file_path, const TrackAnalysisResult &res);
    static bool updateCacheUserEdits(const QString &file_path, double bpm, int64_t grid_offset_ms, int64_t cue_point_ms);
    static bool updateCacheHotCues(const QString &file_path, const int64_t hot_cues[4]);
    static bool updateCacheVjConfig(const QString &file_path, const VjDeckConfig &config);

    static TrackAnalysisResult analyzeFile(const QString &file_path, bool force_reanalyze = false);
    static void analyzeFileAsync(const QString &file_path,
                                 std::function<void(TrackAnalysisResult)> callback,
                                 bool force_reanalyze = false);

    static bool extractAudioOnly(const QString &file_path,
                                 std::vector<float> &out_l,
                                 std::vector<float> &out_r,
                                 uint32_t &out_sr);
    static void extractAudioOnlyAsync(const QString &file_path,
                                      std::function<void(std::vector<float>, std::vector<float>, uint32_t)> callback);
};
