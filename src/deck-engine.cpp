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

    // KÖTELEZŐ: private source, hogy az OBS jelenet-tisztítója ne tekintse szivárgásnak
    source_ = obs_source_create_private("ffmpeg_source", source_name, settings);
    obs_data_release(settings);

    if (!source_) {
        blog(LOG_WARNING, "[obs-vj-mixer] Could not create ffmpeg_source '%s' (%s)",
             source_name, display_name);
        return;
    }

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

    obs_data_set_string(settings, "local_file", path.toUtf8().constData());
    obs_data_set_bool(settings, "is_local_file", true);
    obs_source_update(source_, settings);
    obs_data_release(settings);

    obs_source_media_restart(source_);
    playback_state_.store(State::Playing, std::memory_order_release);
}

void DeckPlayer::play()
{
    if (!source_)
        return;
    obs_source_media_play_pause(source_, false);
    playback_state_.store(State::Playing, std::memory_order_release);
}

void DeckPlayer::pause()
{
    if (!source_)
        return;
    obs_source_media_play_pause(source_, true);
    playback_state_.store(State::Paused, std::memory_order_release);
}

void DeckPlayer::stop()
{
    if (!source_)
        return;
    obs_source_media_restart(source_);
    obs_source_media_play_pause(source_, true);
    playback_state_.store(State::Stopped, std::memory_order_release);
}

void DeckPlayer::seekMs(int64_t ms)
{
    if (!source_)
        return;
    obs_source_media_set_time(source_, ms);
}

int64_t DeckPlayer::durationMs() const
{
    if (!source_)
        return 0;
    return obs_source_media_get_duration(source_);
}

int64_t DeckPlayer::positionMs() const
{
    if (!source_)
        return 0;
    return obs_source_media_get_time(source_);
}

DeckPair::DeckPair()
    : deck_a_("vdj_deck_a", "Deck A"),
      deck_b_("vdj_deck_b", "Deck B")
{
}

void DeckPair::setCrossfader(float value)
{
    crossfader_.store(std::clamp(value, 0.0f, 1.0f), std::memory_order_release);
}

void DeckPair::setTransitionMode(int mode)
{
    transition_mode_.store(mode, std::memory_order_release);
}

static DeckPair *g_decks_instance = nullptr;

void vdj_set_decks(DeckPair *decks)
{
    g_decks_instance = decks;
}

DeckPair *vdj_get_decks()
{
    return g_decks_instance;
}
