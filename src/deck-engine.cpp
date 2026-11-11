#include "deck-engine.hpp"
#include "audio-analyzer.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

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
        blog(LOG_WARNING, "[obs-vj-mixer] Could not create ffmpeg_source '%s' (%s)",
             source_name, display_name);
        return;
    }

    is_deck_a_ = (display_name && std::strcmp(display_name, "Deck A") == 0);

    obs_source_inc_showing(source_);
    obs_source_inc_active(source_);
    obs_source_set_audio_active(source_, true);
    obs_source_set_audio_mixers(source_, 0x3F);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);

    obs_data_t *filter_settings = obs_data_create();
    if (filter_settings) {
        obs_data_set_int(filter_settings, "deck_idx", is_deck_a_ ? 0 : 1);
        eq_filter_ = obs_source_create_private("vdj_eq_filter", "Deck EQ Filter", filter_settings);
        obs_data_release(filter_settings);
        if (eq_filter_) {
            obs_source_filter_add(source_, eq_filter_);
        }
    }
    applyAudioVolume();
}

DeckPlayer::~DeckPlayer()
{
    if (eq_filter_ && source_) {
        obs_source_filter_remove(source_, eq_filter_);
        obs_source_release(eq_filter_);
        eq_filter_ = nullptr;
    }

    if (!source_)
        return;

    obs_source_dec_active(source_);
    obs_source_dec_showing(source_);
    obs_source_release(source_);
    source_ = nullptr;
}

