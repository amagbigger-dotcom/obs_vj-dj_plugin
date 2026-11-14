#include "deck-engine.hpp"
#include "audio-analyzer.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <algorithm>
#include <cmath>
#include <cstring>

static DeckPlayer *g_active_deck = nullptr;
static DeckPlayer *g_deck_a = nullptr;
static DeckPlayer *g_deck_b = nullptr;
static DeckPair *g_decks = nullptr;

DeckPlayer *vj_active_deck()
{
    if (g_active_deck)
        return g_active_deck;
    return g_deck_a;
}

DeckPlayer *vj_deck_a()
{
    return g_deck_a;
}

DeckPlayer *vj_deck_b()
{
    return g_deck_b;
}

void vdj_set_active_deck(DeckPlayer *deck)
{
    g_active_deck = deck;
}

void vdj_set_deck_a(DeckPlayer *deck)
{
    g_deck_a = deck;
    if (deck) deck->setSide(DeckPlayer::DeckSide::A);
}

void vdj_set_deck_b(DeckPlayer *deck)
{
    g_deck_b = deck;
    if (deck) deck->setSide(DeckPlayer::DeckSide::B);
}

DeckPair *vdj_decks() noexcept
{
    return g_decks;
}

void vdj_set_decks(DeckPair *decks) noexcept
{
    g_decks = decks;
}

DeckPlayer::DeckPlayer(const char *source_name, const char *display_name)
{
    is_deck_a_ = (display_name && (std::strstr(display_name, "Deck A") != nullptr || std::strstr(display_name, "A") != nullptr));
    if (!is_deck_a_ && source_name && std::strstr(source_name, "source_a") != nullptr) {
        is_deck_a_ = true;
    }

    obs_data_t *settings = obs_data_create();
    if (!settings)
        return;

    obs_data_set_bool(settings, "looping", false);
    obs_data_set_bool(settings, "close_when_inactive", false);
    obs_data_set_bool(settings, "restart_on_activate", false);
    obs_data_set_bool(settings, "clear_on_media_end", false);
    obs_data_set_bool(settings, "hw_decode", true);
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
    obs_source_set_audio_mixers(source_, 0);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);

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
    obs_data_set_bool(settings, "hw_decode", true);
    obs_data_set_int(settings, "speed_percent", speed_percent_.load(std::memory_order_relaxed));

    obs_source_update(source_, settings);
    obs_data_release(settings);

    obs_source_set_audio_active(source_, true);
    obs_source_set_audio_mixers(source_, 0);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);
    applyAudioVolume();

    looping_.store(false, std::memory_order_relaxed);
    loop_start_ms_.store(0, std::memory_order_relaxed);
    loop_end_ms_.store(0, std::memory_order_relaxed);
    cue_point_ms_.store(0, std::memory_order_relaxed);
    grid_offset_ms_.store(0, std::memory_order_relaxed);
    file_changed_.store(true, std::memory_order_relaxed);

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

    // Do NOT auto-play on media load: initialize paused at 00:00 (CUE)
    obs_source_media_play_pause(source_, true);
    obs_source_media_set_time(source_, 0);
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
    is_sync_.store(false, std::memory_order_relaxed);
}

