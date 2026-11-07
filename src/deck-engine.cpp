#include "deck-engine.hpp"

#include <obs-module.h>

#include <algorithm>
#include <cstdint>

DeckPlayer::DeckPlayer(const char *source_name, const char *display_name)
{
    obs_data_t *settings = obs_data_create();
    if (!settings) {
        blog(LOG_ERROR, "[obs-vj-mixer] Could not allocate source settings for '%s'", source_name);
        return;
    }

    obs_data_set_bool(settings, "looping", false);

    source_ = obs_source_create_private("ffmpeg_source", source_name, settings);
    obs_data_release(settings);

    if (!source_) {
        blog(LOG_WARNING, "[obs-vj-mixer] Could not create ffmpeg_source '%s' (%s)",
             source_name, display_name);
        return;
    }

    // Keep the source active even when the monitor source is not currently
    // present in an OBS scene.
    obs_source_inc_showing(source_);
}

DeckPlayer::~DeckPlayer()
{
    if (!source_)
        return;

    obs_source_dec_showing(source_);
    obs_source_release(source_);
    source_ = nullptr;
}

void DeckPlayer::loadFile(const QString &path)
{
    if (!source_ || path.isEmpty())
        return;

    obs_data_t *settings = obs_source_get_settings(source_);
    if (!settings)
        return;

    const QByteArray utf8_path = path.toUtf8();
    obs_data_set_string(settings, "local_file", utf8_path.constData());
    obs_data_set_bool(settings, "is_local_file", true);
    obs_data_set_bool(settings, "looping", looping_);

    obs_source_update(source_, settings);
    obs_data_release(settings);

    obs_source_media_restart(source_);
}

void DeckPlayer::togglePlayback()
{
    if (!source_)
        return;

    const bool pause =
        obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING;
    obs_source_media_play_pause(source_, pause);
}

void DeckPlayer::setPlaying(bool playing)
{
    if (source_)
        obs_source_media_play_pause(source_, !playing);
}

void DeckPlayer::setLooping(bool looping)
{
    looping_ = looping;

    if (!source_)
        return;

    obs_data_t *settings = obs_source_get_settings(source_);
    if (!settings)
        return;

    obs_data_set_bool(settings, "looping", looping);
    obs_source_update(source_, settings);
    obs_data_release(settings);
}

void DeckPlayer::seek(double normalized_position)
{
    if (!source_)
        return;

    const int64_t duration = obs_source_media_get_duration(source_);
    if (duration <= 0)
        return;

    const double position = std::clamp(normalized_position, 0.0, 1.0);
    const uint64_t target_ms =
        static_cast<uint64_t>(position * static_cast<double>(duration));

    obs_source_media_set_time(source_, target_ms);
}

double DeckPlayer::position() const
{
    if (!source_)
        return 0.0;

    const int64_t duration = obs_source_media_get_duration(source_);
    if (duration <= 0)
        return 0.0;

    const int64_t current = obs_source_media_get_time(source_);
    return std::clamp(
        static_cast<double>(current) / static_cast<double>(duration),
        0.0,
        1.0);
}

bool DeckPlayer::isPlaying() const
{
    return source_ &&
           obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING;
}

void DeckPlayer::setRed(float value) noexcept
{
    red_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed);
}

void DeckPlayer::setGreen(float value) noexcept
{
    green_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed);
}

void DeckPlayer::setBlue(float value) noexcept
{
    blue_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed);
}

void DeckPlayer::setSaturation(float value) noexcept
{
    saturation_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed);
}

float DeckPlayer::red() const noexcept
{
    return red_.load(std::memory_order_relaxed);
}

float DeckPlayer::green() const noexcept
{
    return green_.load(std::memory_order_relaxed);
}

float DeckPlayer::blue() const noexcept
{
    return blue_.load(std::memory_order_relaxed);
}

float DeckPlayer::saturation() const noexcept
{
    return saturation_.load(std::memory_order_relaxed);
}

DeckPair::DeckPair()
    : deck_a_("vdj_deck_a", "Deck A"),
      deck_b_("vdj_deck_b", "Deck B")
{
}

void DeckPair::setCrossfader(double value) noexcept
{
    crossfader_.store(std::clamp(value, 0.0, 1.0), std::memory_order_relaxed);
}

double DeckPair::crossfader() const noexcept
{
    return crossfader_.load(std::memory_order_relaxed);
}

void DeckPair::setTransitionMode(int mode) noexcept
{
    transition_mode_.store(std::clamp(mode, 0, 3), std::memory_order_relaxed);
}

int DeckPair::transitionMode() const noexcept
{
    return transition_mode_.load(std::memory_order_relaxed);
}

namespace {
DeckPair *g_decks = nullptr;
}

DeckPair *vdj_decks() noexcept
{
    return g_decks;
}

void vdj_set_decks(DeckPair *decks) noexcept
{
    g_decks = decks;
}