void DeckPlayer::loadFile(const QString &path)
{
    if (!source_ || path.isEmpty())
        return;

    setFileName(QFileInfo(path).fileName());

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
    obs_data_set_int(settings, "speed_percent",
                     speed_percent_.load(std::memory_order_relaxed));

    obs_source_update(source_, settings);
    obs_data_release(settings);

    enableAudioMonitoring();

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

    // Launch real asynchronous audio decoding, BPM & downbeat analysis
    AudioAnalyzer::analyzeFileAsync(path, [this](TrackAnalysisResult res) {
        if (res.success) {
            setBpm(res.detected_bpm);
            setGridOffsetMs(res.grid_offset_ms);
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

void DeckPlayer::togglePlayback()
{
    if (!source_)
        return;

    enableAudioMonitoring();

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

    enableAudioMonitoring();

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
        const int64_t cue = std::max<int64_t>(
            0, cue_point_ms_.load(std::memory_order_relaxed));

        obs_source_media_play_pause(source_, true);
        obs_source_media_set_time(source_, static_cast<uint64_t>(cue));
    } else {
        const int64_t now = std::max<int64_t>(
            0, obs_source_media_get_time(source_));

        cue_point_ms_.store(now, std::memory_order_relaxed);
        obs_source_media_set_time(source_, static_cast<uint64_t>(now));
    }
}

void DeckPlayer::setSpeed(int percent)
{
    const int clamped = std::clamp(percent, 50, 150);
    const int old_speed = speed_percent_.exchange(clamped, std::memory_order_relaxed);
    if (old_speed == clamped)
        return;

    if (!source_)
        return;

    const auto now_tp = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_speed_update_tp_).count();
    if (elapsed < 80)
        return;

    last_speed_update_tp_ = now_tp;

    const int64_t cur_pos = obs_source_media_get_time(source_);
    if (cur_pos > 100) {
        pending_speed_seek_ms_.store(cur_pos, std::memory_order_relaxed);
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
    eq_hi_.store(std::clamp(val, 0.0f, 2.0f), std::memory_order_relaxed);
    setRed(val);
}

void DeckPlayer::setEqMid(float val) noexcept
{
    eq_mid_.store(std::clamp(val, 0.0f, 2.0f), std::memory_order_relaxed);
    setGreen(val);
}

void DeckPlayer::setEqLow(float val) noexcept
{
    eq_low_.store(std::clamp(val, 0.0f, 2.0f), std::memory_order_relaxed);
    setBlue(val);
}

void DeckPlayer::setFilterVal(float val) noexcept
{
    filter_val_.store(std::clamp(val, -1.0f, 1.0f), std::memory_order_relaxed);
    setSaturation(1.0f + val);
}

float DeckPlayer::eqHi() const noexcept { return eq_hi_.load(std::memory_order_relaxed); }
float DeckPlayer::eqMid() const noexcept { return eq_mid_.load(std::memory_order_relaxed); }
float DeckPlayer::eqLow() const noexcept { return eq_low_.load(std::memory_order_relaxed); }
float DeckPlayer::filterVal() const noexcept { return filter_val_.load(std::memory_order_relaxed); }

void DeckPlayer::processAudioDsp(float *channel_data, size_t num_frames, size_t channel_idx, size_t sample_rate)
{
    if (!channel_data || num_frames == 0)
        return;

    const float g_hi = eq_hi_.load(std::memory_order_relaxed);
    const float g_mid = eq_mid_.load(std::memory_order_relaxed);
    const float g_low = eq_low_.load(std::memory_order_relaxed);
    const float f_val = filter_val_.load(std::memory_order_relaxed);

    // Bit-perfect bypass when controls are centered (1.0 = 0 dB, filter = 0)
    if (std::abs(g_hi - 1.0f) < 0.005f &&
        std::abs(g_mid - 1.0f) < 0.005f &&
        std::abs(g_low - 1.0f) < 0.005f &&
        std::abs(f_val) < 0.005f) {
        return;
    }

    const size_t ch = channel_idx % 2;
    FilterState &st = dsp_states_[ch];

    const float sr = (sample_rate > 0) ? static_cast<float>(sample_rate) : 48000.0f;
    // Crossovers at 260 Hz and 3200 Hz
    const float w_low = 2.0f * 3.14159265f * 260.0f / sr;
    const float alpha_low = std::clamp(w_low / (1.0f + w_low), 0.001f, 0.95f);

    const float w_high = 2.0f * 3.14159265f * 3200.0f / sr;
    const float alpha_high = std::clamp(w_high / (1.0f + w_high), 0.001f, 0.95f);

    for (size_t i = 0; i < num_frames; ++i) {
        const float in = channel_data[i];

        // 3-band crossover decomposition
        st.lp += alpha_low * (in - st.lp);
        st.hp += alpha_high * (in - st.hp);

        const float low_band = st.lp;
        const float high_band = in - st.hp;
        const float mid_band = in - low_band - high_band;

        float out = low_band * g_low + mid_band * g_mid + high_band * g_hi;

        // DJ Filter: Low-Pass when f_val < 0, High-Pass when f_val > 0
        if (f_val < -0.01f) {
            const float cutoff = 300.0f + (1.0f + f_val) * 7500.0f;
            const float a_lp = std::clamp(2.0f * 3.14159265f * cutoff / sr, 0.01f, 0.95f);
            st.dj_lp += a_lp * (out - st.dj_lp);
            out = st.dj_lp;
        } else if (f_val > 0.01f) {
            const float cutoff = 100.0f + f_val * 3900.0f;
            const float a_hp = std::clamp(2.0f * 3.14159265f * cutoff / sr, 0.01f, 0.95f);
            st.dj_hp += a_hp * (out - st.dj_hp);
            out = out - st.dj_hp;
        }

        channel_data[i] = std::clamp(out, -1.5f, 1.5f);
    }
}

void DeckPlayer::setHeadphoneCue(bool cue) noexcept
{
    headphone_cue_.store(cue, std::memory_order_relaxed);
    applyAudioVolume();
}

bool DeckPlayer::isHeadphoneCue() const noexcept
{
    return headphone_cue_.load(std::memory_order_relaxed);
}

int64_t DeckPlayer::loopStartMs() const noexcept
{
    return loop_start_ms_.load(std::memory_order_relaxed);
}

int64_t DeckPlayer::loopEndMs() const noexcept
{
    return loop_end_ms_.load(std::memory_order_relaxed);
}

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
    volume_.store(std::clamp(volume, 0.0f, 1.0f),
                 std::memory_order_relaxed);
    applyAudioVolume();
}

float DeckPlayer::volume() const noexcept
{
    return volume_.load(std::memory_order_relaxed);
}

void DeckPlayer::setMixGain(float gain) noexcept
{
    mix_gain_.store(std::clamp(gain, 0.0f, 1.0f),
                   std::memory_order_relaxed);
    applyAudioVolume();
}

void DeckPlayer::applyAudioVolume() noexcept
{
    if (!source_)
        return;

    const float channel_volume = volume_.load(std::memory_order_relaxed);
    const float trim_gain = gain_.load(std::memory_order_relaxed);
    const float crossfade_gain = mix_gain_.load(std::memory_order_relaxed);

    // Master stream ONLY receives post-crossfader volume!
    // Headphone CUE is routed independently to the monitoring device and must NEVER bleed into master!
    const float final_vol = channel_volume * trim_gain * crossfade_gain;
    obs_source_set_volume(source_, std::clamp(final_vol, 0.0f, 2.0f));
}

void DeckPlayer::enableAudioMonitoring() noexcept
{
    if (!source_)
        return;

    obs_source_set_audio_active(source_, true);
    obs_source_set_audio_mixers(source_, 0x3F);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);

    applyAudioVolume();
}

void DeckPlayer::setLoopBeatIndex(int index) noexcept
{
    loop_beat_index_.store(std::clamp(index, 0, 7),
                          std::memory_order_relaxed);

    if (isLooping())
        setLooping(true);
}

void DeckPlayer::setLooping(bool enabled)
{
    if (!enabled) {
        looping_.store(false, std::memory_order_relaxed);
        loop_end_ms_.store(0, std::memory_order_relaxed);
        is_loop_seeking_.store(false, std::memory_order_relaxed);
        return;
    }

    if (!source_)
        return;

    const int index = loop_beat_index_.load(std::memory_order_relaxed);
    const double beats =
        index == 0 ? 0.5 : static_cast<double>(kLoopBeatCounts[index]);

    const double beat_ms = beatDurationMs();
    const int64_t current = std::max<int64_t>(0, currentTimeMs());

    // Quantize loop start to the nearest beat boundary respecting grid offset
    int64_t start = current;
    if (beat_ms > 20.0) {
        const int64_t offset = gridOffsetMs();
        const int64_t rel = current - offset;
        const int64_t quantized_rel = static_cast<int64_t>(std::round(static_cast<double>(rel) / beat_ms) * beat_ms);
        start = std::max<int64_t>(0, offset + quantized_rel);
    }

    const int64_t loop_length = std::max<int64_t>(
        1, static_cast<int64_t>(beats * beat_ms));
    const int64_t duration = durationMs();
    const int64_t end = (duration > 0)
        ? std::min<int64_t>(duration, start + loop_length)
        : (start + loop_length);

    loop_start_ms_.store(start, std::memory_order_relaxed);
    loop_end_ms_.store(end, std::memory_order_relaxed);
    is_loop_seeking_.store(false, std::memory_order_relaxed);
    looping_.store(true, std::memory_order_relaxed);
}

void DeckPlayer::updateLoop()
{
    if (!source_)
        return;

    const int64_t seek_target = pending_speed_seek_ms_.load(std::memory_order_relaxed);
    if (seek_target > 0) {
        const int64_t actual_pos = obs_source_media_get_time(source_);
        const auto state = obs_source_media_get_state(source_);
        if (state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_PAUSED) {
            if (actual_pos < seek_target - 500) {
                obs_source_media_set_time(source_, static_cast<uint64_t>(seek_target));
            } else if (actual_pos >= seek_target - 200) {
                pending_speed_seek_ms_.store(0, std::memory_order_relaxed);
            }
        }
    }

    if (!isLooping()) {
        is_loop_seeking_.store(false, std::memory_order_relaxed);
        return;
    }

    const int64_t start = loop_start_ms_.load(std::memory_order_relaxed);
    const int64_t end = loop_end_ms_.load(std::memory_order_relaxed);
    if (end <= start)
        return;

    const int64_t cur = currentTimeMs();
    const auto now_tp = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_loop_seek_tp_).count();

    if (is_loop_seeking_.load(std::memory_order_relaxed)) {
        // While seeking, wait until playhead returns into loop range, or 500ms safety timeout
        if (cur < end - 50 || elapsed_ms > 500) {
            is_loop_seeking_.store(false, std::memory_order_relaxed);
        }
    } else {
        if (cur >= end && elapsed_ms > 100) {
            is_loop_seeking_.store(true, std::memory_order_relaxed);
            last_loop_seek_tp_ = now_tp;
            obs_source_media_set_time(source_, start);
        }
    }
}