void DeckPlayer::applySpeedSettings(int speed_percent)
{
    if (!source_)
        return;

    const int clamped = std::clamp(speed_percent, 50, 150);
    speed_percent_.store(clamped, std::memory_order_relaxed);
    is_sync_.store(false, std::memory_order_relaxed);

    if (speed_restart_phase_.load(std::memory_order_relaxed) == 0) {
        const int64_t actual = obs_source_media_get_time(source_);
        if (actual > 300) {
            pending_speed_seek_ms_.store(actual, std::memory_order_relaxed);
            speed_restart_phase_.store(1, std::memory_order_relaxed);
            speed_restart_tp_ = std::chrono::steady_clock::now();
        }
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
    if (is_sync_.load(std::memory_order_relaxed) && !is_master_.load(std::memory_order_relaxed)) {
        return target_sync_bpm_.load(std::memory_order_relaxed);
    }
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

void DeckPlayer::setEqHi(float val) noexcept
{
    const float cl = std::clamp(val, 0.0f, 2.0f);
    eq_hi_.store(cl, std::memory_order_relaxed);
    setRed(cl);
}

void DeckPlayer::setEqMid(float val) noexcept
{
    const float cl = std::clamp(val, 0.0f, 2.0f);
    eq_mid_.store(cl, std::memory_order_relaxed);
    setGreen(cl);
}

void DeckPlayer::setEqLow(float val) noexcept
{
    const float cl = std::clamp(val, 0.0f, 2.0f);
    eq_low_.store(cl, std::memory_order_relaxed);
    setBlue(cl);
}

void DeckPlayer::setFilterVal(float val) noexcept
{
    const float cl = std::clamp(val, -1.0f, 1.0f);
    filter_val_.store(cl, std::memory_order_relaxed);
    setSaturation(1.0f + cl);
}

float DeckPlayer::eqHi() const noexcept { return eq_hi_.load(std::memory_order_relaxed); }
float DeckPlayer::eqMid() const noexcept { return eq_mid_.load(std::memory_order_relaxed); }
float DeckPlayer::eqLow() const noexcept { return eq_low_.load(std::memory_order_relaxed); }
float DeckPlayer::filterVal() const noexcept { return filter_val_.load(std::memory_order_relaxed); }

void DeckPlayer::setRed(float value) noexcept { red_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }
void DeckPlayer::setGreen(float value) noexcept { green_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }
void DeckPlayer::setBlue(float value) noexcept { blue_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }
void DeckPlayer::setSaturation(float value) noexcept { saturation_.store(std::clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); }

float DeckPlayer::red() const noexcept { return red_.load(std::memory_order_relaxed); }
float DeckPlayer::green() const noexcept { return green_.load(std::memory_order_relaxed); }
float DeckPlayer::blue() const noexcept { return blue_.load(std::memory_order_relaxed); }
float DeckPlayer::saturation() const noexcept { return saturation_.load(std::memory_order_relaxed); }

void DeckPlayer::setGain(float gain) noexcept
{
    gain_.store(std::clamp(gain, 0.0f, 2.0f), std::memory_order_relaxed);
    applyAudioVolume();
}

float DeckPlayer::gain() const noexcept
{
    return gain_.load(std::memory_order_relaxed);
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

float DeckPlayer::mixGain() const noexcept
{
    return mix_gain_.load(std::memory_order_relaxed);
}

void DeckPlayer::enableAudioMonitoring() noexcept
{
    if (source_) {
        obs_source_set_audio_active(source_, true);
        obs_source_set_audio_mixers(source_, 0);
    }
}

void DeckPlayer::setHeadphoneCue(bool cue) noexcept
{
    headphone_cue_.store(cue, std::memory_order_relaxed);
}

bool DeckPlayer::isHeadphoneCue() const noexcept
{
    return headphone_cue_.load(std::memory_order_relaxed);
}

void DeckPlayer::applyAudioVolume() noexcept
{
    if (source_) {
        obs_source_set_volume(source_, 1.0f);
    }
}

void DeckPlayer::processAudioDsp(float *channel_data, size_t num_frames, size_t channel_idx, size_t sample_rate)
{
    if (!channel_data || num_frames == 0)
        return;

    const float g_hi = eq_hi_.load(std::memory_order_relaxed);
    const float g_mid = eq_mid_.load(std::memory_order_relaxed);
    const float g_low = eq_low_.load(std::memory_order_relaxed);
    const float f_val = filter_val_.load(std::memory_order_relaxed);

    const size_t ch = channel_idx % 2;
    FilterState &st = dsp_states_[ch];

    const float sr = (sample_rate > 0) ? static_cast<float>(sample_rate) : 48000.0f;
    const float w_low = 2.0f * 3.14159265f * 260.0f / sr;
    const float alpha_low = std::clamp(w_low / (1.0f + w_low), 0.001f, 0.95f);

    const float w_high = 2.0f * 3.14159265f * 3200.0f / sr;
    const float alpha_high = std::clamp(w_high / (1.0f + w_high), 0.001f, 0.95f);

    // Initialize smoothed values on first block
    if (st.cur_low < 0.001f) st.cur_low = g_low;
    if (st.cur_mid < 0.001f) st.cur_mid = g_mid;
    if (st.cur_hi < 0.001f)  st.cur_hi  = g_hi;

    const float smooth_rate = 0.002f; // ~10 ms smooth time at 48kHz (imperceptible latency, zero crackle)

    for (size_t i = 0; i < num_frames; ++i) {
        const float in = channel_data[i];

        // Smooth parameters per sample to eliminate all zipper noise & crackle
        st.cur_low += (g_low - st.cur_low) * smooth_rate;
        st.cur_mid += (g_mid - st.cur_mid) * smooth_rate;
        st.cur_hi  += (g_hi  - st.cur_hi)  * smooth_rate;
        st.cur_flt += (f_val - st.cur_flt) * smooth_rate;

        st.lp += alpha_low * (in - st.lp);
        st.hp += alpha_high * (in - st.hp);

        const float low_band = st.lp;
        const float high_band = in - st.hp;
        const float mid_band = in - low_band - high_band;

        float out = low_band * st.cur_low + mid_band * st.cur_mid + high_band * st.cur_hi;

        // DJ Filter: continuous tracking prevents stale DC offsets and pops
        if (st.cur_flt < -0.001f) {
            // Low Pass: cutoff from ~18000 Hz down to 200 Hz
            const float norm = 1.0f + st.cur_flt;
            const float cutoff = 200.0f + norm * norm * 17800.0f;
            const float a_lp = std::clamp(2.0f * 3.14159265f * cutoff / sr, 0.001f, 0.95f);
            st.dj_lp += a_lp * (out - st.dj_lp);
            st.dj_hp = out;
            out = st.dj_lp;
        } else if (st.cur_flt > 0.001f) {
            // High Pass: cutoff from ~20 Hz up to 8000 Hz
            const float norm = st.cur_flt;
            const float cutoff = 20.0f + norm * norm * 7980.0f;
            const float a_hp = std::clamp(2.0f * 3.14159265f * cutoff / sr, 0.001f, 0.95f);
            st.dj_hp += a_hp * (out - st.dj_hp);
            st.dj_lp = out;
            out = out - st.dj_hp;
        } else {
            st.dj_lp = out;
            st.dj_hp = 0.0f;
        }

        channel_data[i] = std::clamp(out, -1.2f, 1.2f);
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

    const int64_t current = currentTimeMs();
    const double beat_ms = beatDurationMs();
    const int64_t loop_len = static_cast<int64_t>(std::round(num_beats * beat_ms));

    int64_t start = current;
    if (beat_ms > 20.0) {
        const int64_t grid_off = gridOffsetMs();
        const double rel = static_cast<double>(current - grid_off) / beat_ms;
        const int64_t beat_idx = static_cast<int64_t>(std::round(rel));
        const int64_t quant_start = grid_off + static_cast<int64_t>(std::round(beat_idx * beat_ms));
        if (std::abs(current - quant_start) < static_cast<int64_t>(beat_ms * 0.85) && quant_start >= 0) {
            start = quant_start;
        }
    }
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

    // 1. Resume saved position after speed restart without jumping to 0:00
    const int restart_phase = speed_restart_phase_.load(std::memory_order_relaxed);
    if (restart_phase >= 1) {
        const obs_media_state state = obs_source_media_get_state(source_);
        const int64_t seek_target = pending_speed_seek_ms_.load(std::memory_order_relaxed);
        const int64_t cur = obs_source_media_get_time(source_);
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - speed_restart_tp_).count();

        if (restart_phase == 1) {
            if (state == OBS_MEDIA_STATE_PLAYING && (cur < 800 || cur < seek_target / 2)) {
                if (seek_target > 500) {
                    obs_source_media_set_time(source_, static_cast<uint64_t>(seek_target));
                    speed_restart_phase_.store(2, std::memory_order_relaxed);
                    speed_restart_tp_ = now;
                } else {
                    speed_restart_phase_.store(0, std::memory_order_relaxed);
                }
            } else if (elapsed_ms > 1200) {
                if (seek_target > 500) {
                    obs_source_media_set_time(source_, static_cast<uint64_t>(seek_target));
                }
                speed_restart_phase_.store(0, std::memory_order_relaxed);
                pending_speed_seek_ms_.store(0, std::memory_order_relaxed);
            }
        } else if (restart_phase == 2) {
            if (cur >= seek_target - 1500 || elapsed_ms > 350) {
                speed_restart_phase_.store(0, std::memory_order_relaxed);
                pending_speed_seek_ms_.store(0, std::memory_order_relaxed);
            } else if (elapsed_ms > 120 && cur < 800) {
                obs_source_media_set_time(source_, static_cast<uint64_t>(seek_target));
            }
        }
    }

    // 2. Seamless Auto-Loop execution with seek debounce
    if (!isLooping()) {
        return;
    }

    const int64_t current = obs_source_media_get_time(source_);
    const int64_t loop_end = loop_end_ms_.load(std::memory_order_relaxed);
    const int64_t loop_start = loop_start_ms_.load(std::memory_order_relaxed);

    if (loop_end <= loop_start) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_loop_seek_tp_).count();

    if (current >= loop_end && elapsed_ms > 120) {
        obs_source_media_set_time(source_, static_cast<uint64_t>(loop_start));
        last_loop_seek_tp_ = now;
    }
}

void DeckPlayer::syncToBpm(double target_bpm)
{
    const double base = bpm();
    if (base < 10.0 || target_bpm < 10.0)
        return;

    target_sync_bpm_.store(target_bpm, std::memory_order_relaxed);
    is_sync_.store(true, std::memory_order_relaxed);

    const int target_speed = std::clamp(
        static_cast<int>(std::round((target_bpm / base) * 100.0)),
        50, 150);
    applySpeedSettings(target_speed);
    // Maintain sync flag even after applySpeedSettings
    is_sync_.store(true, std::memory_order_relaxed);
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
        obs_source_media_set_time(source_, static_cast<uint64_t>(cur + (static_cast<int64_t>(beat_ms) - phase)));
    } else {
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

// -----------------------------------------------------------------------------
// DeckPair implementation
// -----------------------------------------------------------------------------

DeckPair::DeckPair()
{
    updateAudioMix();

    cue_thread_running_.store(true, std::memory_order_relaxed);
    cue_thread_ = std::thread(&DeckPair::cueWorkerLoop, this);
}

DeckPair::~DeckPair()
{
    cue_thread_running_.store(false, std::memory_order_relaxed);
    cue_cv_.notify_all();
    if (cue_thread_.joinable())
        cue_thread_.join();
}

DeckPlayer &DeckPair::deckA() noexcept
{
    return *vj_deck_a();
}

DeckPlayer &DeckPair::deckB() noexcept
{
    return *vj_deck_b();
}

void DeckPair::setCrossfader(double value) noexcept
{
    const double clamped = std::clamp(value, 0.0, 1.0);
    crossfader_.store(clamped, std::memory_order_relaxed);
    updateAudioMix();

    // Drive OBS Studio Mode scene transition T-Bar in real time
    if (obs_frontend_preview_program_mode_active()) {
        const int tbar_pos = std::clamp(static_cast<int>(std::round(clamped * 1023.0)), 0, 1023);
        obs_frontend_set_tbar_position(tbar_pos);
        if (tbar_pos <= 0 || tbar_pos >= 1023) {
            obs_frontend_release_tbar();
        }
    }
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

void DeckPair::setObsOutputMode(int mode) noexcept
{
    obs_output_mode_.store(std::clamp(mode, 0, 2), std::memory_order_relaxed);
    updateAudioMix();
}

int DeckPair::obsOutputMode() const noexcept
{
    return obs_output_mode_.load(std::memory_order_relaxed);
}

void DeckPair::setHeadphoneVolume(float v) noexcept
{
    headphone_volume_.store(std::clamp(v, 0.0f, 1.0f), std::memory_order_relaxed);
}

float DeckPair::headphoneVolume() const noexcept
{
    return headphone_volume_.load(std::memory_order_relaxed);
}

void DeckPair::setHeadphoneMix(float m) noexcept
{
    headphone_mix_.store(std::clamp(m, 0.0f, 1.0f), std::memory_order_relaxed);
}

float DeckPair::headphoneMix() const noexcept
{
    return headphone_mix_.load(std::memory_order_relaxed);
}

void DeckPair::setMasterMode(int mode) noexcept
{
    master_mode_.store(mode, std::memory_order_relaxed);
    if (mode == MASTER_DECK_A) {
        if (vj_deck_a()) vj_deck_a()->setMaster(true);
        if (vj_deck_b()) vj_deck_b()->setMaster(false);
    } else if (mode == MASTER_DECK_B) {
        if (vj_deck_a()) vj_deck_a()->setMaster(false);
        if (vj_deck_b()) vj_deck_b()->setMaster(true);
    } else {
        if (vj_deck_a()) vj_deck_a()->setMaster(false);
        if (vj_deck_b()) vj_deck_b()->setMaster(false);
    }
}

int DeckPair::masterMode() const noexcept
{
    return master_mode_.load(std::memory_order_relaxed);
}

void DeckPair::setMasterClockBpm(double bpm) noexcept
{
    master_clock_bpm_.store(std::clamp(bpm, 40.0, 300.0), std::memory_order_relaxed);
}

double DeckPair::masterClockBpm() const noexcept
{
    return master_clock_bpm_.load(std::memory_order_relaxed);
}

double DeckPair::effectiveMasterBpm() const noexcept
{
    const int mode = master_mode_.load(std::memory_order_relaxed);
    if (mode == MASTER_DECK_A && vj_deck_a())
        return vj_deck_a()->effectiveBpm();
    if (mode == MASTER_DECK_B && vj_deck_b())
        return vj_deck_b()->effectiveBpm();
    return master_clock_bpm_.load(std::memory_order_relaxed);
}

void DeckPair::setSync(int deck_idx, bool sync_enabled)
{
    DeckPlayer *target = (deck_idx == 0) ? vj_deck_a() : vj_deck_b();
    if (!target) return;
    target->setSync(sync_enabled);
    if (sync_enabled) {
        target->syncToBpm(effectiveMasterBpm());
    }
}

bool DeckPair::isSync(int deck_idx) const noexcept
{
    DeckPlayer *target = (deck_idx == 0) ? vj_deck_a() : vj_deck_b();
    return target ? target->isSync() : false;
}

void DeckPair::setDeckMaster(int deck_idx)
{
    if (deck_idx == 0) {
        setMasterMode((masterMode() == MASTER_DECK_A) ? MASTER_CLOCK : MASTER_DECK_A);
    } else {
        setMasterMode((masterMode() == MASTER_DECK_B) ? MASTER_CLOCK : MASTER_DECK_B);
    }
}

void DeckPair::updateSync()
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (!da || !db) return;

    const double master_bpm = effectiveMasterBpm();
    if (da->isSync() && !da->isMaster()) {
        const double diff = std::abs(da->effectiveBpm() - master_bpm);
        if (diff > 0.05) da->syncToBpm(master_bpm);
    }
    if (db->isSync() && !db->isMaster()) {
        const double diff = std::abs(db->effectiveBpm() - master_bpm);
        if (diff > 0.05) db->syncToBpm(master_bpm);
    }
}

void DeckPair::syncTempoAtoB()
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (da && db) da->syncToBpm(db->effectiveBpm());
}

void DeckPair::syncTempoBtoA()
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (da && db) db->syncToBpm(da->effectiveBpm());
}

void DeckPair::setLoopBeatIndex(int deck_index, int beat_index)
{
    const int clamped = std::clamp(beat_index, 0, 6);
    if (deck_index == 0) {
        loop_beat_idx_a_.store(clamped, std::memory_order_relaxed);
        if (vj_deck_a()) vj_deck_a()->setLoopBeatIndex(clamped);
    } else {
        loop_beat_idx_b_.store(clamped, std::memory_order_relaxed);
        if (vj_deck_b()) vj_deck_b()->setLoopBeatIndex(clamped);
    }
}

int DeckPair::getLoopBeatIndex(int deck_index) const noexcept
{
    return deck_index == 0
        ? loop_beat_idx_a_.load(std::memory_order_relaxed)
        : loop_beat_idx_b_.load(std::memory_order_relaxed);
}

void DeckPair::updateAudioMix() noexcept
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (!da || !db) return;

    const int out_mode = obs_output_mode_.load(std::memory_order_relaxed);
    if (out_mode == 1) {
        da->setMixGain(1.0f);
        db->setMixGain(0.0f);
    } else if (out_mode == 2) {
        da->setMixGain(0.0f);
        db->setMixGain(1.0f);
    } else {
        const double x = crossfader_.load(std::memory_order_relaxed);
        float gain_a = 1.0f;
        float gain_b = 1.0f;
        if (x > 0.5) {
            // As x goes from 0.5 to 1.0: Deck A fades smoothly from 1.0 to 0.0 using cosine
            const double t = (x - 0.5) * 2.0;
            gain_a = static_cast<float>(std::cos(t * 1.5707963267948966));
            gain_b = 1.0f;
        } else if (x < 0.5) {
            // As x goes from 0.0 to 0.5: Deck B rises smoothly from 0.0 to 1.0 using sine
            const double t = x * 2.0;
            gain_a = 1.0f;
            gain_b = static_cast<float>(std::sin(t * 1.5707963267948966));
        } else {
            // Exactly at center 50/50: BOTH decks are at 100% full line level (1.0f)!
            gain_a = 1.0f;
            gain_b = 1.0f;
        }
        da->setMixGain(gain_a);
        db->setMixGain(gain_b);
    }
}

