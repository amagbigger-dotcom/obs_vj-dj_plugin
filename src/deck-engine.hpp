#pragma once

#include <obs.h>
#include <QString>
#include <QFileInfo>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class DeckPlayer {
public:
    DeckPlayer(const char *source_name = "vj_deck_source",
               const char *display_name = "OBS VJ Deck");
    ~DeckPlayer();

    void loadFile(const QString &path);

    QString fileName() const;
    void setFileName(const QString &name);
    QString filePath() const;
    void setFilePath(const QString &path);

    void play();
    void pause();
    void togglePlayback();
    void setPlaying(bool playing);
    bool isPlaying() const;

    void cue();
    int64_t cuePointMs() const noexcept { return cue_point_ms_.load(std::memory_order_relaxed); }
    void setCuePointMs(int64_t ms) noexcept;

    void seek(double normalized_pos);
    void seekMs(int64_t ms);

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

    void setVolume(float volume);
    float volume() const noexcept;

    void setLoopBeatIndex(int index) noexcept;
    int loopBeatIndex() const noexcept;
    void setLooping(bool looping);
    bool isLooping() const noexcept;
    int64_t loopStartMs() const noexcept;
    int64_t loopEndMs() const noexcept;
    void updateLoop();

    bool hasVideo() const noexcept { return has_video_.load(std::memory_order_relaxed); }
    bool isSpeedRestarting() const noexcept { return speed_restart_phase_.load(std::memory_order_relaxed) != 0; }
    bool hasFileChanged() const noexcept { return file_changed_.load(std::memory_order_relaxed); }
    void clearFileChanged() noexcept { file_changed_.store(false, std::memory_order_relaxed); }

    enum class DeckSide { A, B };
    void setSide(DeckSide side) noexcept { side_.store(side, std::memory_order_relaxed); }
    DeckSide side() const noexcept { return side_.load(std::memory_order_relaxed); }

    // Sync readiness for future mixer connection
    bool isSync() const noexcept { return is_sync_.load(std::memory_order_relaxed); }
    void setSync(bool s) noexcept { is_sync_.store(s, std::memory_order_relaxed); }
    void syncToBpm(double target_bpm);
    void syncPhase(int64_t beat_time_ms);

    double position() const;
    int64_t currentTimeMs() const;
    int64_t durationMs() const;

    obs_source_t *source() const noexcept { return source_; }

private:
    void applyAudioVolume() noexcept;

    obs_source_t *source_ = nullptr;

    mutable std::mutex file_mutex_;
    QString file_name_{};
    QString file_path_{};

    std::atomic<bool> looping_{false};
    std::atomic<bool> has_video_{false};
    std::atomic<bool> file_changed_{false};
    std::atomic<DeckSide> side_{DeckSide::A};
    std::atomic<int> loop_beat_index_{3}; // 4 beats default

    std::atomic<int64_t> loop_start_ms_{0};
    std::atomic<int64_t> loop_end_ms_{0};
    std::atomic<int64_t> cue_point_ms_{0};

    std::atomic<double> base_bpm_{128.0};
    std::atomic<int64_t> grid_offset_ms_{0};

    std::atomic<int> speed_percent_{100};
    std::atomic<float> volume_{1.0f};

    std::atomic<bool> is_sync_{false};
    std::atomic<int64_t> pending_speed_seek_ms_{0};
    std::atomic<int> speed_restart_phase_{0};
    std::chrono::steady_clock::time_point speed_restart_tp_{};
    std::atomic<bool> is_loop_seeking_{false};
    std::chrono::steady_clock::time_point last_loop_seek_tp_{};

    mutable std::mutex waveform_mutex_;
    std::vector<float> overview_samples_;
    std::vector<float> detailed_samples_;
};

// Global deck accessors
DeckPlayer *vj_active_deck();
DeckPlayer *vj_deck_a();
DeckPlayer *vj_deck_b();
void vdj_set_active_deck(DeckPlayer *deck);
void vdj_set_deck_a(DeckPlayer *deck);
void vdj_set_deck_b(DeckPlayer *deck);