void DeckPlayer::seek(double normalized_position)
{
    if (!source_)
        return;

    const int64_t duration = durationMs();
    if (duration <= 0)
        return;

    const double clamped = std::clamp(normalized_position, 0.0, 1.0);
    const uint64_t target = static_cast<uint64_t>(
        clamped * static_cast<double>(duration));

    obs_source_media_set_time(source_, target);

    if (isLooping())
        setLooping(true);
}

double DeckPlayer::position() const
{
    const int64_t duration = durationMs();
    if (duration <= 0)
        return 0.0;

    return std::clamp(
        static_cast<double>(currentTimeMs()) / static_cast<double>(duration),
        0.0, 1.0);
}

int64_t DeckPlayer::currentTimeMs() const
{
    if (!source_)
        return 0;

    const int64_t pending = pending_speed_seek_ms_.load(std::memory_order_relaxed);
    if (pending > 0) {
        const int64_t actual = obs_source_media_get_time(source_);
        if (actual < pending - 500) {
            return pending;
        }
    }

    return std::max<int64_t>(0, obs_source_media_get_time(source_));
}

int64_t DeckPlayer::durationMs() const
{
    return source_ ? obs_source_media_get_duration(source_) : 0;
}

bool DeckPlayer::isPlaying() const
{
    return source_ &&
           obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING;
}

