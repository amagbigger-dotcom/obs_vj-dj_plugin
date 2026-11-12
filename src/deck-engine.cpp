#include "deck-engine.hpp"
#include "audio-analyzer.hpp"

#include <obs-module.h>
#include <algorithm>
#include <cmath>
#include <cstring>

static DeckPlayer *g_active_deck = nullptr;

DeckPlayer *vj_active_deck()
{
    return g_active_deck;
}

void vdj_set_active_deck(DeckPlayer *deck)
{
    g_active_deck = deck;
}

DeckPlayer::DeckPlayer(const char *source_name, const char *display_name)
{
    UNUSED_PARAMETER(display_name);

    obs_data_t *settings = obs_data_create();
    if (!settings)
        return;

    obs_data_set_bool(settings, "looping", false);
    obs_data_set_bool(settings, "close_when_inactive", false);
    obs_data_set_bool(settings, "restart_on_activate", false);
    obs_data_set_bool(settings, "clear_on_media_end", false);
    obs_data_set_int(settings, "speed_percent", 100);

    source_ = obs_source_create_private("ffmpeg_source", source_name, settings);
    obs_data_release(settings);

    if (!source_) {
        blog(LOG_WARNING, "[obs-vj-deck] Failed to create internal ffmpeg_source '%s'", source_name);
        return;
    }

    obs_source_inc_showing(source_);
    obs_source_inc_active(source_);
    obs_source_set_audio_active(source_, true);
    obs_source_set_audio_mixers(source_, 0x3F);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);

    applyAudioVolume();
}

DeckPlayer::~DeckPlayer()
{
    if (source_) {
        obs_source_dec_active(source_);
        obs_source_dec_showing(source_);
        obs_source_release(source_);
        source_ = nullptr;
    }
}

void DeckPlayer::loadFile(const QString &path)
{
    if (!source_ || path.isEmpty())
        return;

    setFilePath(path);
    setFileName(QFileInfo(path).fileName());

    const QString ext = QFileInfo(path).suffix().toLower();
    const bool is_vid = (ext == "mp4" || ext == "mkv" || ext == "mov" ||
                         ext == "avi" || ext == "webm" || ext == "flv" ||
                         ext == "wmv" || ext == "m4v" || ext == "ts");
    has_video_.store(is_vid, std::memory_order_relaxed);

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

    obs_source_set_audio_active(source_, true);
    obs_source_set_audio_mixers(source_, 0x3F);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
    applyAudioVolume();

    looping_.store(false, std::memory_order_relaxed);
    loop_start_ms_.store(0, std::memory_order_relaxed);
    loop_end_ms_.store(0, std::memory_order_relaxed);
    cue_point_ms_.store(0, std::memory_order_relaxed);
    grid_offset_ms_.store(0, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(waveform_mutex_);
        overview_samples_.clear();
        detailed_samples_.clear();
    }

    AudioAnalyzer::analyzeFileAsync(path, [this](TrackAnalysisResult res) {
        if (res.success) {
            setBpm(res.detected_bpm);
            setGridOffsetMs(res.grid_offset_ms);
            if (res.cue_point_ms > 0) {
                cue_point_ms_.store(res.cue_point_ms, std::memory_order_relaxed);
            }
            setWaveformSamples(res.overview_peaks, res.detailed_peaks);
        }
    });

    obs_source_media_restart(source_);
}

QString DeckPlayer::fileName() const
{
    std::lock_guard<std::mutex> lock(file_mutex_);
    return file_name_;
}

void DeckPlayer::setFileName(const QString &name)
{
    std::lock_guard<std::mutex> lock(file_mutex_);
    file_name_ = name;
}

QString DeckPlayer::filePath() const
{
    std::lock_guard<std::mutex> lock(file_mutex_);
    return file_path_;
}

void DeckPlayer::setFilePath(const QString &path)
{
    std::lock_guard<std::mutex> lock(file_mutex_);
    file_path_ = path;
}

void DeckPlayer::play()
{
    if (!source_)
        return;
    const obs_media_state state = obs_source_media_get_state(source_);
    if (state == OBS_MEDIA_STATE_PAUSED)
        obs_source_media_play_pause(source_, false);
    else if (state != OBS_MEDIA_STATE_PLAYING)
        obs_source_media_restart(source_);
}

void DeckPlayer::pause()
{
    if (!source_)
        return;
    if (obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING)
        obs_source_media_play_pause(source_, true);
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
    if (playing)
        play();
    else
        pause();
}

bool DeckPlayer::isPlaying() const
{
    if (!source_)
        return false;
    return obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING;
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
        saveUserEdits();
    }
}

void DeckPlayer::setCuePointMs(int64_t ms) noexcept
{
    cue_point_ms_.store(std::max<int64_t>(0, ms), std::memory_order_relaxed);
    saveUserEdits();
}

