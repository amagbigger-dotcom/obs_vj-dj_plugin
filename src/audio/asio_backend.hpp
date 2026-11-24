#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

struct AsioChannelRouting {
    int master_l = 1;
    int master_r = 2;
    int cue_l = 3;
    int cue_r = 4;
    int booth_l = 5;
    int booth_r = 6;
    int deck_a_l = 1;
    int deck_a_r = 2;
    int deck_b_l = 3;
    int deck_b_r = 4;
    bool external_mixer_mode = false;
};

class AsioBackend {
public:
    static AsioBackend &instance();

    AsioBackend();
    ~AsioBackend();

    bool open(const std::string &driver_name, int sample_rate = 48000, int preferred_buffer_size = 256);
    void close();
    bool isRunning() const noexcept;
    std::string currentDriverName() const;

    void setRouting(const AsioChannelRouting &routing) noexcept;
    AsioChannelRouting routing() const noexcept;

    // Zero-allocation frame feeding for Master and CUE
    void writeMasterFrames(const float *l, const float *r, size_t frames) noexcept;
    void writeCueFrames(const float *l, const float *r, size_t frames) noexcept;
    void writeDeckFrames(int deck_idx, const float *l, const float *r, size_t frames) noexcept;

    bool openControlPanel();

    int outputChannelCount() const noexcept;
    int sampleRate() const noexcept;
    int bufferSize() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
