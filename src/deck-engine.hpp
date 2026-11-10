#pragma once

#include <obs-module.h>
#include <QString>
#include <atomic>
#include <cstdint>

class DeckPlayer final {
public:
    DeckPlayer(const char *source_name, const char *display_name);
    ~DeckPlayer();

    DeckPlayer(const DeckPlayer &) = delete;
    DeckPlayer &operator=(const DeckPlayer &) = delete;

    obs_source_t *source() const noexcept { return source_; }

    void loadFile(const QString &path);
    void togglePlayback();
    void setPlaying(bool playing);
    void setLooping(bool looping);
    void setLoopBeatIndex(int index) noexcept;
    void updateLoop();
    void seek(double normalized_position);
    void cue();

    void setSpeed(int percent);
    int speed() const noexcept;

    void setVolume(float volume);
    float volume() const noexcept;

    void setMixGain(float gain) noexcept;
    void enableAudioMonitoring() noexcept;

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

    std::atomic<bool> looping_{false};
    std::atomic<int> loop_beat_index_{3};

    std::atomic<int64_t> loop_start_ms_{0};
    std::atomic<int64_t> loop_end_ms_{0};
    std::atomic<int64_t> cue_point_ms_{0};

    std::atomic<int> speed_percent_{100};
    std::atomic<float> volume_{1.0f};
    std::atomic<float> mix_gain_{1.0f};

    std::atomic<float> red_{1.0f};
    std::atomic<float> green_{1.0f};
    std::atomic<float> blue_{1.0f};
    std::atomic<float> saturation_{1.0f};
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
    std::atomic<int> loop_beat_idx_a_{3};
    std::atomic<int> loop_beat_idx_b_{3};
};

DeckPair *vdj_decks() noexcept;
void vdj_set_decks(DeckPair *decks) noexcept;
