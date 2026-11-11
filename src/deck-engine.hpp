#pragma once

#include <obs-module.h>
#include <QString>
#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <chrono>
#include <queue>
#include <thread>
#include <condition_variable>

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
    QString filePath() const;
    void setFilePath(const QString &path);

    void togglePlayback();
    void setPlaying(bool playing);
    void setLooping(bool looping);
    void setLoopBeatIndex(int index) noexcept;
    void updateLoop();
    void seek(double normalized_position);
    void cue();

    void setSpeed(int percent);
    void applySpeedSettings(int speed_percent);
    int speed() const noexcept;

    void setBpm(double bpm) noexcept;
    double bpm() const noexcept;
    double effectiveBpm() const noexcept;
    double beatDurationMs() const noexcept;

    void setGridOffsetMs(int64_t offset_ms) noexcept;
    int64_t gridOffsetMs() const noexcept;
    void nudgeGrid(int64_t delta_ms) noexcept;
    void setGridAnchorAtCurrent() noexcept;

    void saveUserEdits();

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
    bool hasVideo() const noexcept { return has_video_.load(std::memory_order_relaxed); }
    void setHasVideo(bool v) noexcept { has_video_.store(v, std::memory_order_relaxed); }

    bool isMaster() const noexcept { return is_master_.load(std::memory_order_relaxed); }
    void setMaster(bool m) noexcept { is_master_.store(m, std::memory_order_relaxed); }
    bool isSync() const noexcept { return is_sync_.load(std::memory_order_relaxed); }
    void setSync(bool s) noexcept { is_sync_.store(s, std::memory_order_relaxed); }

private:
    void applyAudioVolume() noexcept;

    obs_source_t *source_ = nullptr;
    obs_source_t *eq_filter_ = nullptr;
    bool is_deck_a_ = true;

    mutable std::mutex file_mutex_;
    QString file_name_{};
    QString file_path_{};

    std::atomic<bool> looping_{false};
    std::atomic<bool> has_video_{false};
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

    std::atomic<bool> is_master_{false};
    std::atomic<bool> is_sync_{false};
    std::atomic<int64_t> pending_speed_seek_ms_{0};
    std::atomic<bool> is_restarting_for_speed_{false};
    std::atomic<bool> is_loop_seeking_{false};
    std::chrono::steady_clock::time_point last_loop_seek_tp_{};
    std::chrono::steady_clock::time_point last_speed_update_tp_{};

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
    obs_source_t *cueSource() const noexcept { return cue_source_; }

    void queueCueAudio(int deck_idx, const float *const *channels, size_t frames);

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

    enum MasterMode {
        MASTER_CLOCK = 0,
        MASTER_DECK_A = 1,
        MASTER_DECK_B = 2,
        MASTER_EXT_MIDI = 3
    };

    void setMasterMode(int mode) noexcept;
    int masterMode() const noexcept;

    void setMasterClockBpm(double bpm) noexcept;
    double masterClockBpm() const noexcept;
    double effectiveMasterBpm() const noexcept;

    void setSync(int deck_idx, bool sync_enabled);
    bool isSync(int deck_idx) const noexcept;
    void setDeckMaster(int deck_idx);
    void updateSync();

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
    obs_source_t *cue_source_ = nullptr;

    std::atomic<double> crossfader_{0.5};
    std::atomic<int> transition_mode_{0};
    std::atomic<int> obs_output_mode_{0};
    std::atomic<float> headphone_volume_{1.0f};
    std::atomic<float> headphone_mix_{0.5f};
    std::atomic<int> master_mode_{0};
    std::atomic<double> master_clock_bpm_{128.0};
    std::atomic<int> loop_beat_idx_a_{3};
    std::atomic<int> loop_beat_idx_b_{3};

    struct CuePacket {
        std::vector<float> l;
        std::vector<float> r;
        size_t frames = 0;
    };
    std::mutex cue_mutex_;
    std::condition_variable cue_cv_;
    std::queue<CuePacket> cue_queue_;
    std::thread cue_thread_;
    std::atomic<bool> cue_thread_running_{false};

    void cueWorkerLoop();
};

DeckPair *vdj_decks() noexcept;
void vdj_set_decks(DeckPair *decks) noexcept;
