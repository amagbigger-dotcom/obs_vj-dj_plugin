#pragma once

#include <QString>
#include <vector>
#include <cstdint>

class WmfDecoder {
public:
    static bool decodePcm(const QString &file_path,
                          std::vector<float> &out_l,
                          std::vector<float> &out_r,
                          std::vector<float> *out_mono,
                          uint32_t &out_sample_rate,
                          int64_t &out_duration_ms);
};