void DeckPlayer::seek(double normalized_pos)
{
    if (!source_)
        return;
    const int64_t dur = durationMs();
    if (dur <= 0)
        return;
    const int64_t target_ms = static_cast<int64_t>(std::clamp(normalized_pos, 0.0, 1.0) * static_cast<double>(dur));
    seekMs(target_ms);
}

void DeckPlayer::seekMs(int64_t ms)
{
    if (!source_ || ms < 0)
        return;
    obs_source_media_set_time(source_, static_cast<uint64_t>(ms));
}

void DeckPlayer::setSpeed(int percent)
{
    const int clamped = std::clamp(percent, 50, 150);
    speed_percent_.store(clamped, std::memory_order_relaxed);
}

void DeckPlayer::applySpeedSettings(int speed_percent)
{
    if (!source_)
        return;

    const int clamped = std::clamp(speed_percent, 50, 150);
    speed_percent_.store(clamped, std::memory_order_relaxed);

    const int64_t actual = obs_source_media_get_time(source_);
    if (actual > 100) {
        pending_speed_seek_ms_.store(actual, std::memory_order_relaxed);
        is_restarting_for_speed_.store(true, std::memory_order_relaxed);
    }

    obs_data_t *settings = obs_source_get_settings(source_);
    if (!settings)
        return;

    obs_data_set_int(settings, "speed_percent", clamped);
    obs_source_update(source_, settings);
    obs_data_release(settings);
}

int DeckPlayer::speed() const noexcept
{
    return speed_percent_.load(std::memory_order_relaxed);
}

void DeckPlayer::setBpm(double bpm) noexcept
{
    base_bpm_.store(std::clamp(bpm, 40.0, 300.0), std::memory_order_relaxed);
    saveUserEdits();
}

double DeckPlayer::bpm() const noexcept
{
    return base_bpm_.load(std::memory_order_relaxed);
}

double DeckPlayer::effectiveBpm() const noexcept
{
    const double b = bpm();
    const double spd = static_cast<double>(speed()) / 100.0;
    return b * spd;
}

double DeckPlayer::beatDurationMs() const noexcept
{
    const double eff = effectiveBpm();
    return (eff > 1.0) ? (60000.0 / eff) : 500.0;
}

void DeckPlayer::setGridOffsetMs(int64_t offset_ms) noexcept
{
    grid_offset_ms_.store(offset_ms, std::memory_order_relaxed);
}

int64_t DeckPlayer::gridOffsetMs() const noexcept
{
    return grid_offset_ms_.load(std::memory_order_relaxed);
}

void DeckPlayer::nudgeGrid(int64_t delta_ms) noexcept
{
    const double beat_ms = beatDurationMs();
    int64_t cur = grid_offset_ms_.load(std::memory_order_relaxed);
    cur += delta_ms;
    if (beat_ms > 20.0) {
        const int64_t b_ms = static_cast<int64_t>(beat_ms);
        cur = ((cur % b_ms) + b_ms) % b_ms;
    }
    grid_offset_ms_.store(cur, std::memory_order_relaxed);
    saveUserEdits();
}

void DeckPlayer::setGridAnchorAtCurrent() noexcept
{
    const int64_t now = currentTimeMs();
    const double beat_ms = beatDurationMs();
    int64_t offset = now;
    if (beat_ms > 20.0) {
        const int64_t b_ms = static_cast<int64_t>(beat_ms);
        offset = ((now % b_ms) + b_ms) % b_ms;
    }
    grid_offset_ms_.store(offset, std::memory_order_relaxed);
    saveUserEdits();
}

void DeckPlayer::saveUserEdits()
{
    const QString path = filePath();
    if (path.isEmpty())
        return;
    AudioAnalyzer::updateCacheUserEdits(path, bpm(), gridOffsetMs(), cue_point_ms_.load(std::memory_order_relaxed));
}

void DeckPlayer::setWaveformSamples(const std::vector<float> &overview, const std::vector<float> &detailed)
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    overview_samples_ = overview;
    detailed_samples_ = detailed;
}

std::vector<float> DeckPlayer::overviewSamples() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return overview_samples_;
}

std::vector<float> DeckPlayer::detailedSamples() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return detailed_samples_;
}

std::vector<float> DeckPlayer::waveformSamples() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return overview_samples_;
}

bool DeckPlayer::hasAnalyzedWaveform() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return !overview_samples_.empty() || !detailed_samples_.empty();
}

void DeckPlayer::setVolume(float volume)
{
    volume_.store(std::clamp(volume, 0.0f, 2.0f), std::memory_order_relaxed);
    applyAudioVolume();
}

float DeckPlayer::volume() const noexcept
{
    return volume_.load(std::memory_order_relaxed);
}

void DeckPlayer::applyAudioVolume() noexcept
{
    if (source_) {
        obs_source_set_volume(source_, volume_.load(std::memory_order_relaxed));
    }
}

