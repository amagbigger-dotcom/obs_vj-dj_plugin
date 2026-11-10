#pragma once

#include <QString>
#include <vector>
#include <functional>
#include <cstdint>

struct TrackAnalysisResult {
    bool success = false;
    double detected_bpm = 128.0;
    int64_t grid_offset_ms = 0;
    int64_t duration_ms = 0;
    std::vector<float> overview_peaks;   // ~500 points for full track overview strip
    std::vector<float> detailed_peaks;   // 100 points/sec (10ms resolution) for scrolling wave
    std::vector<float> waveform_peaks;   // alias for overview
};

class AudioAnalyzer {
public:
    static TrackAnalysisResult analyzeFile(const QString &file_path);
    static void analyzeFileAsync(const QString &file_path,
                                 std::function<void(TrackAnalysisResult)> callback);
};
