#include "deck-engine.hpp"

#include <obs-module.h>
#include <algorithm>

namespace {
DeckPair *g_decks = nullptr;
constexpr double kReferenceBeatMs = 500.0;
constexpr int kLoopBeatCounts[] = {0, 1, 2, 4, 8, 16, 32, 64};
} // namespace

DeckPlayer::DeckPlayer(const char *source_name, const char *display_name)
{
    obs_data_t *settings = obs_data_create();
    if (!settings) {
        blog(LOG_ERROR, "[obs-vj-mixer] Could not allocate settings for '%s'", source_name);
        return;
    }

    obs_data_set_bool(settings, "looping", false);
    obs_data_set_bool(settings, "close_when_inactive", false);
    obs_data_set_bool(settings, "restart_on_activate", false);
    obs_data_set_bool(settings, "clear_on_media_end", false);
    obs_data_set_int(settings, "speed_percent", 100);

    source_ = obs_source_create_private("ffmpeg_source", source_name, settings);
    obs_data_release(settings);
    if (!source_) {
        blog(LOG_WARNING, "[obs-vj-mixer] Could not create ffmpeg_source '%s' (%s)", source_name, display_name);
        return;
    }

    obs_source_inc_showing(source_);
    // OBS's monitoring device selection controls the physical playback device.
    obs_source_set_monitoring_enabled(source_, true);
}

DeckPlayer::~DeckPlayer()
{
    if (!source_)
        return;
    obs_source_set_monitoring_enabled(source_, false);
    obs_source_dec_showing(source_);
    obs_source_release(source_);
    source_ = nullptr;
}

bool DeckPlayer::hasMedia() const
{
    return source_ && (obs_source_get_width(source_) > 0 || durationMs() > 0);
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
    obs_data_set_bool(settings, "looping", false);
    obs_data_set_bool(settings, "close_when_inactive", false);
    obs_data_set_bool(settings, "restart_on_activate", false);
    obs_data_set_bool(settings, "clear_on_media_end", false);
    obs_data_set_int(settings, "speed_percent", speed_percent_.load(std::memory_order_relaxed));
    obs_source_update(source_, settings);
    obs_data_release(settings);

    // Keep monitoring enabled after ffmpeg_source updates its settings.
    obs_source_set_monitoring_enabled(source_, true);
    looping_.store(false, std::memory_order_relaxed);
    loop_start_ms_.store(0, std::memory_order_relaxed);
    loop_end_ms_.store(0, std::memory_order_relaxed);
    cue_point_ms_.store(0, std::memory_order_relaxed);
    obs_source_media_restart(source_);
}

void DeckPlayer::togglePlayback()
{
    if (!source_)
        return;
    const obs_media_state state = obs_source_media_get_state(source_);
    if (state == OBS_MEDIA_STATE_PLAYING)
        obs_source_media_play_pause(source_, true);
    else if (state == OBS_MEDIA_STATE_PAUSED)
        obs_source_media_play_pause(source_, false);
    else
        obs_source_media_restart(source_);
}

void DeckPlayer::setPlaying(bool playing)
{
    if (!source_)
        return;
    const obs_media_state state = obs_source_media_get_state(source_);
    if (!playing) {
        if (state == OBS_MEDIA_STATE_PLAYING)
            obs_source_media_play_pause(source_, true);
    } else if (state == OBS_MEDIA_STATE_PAUSED) {
        obs_source_media_play_pause(source_, false);
    } else if (state != OBS_MEDIA_STATE_PLAYING) {
        obs_source_media_restart(source_);
    }
}

void DeckPlayer::cue()
{
    if (!source_)
        return;
    const obs_media_state state = obs_source_media_get_state(source_);
    if (state == OBS_MEDIA_STATE_PLAYING) {
        const int64_t cue = std::max<int64_t>(0, cue_point_ms_.load(std::memory_order_relaxed));
        obs_source_media_play_pause(source_, true);
        obs_source_media_set_time(source_, static_cast<uint64_t>(cue));
    } else {
        const int64_t now = std::max<int64_t>(0, obs_source_media_get_time(source_));
        cue_point_ms_.store(now, std::memory_order_relaxed);
        obs_source_media_set_time(source_, static_cast<uint64_t>(now));
    }
}

void DeckPlayer::setSpeed(int percent)
{
    const int clamped = std::clamp(percent, 50, 150);
    speed_percent_.store(clamped, std::memory_order_relaxed);
    if (!source_)
        return;
    const int64_t current_time = obs_source_media_get_time(source_);
    obs_data_t *settings = obs_source_get_settings(source_);
    if (!settings)
        return;
    obs_data_set_int(settings, "speed_percent", clamped);
    obs_source_update(source_, settings);
    obs_data_release(settings);
    obs_source_set_monitoring_enabled(source_, true);
    if (current_time > 0)
        obs_source_media_set_time(source_, static_cast<uint64_t>(current_time));
}

int DeckPlayer::speed() const noexcept
{
    return speed_percent_.load(std::memory_order_relaxed);
}

void DeckPlayer::setVolume(float volume)
{
    volume_.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
    applyAudioVolume();
}

float DeckPlayer::volume() const noexcept
{
    return volume_.load(std::memory_order_relaxed);
}

void DeckPlayer::setMixGain(float gain) noexcept
{
    mix_gain_.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_relaxed);
    applyAudioVolume();
}

void DeckPlayer::applyAudioVolume() noexcept
{
    if (source_)
        obs_source_set_volume(source_, volume_.load(std::memory_order_relaxed) * mix_gain_.load(std::memory_order_relaxed));
}

void DeckPlayer::setLoopBeatIndex(int index) noexcept
{
    loop_beat_index_.store(std::clamp(index, 0, 7), std::memory_order_relaxed);
    if (isLooping())
        setLooping(true);
}