void DeckPlayer::setLoopBeatIndex(int index) noexcept
{
    loop_beat_index_.store(std::clamp(index, 0, 6), std::memory_order_relaxed);
}

int DeckPlayer::loopBeatIndex() const noexcept
{
    return loop_beat_index_.load(std::memory_order_relaxed);
}

void DeckPlayer::setLooping(bool looping)
{
    if (!looping) {
        looping_.store(false, std::memory_order_relaxed);
        return;
    }

    static const double BEAT_MULTIPLIERS[] = {0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0};
    const int idx = loop_beat_index_.load(std::memory_order_relaxed);
    const double num_beats = BEAT_MULTIPLIERS[std::clamp(idx, 0, 6)];

    const int64_t start = currentTimeMs();
    const double beat_ms = beatDurationMs();
    const int64_t loop_len = static_cast<int64_t>(std::round(num_beats * beat_ms));
    const int64_t end = start + std::max<int64_t>(100, loop_len);

    loop_start_ms_.store(start, std::memory_order_relaxed);
    loop_end_ms_.store(end, std::memory_order_relaxed);
    looping_.store(true, std::memory_order_relaxed);
}

bool DeckPlayer::isLooping() const noexcept
{
    return looping_.load(std::memory_order_relaxed);
}

int64_t DeckPlayer::loopStartMs() const noexcept
{
    return loop_start_ms_.load(std::memory_order_relaxed);
}

int64_t DeckPlayer::loopEndMs() const noexcept
{
    return loop_end_ms_.load(std::memory_order_relaxed);
}

void DeckPlayer::updateLoop()
{
    if (!source_)
        return;

    // 1. Resume saved position after speed restart
    if (is_restarting_for_speed_.load(std::memory_order_relaxed)) {
        const int64_t seek_target = pending_speed_seek_ms_.load(std::memory_order_relaxed);
        const int64_t cur = obs_source_media_get_time(source_);
        if (cur < 300 || cur < seek_target / 2) {
            obs_source_media_set_time(source_, static_cast<uint64_t>(seek_target));
            is_restarting_for_speed_.store(false, std::memory_order_relaxed);
            pending_speed_seek_ms_.store(0, std::memory_order_relaxed);
        }
    }

    // 2. Seamless Auto-Loop execution
    if (!isLooping()) {
        is_loop_seeking_.store(false, std::memory_order_relaxed);
        return;
    }

    const int64_t current = obs_source_media_get_time(source_);
    const int64_t loop_end = loop_end_ms_.load(std::memory_order_relaxed);
    const int64_t loop_start = loop_start_ms_.load(std::memory_order_relaxed);

    if (loop_end <= loop_start || current < loop_end) {
        is_loop_seeking_.store(false, std::memory_order_relaxed);
        return;
    }

    if (!is_loop_seeking_.load(std::memory_order_relaxed)) {
        obs_source_media_set_time(source_, static_cast<uint64_t>(loop_start));
        is_loop_seeking_.store(true, std::memory_order_relaxed);
        last_loop_seek_tp_ = std::chrono::steady_clock::now();
    } else {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_loop_seek_tp_).count();
        if (diff > 120 && current < loop_end) {
            is_loop_seeking_.store(false, std::memory_order_relaxed);
        }
    }
}

void DeckPlayer::syncToBpm(double target_bpm)
{
    const double base = bpm();
    if (base < 10.0 || target_bpm < 10.0)
        return;

    const int target_speed = std::clamp(
        static_cast<int>(std::round((target_bpm / base) * 100.0)),
        50, 150);
    applySpeedSettings(target_speed);
}

void DeckPlayer::syncPhase(int64_t beat_time_ms)
{
    if (!source_ || beat_time_ms < 0)
        return;

    const double beat_ms = beatDurationMs();
    if (beat_ms < 10.0)
        return;

    const int64_t cur = currentTimeMs();
    const int64_t diff = (cur - beat_time_ms);
    const int64_t phase = ((diff % static_cast<int64_t>(beat_ms)) + static_cast<int64_t>(beat_ms)) % static_cast<int64_t>(beat_ms);

    if (phase > static_cast<int64_t>(beat_ms / 2.0)) {
        // Nudge forward to next beat
        obs_source_media_set_time(source_, static_cast<uint64_t>(cur + (static_cast<int64_t>(beat_ms) - phase)));
    } else {
        // Nudge back to beat
        obs_source_media_set_time(source_, static_cast<uint64_t>(std::max<int64_t>(0, cur - phase)));
    }
}

double DeckPlayer::position() const
{
    const int64_t dur = durationMs();
    if (dur <= 0)
        return 0.0;
    return static_cast<double>(currentTimeMs()) / static_cast<double>(dur);
}

int64_t DeckPlayer::currentTimeMs() const
{
    if (!source_)
        return 0;
    return obs_source_media_get_time(source_);
}

int64_t DeckPlayer::durationMs() const
{
    if (!source_)
        return 0;
    return obs_source_media_get_duration(source_);
}