void DeckPair::enableAudioMonitoring() noexcept
{
    if (vj_deck_a()) vj_deck_a()->enableAudioMonitoring();
    if (vj_deck_b()) vj_deck_b()->enableAudioMonitoring();
}

void DeckPair::queueCueAudio(int deck_idx, const float *const *channels, size_t frames)
{
    if (!cue_source_ || !channels || !channels[0] || frames == 0)
        return;

    const float hp_vol = headphone_volume_.load(std::memory_order_relaxed);
    const float hp_mix = headphone_mix_.load(std::memory_order_relaxed);

    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (!da || !db) return;

    const bool is_cue = (deck_idx == 0) ? da->isHeadphoneCue() : db->isHeadphoneCue();
    if (!is_cue || hp_vol <= 0.001f)
        return;

    const float ch_gain = (deck_idx == 0) ? da->gain() : db->gain();
    const float ch_vol = (deck_idx == 0) ? da->volume() : db->volume();
    const float xfade = static_cast<float>(crossfader_.load(std::memory_order_relaxed));
    const float xfade_gain = (deck_idx == 0) ? (1.0f - xfade) : xfade;

    const float cue_factor = (1.0f - hp_mix) * ch_gain;
    const float master_factor = hp_mix * ch_gain * ch_vol * xfade_gain;
    const float total_factor = (cue_factor + master_factor) * hp_vol;

    if (total_factor <= 0.0001f)
        return;

    CuePacket pkt;
    pkt.l.resize(frames);
    pkt.r.resize(frames);
    const float *in_l = channels[0];
    const float *in_r = channels[1] ? channels[1] : channels[0];

    for (size_t i = 0; i < frames; ++i) {
        pkt.l[i] = in_l[i] * total_factor;
        pkt.r[i] = in_r[i] * total_factor;
    }
    pkt.frames = frames;

    std::lock_guard<std::mutex> lock(cue_mutex_);
    if (cue_queue_.size() > 15)
        cue_queue_.pop();
    cue_queue_.push(std::move(pkt));
    cue_cv_.notify_one();
}

