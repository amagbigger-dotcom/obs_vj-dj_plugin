#pragma once

#include <obs.h>
#include <QString>

#include <atomic>

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

    void setRed(float value) noexcept;
    void setGreen(float value) noexcept;
    void setBlue(float value) noexcept;
    void setSaturation(float value) noexcept;

    float red() const noexcept;
    float green() const noexcept;
    float blue() const noexcept;
    float saturation() const noexcept;

    double position() const;
    bool isPlaying() const;
    bool isLooping() const noexcept { return looping_; }

private:
    obs_source_t *source_ = nullptr;
    bool looping_ = false;

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

private:
    DeckPlayer deck_a_;
    DeckPlayer deck_b_;

    std::atomic<double> crossfader_{0.5};
    std::atomic<int> transition_mode_{0};
};

DeckPair *vdj_decks() noexcept;
void vdj_set_decks(DeckPair *decks) noexcept;