bool DeckPlayer::isLooping() const noexcept
{
    return looping_.load(std::memory_order_relaxed);
}

void DeckPlayer::setRed(float value) noexcept
{
    red_.store(std::clamp(value, 0.0f, 2.0f),
              std::memory_order_relaxed);
}

void DeckPlayer::setGreen(float value) noexcept
{
    green_.store(std::clamp(value, 0.0f, 2.0f),
                std::memory_order_relaxed);
}

void DeckPlayer::setBlue(float value) noexcept
{
    blue_.store(std::clamp(value, 0.0f, 2.0f),
               std::memory_order_relaxed);
}

void DeckPlayer::setSaturation(float value) noexcept
{
    saturation_.store(std::clamp(value, 0.0f, 2.0f),
                     std::memory_order_relaxed);
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
    obs_data_t *settings = obs_data_create();

    if (settings) {
        master_source_ =
            obs_source_create_private("vdj_master_mixer",
                                     "VJ Master Preview", settings);
        obs_data_release(settings);
    }

    if (master_source_) {
        obs_source_inc_showing(master_source_);
        obs_source_inc_active(master_source_);
        obs_source_set_audio_active(master_source_, true);
        obs_source_set_audio_mixers(master_source_, 0x3F);
        obs_source_set_monitoring_type(master_source_, OBS_MONITORING_TYPE_NONE);
    }

    cue_source_ = obs_source_create_private("vdj_headphone_cue", "VJ Headphone Cue", nullptr);
    if (cue_source_) {
        obs_source_inc_showing(cue_source_);
        obs_source_inc_active(cue_source_);
        obs_source_set_audio_active(cue_source_, true);
        obs_source_set_audio_mixers(cue_source_, 0); // Never bleed to master recording/stream
        obs_source_set_monitoring_type(cue_source_, OBS_MONITORING_TYPE_MONITOR_ONLY);
    }

    updateAudioMix();
    enableAudioMonitoring();

    cue_thread_running_.store(true, std::memory_order_relaxed);
    cue_thread_ = std::thread(&DeckPair::cueWorkerLoop, this);
}

