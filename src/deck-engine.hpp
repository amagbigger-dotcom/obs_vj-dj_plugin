#pragma once

#include <obs-module.h>
#include <QString>
#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <chrono>

class DeckPlayer final {
public:
    DeckPlayer(const char *source_name, const char *display_name);
    ~DeckPlayer();

    DeckPlayer(const DeckPlayer &) = delete;
    DeckPlayer &operator=(const DeckPlayer &) = delete;

    obs_source_t *source() const noexcept { return source_; }

    void loadFile(const QString &path);
    QString fileName() const;
    void setFileName(const QString &name);

    void togglePlayback();
    void setPlaying(bool playing);
    void setLooping(bool looping);
    void setLoopBeatIndex(int index) noexcept;
    void updateLoop();
    void seek(double normalized_position);
    void cue();

    void setSpeed(int percent);
    int speed() const noexcept;

    void setBpm(double bpm) noexcept;
    double bpm() const noexcept;
    double effectiveBpm() const noexcept;
    double beatDurationMs() const noexcept;

    void setGridOffsetMs(int64_t offset_ms) noexcept;
    int64_t gridOffsetMs() const noexcept;
    void nudgeGrid(int64_t delta_ms) noexcept;
    void setGridAnchorAtCurrent() noexcept;

    void setWaveformSamples(const std::vector<float> &overview, const std::vector<float> &detailed);
    std::vector<float> overviewSamples() const;
    std::vector<float> detailedSamples() const;
    std::vector<float> waveformSamples() const;
    bool hasAnalyzedWaveform() const;

    void setEqHi(float val) noexcept;
    void setEqMid(float val) noexcept;
    void setEqLow(float val) noexcept;
    void setFilterVal(float val) noexcept;

    float eqHi() const noexcept;
    float eqMid() const noexcept;
    float eqLow() const noexcept;
    float filterVal() const noexcept;

    void processAudioDsp(float *channel_data, size_t num_frames, size_t channel_idx, size_t sample_rate);

    void setGain(float gain) noexcept;
    float gain() const noexcept;

    void setVolume(float volume);
    float volume() const noexcept;

    void setMixGain(float gain) noexcept;
    void enableAudioMonitoring() noexcept;

    void setHeadphoneCue(bool cue) noexcept;
    bool isHeadphoneCue() const noexcept;

    int64_t loopStartMs() const noexcept;
    int64_t loopEndMs() const noexcept;

    void setRed(float value) noexcept;
    void setGreen(float value) noexcept;
    void setBlue(float value) noexcept;
    void setSaturation(float value) noexcept;

    float red() const noexcept;
    float green() const noexcept;
    float blue() const noexcept;
    float saturation() const noexcept;

    double position() const;
    int64_t currentTimeMs() const;
    int64_t durationMs() const;
    bool isPlaying() const;
    bool isLooping() const noexcept;

private:
    void applyAudioVolume() noexcept;

    obs_source_t *source_ = nullptr;

    mutable std::mutex file_mutex_;
    QString file_name_{};

    std::atomic<bool> looping_{false};
    std::atomic<int> loop_beat_index_{3};

    std::atomic<int64_t> loop_start_ms_{0};
    std::atomic<int64_t> loop_end_ms_{0};
    std::atomic<int64_t> cue_point_ms_{0};

    std::atomic<double> base_bpm_{128.0};
    std::atomic<int64_t> grid_offset_ms_{0};
    std::atomic<bool> headphone_cue_{false};

    std::atomic<int> speed_percent_{100};
    std::atomic<float> gain_{1.0f};
    std::atomic<float> volume_{1.0f};
    std::atomic<float> mix_gain_{1.0f};

    std::atomic<float> red_{1.0f};
    std::atomic<float> green_{1.0f};
    std::atomic<float> blue_{1.0f};
    std::atomic<float> saturation_{1.0f};

    std::atomic<float> eq_hi_{1.0f};
    std::atomic<float> eq_mid_{1.0f};
    std::atomic<float> eq_low_{1.0f};
    std::atomic<float> filter_val_{0.0f};

    std::chrono::steady_clock::time_point last_loop_seek_tp_{};

    struct FilterState {
        float lp = 0.0f;
        float hp = 0.0f;
        float dj_lp = 0.0f;
        float dj_hp = 0.0f;
    } dsp_states_[2];

    mutable std::mutex waveform_mutex_;
    std::vector<float> overview_samples_;
    std::vector<float> detailed_samples_;
};

using DeckEngine = DeckPlayer;

class DeckPair final {
public:
    DeckPair();
    ~DeckPair();

    DeckPair(const DeckPair &) = delete;
    DeckPair &operator=(const DeckPair &) = delete;

    DeckPlayer &deckA() noexcept { return deck_a_; }
    DeckPlayer &deckB() noexcept { return deck_b_; }

    obs_source_t *masterSource() const noexcept { return master_source_; }

    void setCrossfader(double value) noexcept;
    double crossfader() const noexcept;

    void setTransitionMode(int mode) noexcept;
    int transitionMode() const noexcept;

    void setObsOutputMode(int mode) noexcept;
    int obsOutputMode() const noexcept;

    void setHeadphoneVolume(float v) noexcept;
    float headphoneVolume() const noexcept;

    void setHeadphoneMix(float m) noexcept;
    float headphoneMix() const noexcept;

    void syncTempoAtoB();
    void syncTempoBtoA();

    void setLoopBeatIndex(int deck_index, int beat_index);
    int getLoopBeatIndex(int deck_index) const noexcept;

    void updateAudioMix() noexcept;
    void enableAudioMonitoring() noexcept;

private:
    DeckPlayer deck_a_;
    DeckPlayer deck_b_;
    obs_source_t *master_source_ = nullptr;

    std::atomic<double> crossfader_{0.5};
    std::atomic<int> transition_mode_{0};
    std::atomic<int> obs_output_mode_{0};
    std::atomic<float> headphone_volume_{1.0f};
    std::atomic<float> headphone_mix_{0.5f};
    std::atomic<int> loop_beat_idx_a_{3};
    std::atomic<int> loop_beat_idx_b_{3};
};

DeckPair *vdj_decks() noexcept;
void vdj_set_decks(DeckPair *decks) noexcept;
