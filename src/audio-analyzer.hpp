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
    std::vector<float> waveform_peaks;
};

class AudioAnalyzer {
public:
    static TrackAnalysisResult analyzeFile(const QString &file_path);
    static void analyzeFileAsync(const QString &file_path,
                                 std::function<void(TrackAnalysisResult)> callback);
};