DeckPair::~DeckPair()
{
    cue_thread_running_.store(false, std::memory_order_relaxed);
    cue_cv_.notify_all();
    if (cue_thread_.joinable())
        cue_thread_.join();

    if (cue_source_) {
        obs_source_dec_active(cue_source_);
        obs_source_dec_showing(cue_source_);
        obs_source_release(cue_source_);
        cue_source_ = nullptr;
    }

    if (!master_source_)
        return;

    obs_source_dec_active(master_source_);
    obs_source_dec_showing(master_source_);
    obs_source_release(master_source_);
    master_source_ = nullptr;
}

void DeckPair::setCrossfader(double value) noexcept
{
    crossfader_.store(std::clamp(value, 0.0, 1.0),
                     std::memory_order_relaxed);
    updateAudioMix();
}

double DeckPair::crossfader() const noexcept
{
    return crossfader_.load(std::memory_order_relaxed);
}

void DeckPair::setTransitionMode(int mode) noexcept
{
    transition_mode_.store(std::clamp(mode, 0, 3),
                          std::memory_order_relaxed);
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
    master_mode_.store(std::clamp(mode, 0, 2), std::memory_order_relaxed);
    if (mode == MASTER_DECK_A) {
        deck_a_.setMaster(true);
        deck_b_.setMaster(false);
    } else if (mode == MASTER_DECK_B) {
        deck_a_.setMaster(false);
        deck_b_.setMaster(true);
    } else {
        deck_a_.setMaster(false);
        deck_b_.setMaster(false);
    }
    updateSync();
}

int DeckPair::masterMode() const noexcept
{
    return master_mode_.load(std::memory_order_relaxed);
}

void DeckPair::setMasterClockBpm(double bpm) noexcept
{
    master_clock_bpm_.store(std::clamp(bpm, 40.0, 300.0), std::memory_order_relaxed);
    updateSync();
}

double DeckPair::masterClockBpm() const noexcept
{
    return master_clock_bpm_.load(std::memory_order_relaxed);
}

double DeckPair::effectiveMasterBpm() const noexcept
{
    const int mode = master_mode_.load(std::memory_order_relaxed);
    if (mode == MASTER_DECK_A)
        return deck_a_.effectiveBpm();
    if (mode == MASTER_DECK_B)
        return deck_b_.effectiveBpm();
    return master_clock_bpm_.load(std::memory_order_relaxed);
}

void DeckPair::setSync(int deck_idx, bool sync_enabled)
{
    if (deck_idx == 0) {
        deck_a_.setSync(sync_enabled);
    } else {
        deck_b_.setSync(sync_enabled);
    }
    updateSync();
}

bool DeckPair::isSync(int deck_idx) const noexcept
{
    return deck_idx == 0 ? deck_a_.isSync() : deck_b_.isSync();
}

void DeckPair::setDeckMaster(int deck_idx)
{
    if (deck_idx == 0) {
        setMasterMode(MASTER_DECK_A);
    } else {
        setMasterMode(MASTER_DECK_B);
    }
}

