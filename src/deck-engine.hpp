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
    void seek(double normalized_position);
    void cue();
    void setSpeed(int percent);
    int speed() const noexcept { return speed_percent_.load(std::memory_order_relaxed); }
    void setVolume(float vol);
    float volume() const noexcept { return volume_.load(std::memory_order_relaxed); }

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
    bool isLooping() const noexcept { return looping_; }

private:
    obs_source_t *source_ = nullptr;
    bool looping_ = false;
    uint64_t cue_point_ms_ = 0;
    std::atomic<int> speed_percent_{100};
    std::atomic<float> volume_{1.0f};
    std::atomic<float> red_{1.0f};
    std::atomic<float> green_{1.0f};
    std::atomic<float> blue_{1.0f};
    std::atomic<float> saturation_{1.0f};
};

using DeckEngine = DeckPlayer;

class DeckPair final {
public:
    DeckPair();

    DeckPlayer &deckA() noexcept { return deck_a_; }
    DeckPlayer &deckB() noexcept { return deck_b_; }

    void setCrossfader(double value) noexcept;
    double crossfader() const noexcept;

    void setTransitionMode(int mode) noexcept;
    int transitionMode() const noexcept;

    void syncTempoAtoB();
    void syncTempoBtoA();

    void setLoopBeatIndex(int deck_idx, int beat_index);
    int getLoopBeatIndex(int deck_idx) const noexcept;

private:
    DeckPlayer deck_a_;
    DeckPlayer deck_b_;
    std::atomic<double> crossfader_{0.5};
    std::atomic<int> transition_mode_{0};
    std::atomic<int> loop_beat_idx_a_{3};
    std::atomic<int> loop_beat_idx_b_{3};
};

DeckPair *vdj_decks() noexcept;
void vdj_set_decks(DeckPair *decks) noexcept;