void DeckPair::cueWorkerLoop()
{
    uint64_t cue_next_timestamp = 0;
    while (cue_thread_running_.load(std::memory_order_relaxed)) {
        CuePacket pkt;
        {
            std::unique_lock<std::mutex> lock(cue_mutex_);
            cue_cv_.wait_for(lock, std::chrono::milliseconds(20), [this]() {
                return !cue_queue_.empty() || !cue_thread_running_.load(std::memory_order_relaxed);
            });
            if (!cue_thread_running_.load(std::memory_order_relaxed))
                break;
            if (cue_queue_.empty())
                continue;
            pkt = std::move(cue_queue_.front());
            cue_queue_.pop();
        }

        if (cue_source_ && pkt.frames > 0 && !pkt.l.empty() && !pkt.r.empty()) {
            struct obs_source_audio audio_out = {};
            audio_out.data[0] = reinterpret_cast<uint8_t *>(pkt.l.data());
            audio_out.data[1] = reinterpret_cast<uint8_t *>(pkt.r.data());
            audio_out.frames = static_cast<uint32_t>(pkt.frames);
            audio_out.speakers = SPEAKERS_STEREO;
            audio_out.format = AUDIO_FORMAT_FLOAT_PLANAR;
            audio_out.samples_per_sec = 48000;

            const uint64_t now_ns = os_gettime_ns();
            if (cue_next_timestamp == 0 ||
                (now_ns > cue_next_timestamp && (now_ns - cue_next_timestamp) > 100000000ULL) ||
                (cue_next_timestamp > now_ns && (cue_next_timestamp - now_ns) > 100000000ULL)) {
                cue_next_timestamp = now_ns;
            }
            audio_out.timestamp = cue_next_timestamp;
            cue_next_timestamp += static_cast<uint64_t>(pkt.frames) * 1000000000ULL / 48000ULL;

            obs_source_output_audio(cue_source_, &audio_out);
        }
    }
}