void DeckPair::updateSync()
{
    const int mode = master_mode_.load(std::memory_order_relaxed);
    const double master_bpm = effectiveMasterBpm();
    if (master_bpm < 20.0)
        return;

    // Deck A syncs to Master if Deck A is not Master
    if (deck_a_.isSync() && mode != MASTER_DECK_A) {
        const double base_a = deck_a_.bpm();
        if (base_a > 10.0) {
            const int target_speed = std::clamp(
                static_cast<int>(std::round((master_bpm / base_a) * 100.0)),
                50, 150);
            if (deck_a_.speed() != target_speed)
                deck_a_.setSpeed(target_speed);
        }
    }

    // Deck B syncs to Master if Deck B is not Master
    if (deck_b_.isSync() && mode != MASTER_DECK_B) {
        const double base_b = deck_b_.bpm();
        if (base_b > 10.0) {
            const int target_speed = std::clamp(
                static_cast<int>(std::round((master_bpm / base_b) * 100.0)),
                50, 150);
            if (deck_b_.speed() != target_speed)
                deck_b_.setSpeed(target_speed);
        }
    }
}

void DeckPair::syncTempoAtoB()
{
    // Deck B matches Deck A's effective BPM
    const double eff_bpm_a = deck_a_.effectiveBpm();
    const double base_bpm_b = deck_b_.bpm();
    if (base_bpm_b > 10.0 && eff_bpm_a > 10.0) {
        const int target_speed = std::clamp(
            static_cast<int>(std::round((eff_bpm_a / base_bpm_b) * 100.0)),
            50, 150);
        deck_b_.setSpeed(target_speed);
    } else {
        deck_b_.setSpeed(deck_a_.speed());
    }
}

void DeckPair::syncTempoBtoA()
{
    // Deck A matches Deck B's effective BPM
    const double eff_bpm_b = deck_b_.effectiveBpm();
    const double base_bpm_a = deck_a_.bpm();
    if (base_bpm_a > 10.0 && eff_bpm_b > 10.0) {
        const int target_speed = std::clamp(
            static_cast<int>(std::round((eff_bpm_b / base_bpm_a) * 100.0)),
            50, 150);
        deck_a_.setSpeed(target_speed);
    } else {
        deck_a_.setSpeed(deck_b_.speed());
    }
}

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
    return deck_index == 0
        ? loop_beat_idx_a_.load(std::memory_order_relaxed)
        : loop_beat_idx_b_.load(std::memory_order_relaxed);
}

void DeckPair::updateAudioMix() noexcept
{
    const int out_mode = obs_output_mode_.load(std::memory_order_relaxed);
    if (out_mode == 1) {
        deck_a_.setMixGain(1.0f);
        deck_b_.setMixGain(0.0f);
    } else if (out_mode == 2) {
        deck_a_.setMixGain(0.0f);
        deck_b_.setMixGain(1.0f);
    } else {
        const double x = crossfader();
        deck_a_.setMixGain(static_cast<float>(1.0 - x));
        deck_b_.setMixGain(static_cast<float>(x));
    }
}

void DeckPair::enableAudioMonitoring() noexcept
{
    deck_a_.enableAudioMonitoring();
    deck_b_.enableAudioMonitoring();
}

void DeckPair::queueCueAudio(int deck_idx, const float *const *channels, size_t frames)
{
    if (!cue_source_ || !channels || !channels[0] || frames == 0)
        return;

    const float hp_vol = headphone_volume_.load(std::memory_order_relaxed);
    const float hp_mix = headphone_mix_.load(std::memory_order_relaxed);

    const bool is_cue = (deck_idx == 0) ? deck_a_.isHeadphoneCue() : deck_b_.isHeadphoneCue();
    const float ch_gain = (deck_idx == 0) ? deck_a_.gain() : deck_b_.gain();
    const float ch_vol = (deck_idx == 0) ? deck_a_.volume() : deck_b_.volume();
    const float xfade = static_cast<float>(crossfader_.load(std::memory_order_relaxed));
    const float xfade_gain = (deck_idx == 0) ? (1.0f - xfade) : xfade;

    // CUE factor (pre-fader) + Master factor (post-crossfader)
    const float cue_factor = is_cue ? (1.0f - hp_mix) * ch_gain : 0.0f;
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

DeckPair *vdj_decks() noexcept
{
    return g_decks;
}

void vdj_set_decks(DeckPair *decks) noexcept
{
    g_decks = decks;
}