void DeckPlayer::setLooping(bool enabled)
{
    if (!enabled) {
        looping_.store(false, std::memory_order_relaxed);
        loop_end_ms_.store(0, std::memory_order_relaxed);
        return;
    }
    if (!source_ || durationMs() <= 0)
        return;
    const int index = loop_beat_index_.load(std::memory_order_relaxed);
    const double beats = index == 0 ? 0.5 : static_cast<double>(kLoopBeatCounts[index]);
    const int64_t start = std::max<int64_t>(0, currentTimeMs());
    const int64_t loop_length = std::max<int64_t>(1, static_cast<int64_t>(beats * kReferenceBeatMs));
    const int64_t end = std::min<int64_t>(durationMs(), start + loop_length);
    if (end <= start)
        return;
    loop_start_ms_.store(start, std::memory_order_relaxed);
    loop_end_ms_.store(end, std::memory_order_relaxed);
    looping_.store(true, std::memory_order_relaxed);
}

void DeckPlayer::updateLoop()
{
    if (!isLooping() || !source_ || !isPlaying())
        return;
    const int64_t start = loop_start_ms_.load(std::memory_order_relaxed);
    const int64_t end = loop_end_ms_.load(std::memory_order_relaxed);
    if (end > start && currentTimeMs() >= end)
        obs_source_media_set_time(source_, static_cast<uint64_t>(start));
}

void DeckPlayer::seek(double normalized_position)
{
    if (!source_)
        return;
    const int64_t duration = durationMs();
    if (duration <= 0)
        return;
    const double clamped = std::clamp(normalized_position, 0.0, 1.0);
    obs_source_media_set_time(source_, static_cast<uint64_t>(clamped * static_cast<double>(duration)));
    if (isLooping())
        setLooping(true);
}

double DeckPlayer::position() const
{
    const int64_t duration = durationMs();
    if (duration <= 0)
        return 0.0;
    return std::clamp(static_cast<double>(currentTimeMs()) / static_cast<double>(duration), 0.0, 1.0);
}

int64_t DeckPlayer::currentTimeMs() const { return source_ ? obs_source_media_get_time(source_) : 0; }
int64_t DeckPlayer::durationMs() const { return source_ ? obs_source_media_get_duration(source_) : 0; }
bool DeckPlayer::isPlaying() const { return source_ && obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING; }
bool DeckPlayer::isLooping() const noexcept { return looping_.load(std::memory_order_relaxed); }

void DeckPlayer::setRed(float value) noexcept { red_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }
void DeckPlayer::setGreen(float value) noexcept { green_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }
void DeckPlayer::setBlue(float value) noexcept { blue_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }
void DeckPlayer::setSaturation(float value) noexcept { saturation_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }
float DeckPlayer::red() const noexcept { return red_.load(std::memory_order_relaxed); }
float DeckPlayer::green() const noexcept { return green_.load(std::memory_order_relaxed); }
float DeckPlayer::blue() const noexcept { return blue_.load(std::memory_order_relaxed); }
float DeckPlayer::saturation() const noexcept { return saturation_.load(std::memory_order_relaxed); }

DeckPair::DeckPair()
    : deck_a_("vdj_deck_a", "Deck A"), deck_b_("vdj_deck_b", "Deck B")
{
    obs_data_t *settings = obs_data_create();
    if (settings) {
        master_source_ = obs_source_create_private("vdj_master_mixer", "VJ Master Preview", settings);
        obs_data_release(settings);
    }
    if (master_source_)
        obs_source_inc_showing(master_source_);
    updateAudioMix();
}

DeckPair::~DeckPair()
{
    if (master_source_) {
        obs_source_dec_showing(master_source_);
        obs_source_release(master_source_);
        master_source_ = nullptr;
    }
}

void DeckPair::setCrossfader(double value) noexcept
{
    crossfader_.store(std::clamp(value, 0.0, 1.0), std::memory_order_relaxed);
    updateAudioMix();
}
double DeckPair::crossfader() const noexcept { return crossfader_.load(std::memory_order_relaxed); }
void DeckPair::setTransitionMode(int mode) noexcept { transition_mode_.store(std::clamp(mode, 0, 3), std::memory_order_relaxed); }
int DeckPair::transitionMode() const noexcept { return transition_mode_.load(std::memory_order_relaxed); }
void DeckPair::syncTempoAtoB() { deck_b_.setSpeed(deck_a_.speed()); }
void DeckPair::syncTempoBtoA() { deck_a_.setSpeed(deck_b_.speed()); }

void DeckPair::setLoopBeatIndex(int deck_index, int beat_index)
{
    const int clamped = std::clamp(beat_index, 0, 7);
    if (deck_index == 0) {
        loop_beat_idx_a_.store(clamped, std::memory_order_relaxed);
        deck_a_.setLoopBeatIndex(clamped);
    } else {
        loop_beat_idx_b_.store(clamped, std::memory_order_relaxed);
        deck_b_.setLoopBeatIndex(clamped);
    }
}
int DeckPair::getLoopBeatIndex(int deck_index) const noexcept
{
    return deck_index == 0 ? loop_beat_idx_a_.load(std::memory_order_relaxed) : loop_beat_idx_b_.load(std::memory_order_relaxed);
}
void DeckPair::updateAudioMix() noexcept
{
    const double x = crossfader();
    deck_a_.setMixGain(static_cast<float>(1.0 - x));
    deck_b_.setMixGain(static_cast<float>(x));
}
DeckPair *vdj_decks() noexcept { return g_decks; }
void vdj_set_decks(DeckPair *decks) noexcept { g_decks = decks; }
