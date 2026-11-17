#include "engine/deck_engine.hpp"
#include "engine/engine_dsp.hpp"
#include "engine/engine_loop.hpp"
#include "engine/engine_sync.hpp"
#include "engine/engine_buffer.hpp"
#include "audio/audio_analyzer.hpp"
#include "audio/wasapi_output.hpp"

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
    side_.store(is_deck_a_ ? DeckSide::A : DeckSide::B, std::memory_order_relaxed);

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
    // A belső privát FFmpeg videodekóder forrást NEM regisztráljuk aktív OBS audióként,
    // mert az OBS globális keverője hiányzó hangcsomagok miatt 960ms puffer-újraindítási ciklusba kerülne.
    // (Never register private ffmpeg decoder as active OBS audio source to prevent audio lag restart spiral)
    obs_source_set_audio_active(source_, false);
    obs_source_set_muted(source_, true);
    obs_source_set_volume(source_, 0.0f);
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

    // Ha a deck éppen lejátszás alatt áll, szigorúan tilos új számot rátölteni (Stop/Pause szükséges)
    if (isPlaying()) {
        blog(LOG_WARNING, "[obs-vj-deck] Deck '%s' is currently playing! Playback must be stopped before loading a new track.",
             deckId());
        return;
    }

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

    // A belső privát FFmpeg videodekóder hangját teljesen némítjuk és kikapcsoljuk a frissítés előtt,
    // így az FFmpeg konténer megnyitásakor keletkező első dekódolt audiócsomag soha nem pattanhat be az OBS kimenetre.
    obs_source_set_audio_active(source_, false);
    obs_source_set_muted(source_, true);
    obs_source_set_volume(source_, 0.0f);
    obs_source_set_audio_mixers(source_, 0);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);

    obs_source_update(source_, settings);
    obs_data_release(settings);

    // Megerősítjük a teljes csendet a belső forráson a beállítások frissítése után is
    obs_source_set_audio_active(source_, false);
    obs_source_set_muted(source_, true);
    obs_source_set_volume(source_, 0.0f);
    obs_source_set_audio_mixers(source_, 0);
    obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);
    applyAudioVolume();

    looping_.store(false, std::memory_order_relaxed);
    loop_start_ms_.store(0, std::memory_order_relaxed);
    loop_end_ms_.store(0, std::memory_order_relaxed);
    cue_point_ms_.store(0, std::memory_order_relaxed);
    for (int i = 0; i < 4; ++i) hot_cues_[i].store(-1, std::memory_order_relaxed);
    grid_offset_ms_.store(0, std::memory_order_relaxed);
    file_changed_.store(true, std::memory_order_relaxed);

    // Ha ez a deck volt a Master, de a másik deck éppen játszik a közönségnek,
    // átadjuk a Master jogot a játszó decknek, hogy az új szám betöltése NE rántsa el a zenét!
    DeckPair *dp = vdj_decks();
    if (dp) {
        DeckPlayer *other = (side() == DeckSide::A) ? vj_deck_b() : vj_deck_a();
        if (other && other->isPlaying()) {
            const int other_master = (side() == DeckSide::A) ? DeckPair::MASTER_DECK_B : DeckPair::MASTER_DECK_A;
            dp->setMasterMode(other_master);
        }
    }

    clearAudioBuffer();

    {
        std::lock_guard<std::mutex> lock(waveform_mutex_);
        overview_samples_.clear();
        detailed_samples_.clear();
    }

    loadVjConfigFromCache();

    // Aszinkron audio analízis és PCM betöltés a memóriába (Async Audio Analysis & RAM PCM Buffer)
    AudioAnalyzer::analyzeFileAsync(path, [this](TrackAnalysisResult res) {
        if (res.success) {
            base_bpm_.store(std::clamp(res.detected_bpm, 40.0, 300.0), std::memory_order_relaxed);
            playback_bpm_.store(0.0, std::memory_order_relaxed);
            if (isMaster()) {
                DeckPair *dp = vdj_decks();
                if (dp) dp->setMasterClockBpm(res.detected_bpm);
            }
            grid_offset_ms_.store(res.grid_offset_ms, std::memory_order_relaxed);
            if (res.cue_point_ms > 0) {
                cue_point_ms_.store(res.cue_point_ms, std::memory_order_relaxed);
            }
            for (int i = 0; i < 4; ++i) {
                hot_cues_[i].store(res.hot_cues[i], std::memory_order_relaxed);
            }
            setKeys(res.camelot_key, res.detected_key);
            setWaveformSamples(res.overview_peaks, res.detailed_peaks);
            setSpectralSamples(res.overview_low, res.overview_mid, res.overview_high,
                               res.detailed_low, res.detailed_mid, res.detailed_high);
            setTonusSamples(res.overview_tonus);
            if (!res.audio_pcm_l.empty() && !res.audio_pcm_r.empty()) {
                setAudioBuffer(std::move(res.audio_pcm_l), std::move(res.audio_pcm_r), res.audio_sample_rate);
            }
            if (res.has_vj_config) {
                std::lock_guard<std::mutex> lock(vj_config_mutex_);
                vj_config_ = res.vj_config;
                const int s = std::clamp(vj_config_.shader_idx, 0, 11);
                vj_config_.all_shader_params[s] = vj_config_.params;
            }
        }
    });

    // Betöltéskor NEM indul el automatikusan: 00:00 CUE pozícióban szünetel (Never Autoplay on Load)
    user_playing_.store(false, std::memory_order_relaxed);
    playback_bpm_.store(0.0, std::memory_order_relaxed);
    pause_on_load_pending_.store(true, std::memory_order_relaxed);
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

QString DeckPlayer::camelotKey() const
{
    std::lock_guard<std::mutex> lock(file_mutex_);
    return camelot_key_;
}

QString DeckPlayer::detectedKey() const
{
    std::lock_guard<std::mutex> lock(file_mutex_);
    return detected_key_;
}

void DeckPlayer::setKeys(const QString &camelot, const QString &key)
{
    std::lock_guard<std::mutex> lock(file_mutex_);
    camelot_key_ = camelot.isEmpty() ? QStringLiteral("--") : camelot;
    detected_key_ = key.isEmpty() ? QStringLiteral("--") : key;
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

// =============================================================================
// Lejátszásvezérlés (Transport Controls: Play, Pause, Cue)
// =============================================================================
void DeckPlayer::play()
{
    user_playing_.store(true, std::memory_order_relaxed);
    pause_on_load_pending_.store(false, std::memory_order_relaxed);
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
    user_playing_.store(false, std::memory_order_relaxed);
    if (!source_)
        return;
    if (obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING)
        obs_source_media_play_pause(source_, true);
}

void DeckPlayer::togglePlayback()
{
    if (user_playing_.load(std::memory_order_relaxed)) {
        pause();
    } else {
        play();
    }
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
    return user_playing_.load(std::memory_order_relaxed);
}

void DeckPlayer::cue()
{
    const int64_t cue = std::max<int64_t>(0, cue_point_ms_.load(std::memory_order_relaxed));
    if (user_playing_.load(std::memory_order_relaxed)) {
        // Lejátszás közben: CUE-ra ugrás és szüneteltetés (Jump to cue and pause)
        pause();
        seekMs(cue);
    } else {
        // Álló helyzetben: új CUE pont rögzítése az aktuális pozíciónál (Set CUE at current)
        const int64_t now = currentTimeMs();
        cue_point_ms_.store(now, std::memory_order_relaxed);
        seekMs(now);
        saveUserEdits();
    }
}

void DeckPlayer::setCuePointMs(int64_t ms) noexcept
{
    cue_point_ms_.store(std::max<int64_t>(0, ms), std::memory_order_relaxed);
    saveUserEdits();
}

int64_t DeckPlayer::hotCueMs(int index) const noexcept
{
    if (index < 0 || index >= 4) return -1;
    return hot_cues_[index].load(std::memory_order_relaxed);
}

void DeckPlayer::setHotCueMs(int index, int64_t ms) noexcept
{
    if (index < 0 || index >= 4) return;
    hot_cues_[index].store(ms, std::memory_order_relaxed);
    QString path;
    {
        std::lock_guard<std::mutex> lock(file_mutex_);
        path = file_path_;
    }
    if (!path.isEmpty()) {
        int64_t cues[4];
        for (int i = 0; i < 4; ++i) cues[i] = hot_cues_[i].load(std::memory_order_relaxed);
        AudioAnalyzer::updateCacheHotCues(path, cues);
    }
}

void DeckPlayer::clearHotCue(int index) noexcept
{
    if (index < 0 || index >= 4) return;
    hot_cues_[index].store(-1, std::memory_order_relaxed);
    QString path;
    {
        std::lock_guard<std::mutex> lock(file_mutex_);
        path = file_path_;
    }
    if (!path.isEmpty()) {
        int64_t cues[4];
        for (int i = 0; i < 4; ++i) cues[i] = hot_cues_[i].load(std::memory_order_relaxed);
        AudioAnalyzer::updateCacheHotCues(path, cues);
    }
}

void DeckPlayer::triggerHotCue(int index) noexcept
{
    if (index < 0 || index >= 4) return;
    const int64_t ms = hot_cues_[index].load(std::memory_order_relaxed);
    if (ms < 0) {
        // Record current position snapped to the nearest beat
        const int64_t cur = currentTimeMs();
        const double b = (bpm() > 10.0) ? bpm() : 128.0;
        const double beat_ms = 60000.0 / b;
        const int64_t grid = gridOffsetMs();
        const int64_t rel = cur - grid;
        const int64_t nearest_beat_idx = static_cast<int64_t>(std::round(static_cast<double>(rel) / beat_ms));
        const int64_t snapped_ms = std::max<int64_t>(0, grid + static_cast<int64_t>(nearest_beat_idx * beat_ms));
        setHotCueMs(index, snapped_ms);
    } else {
        // Quantized Hot Cue Jump:
        // If playing, maintain phase alignment with the beat grid so no rhythmic stumble occurs!
        if (isPlaying()) {
            const double b = (bpm() > 10.0) ? bpm() : 128.0;
            const double beat_ms = 60000.0 / b;
            const int64_t grid = gridOffsetMs();
            const int64_t cur = currentTimeMs();
            const double cur_phase = std::fmod(std::fmod(static_cast<double>(cur - grid), beat_ms) + beat_ms, beat_ms);
            const int64_t target = ms + static_cast<int64_t>(std::round(cur_phase));
            seekMs(target);
        } else {
            seekMs(ms);
        }
    }
}

std::array<int64_t, 4> DeckPlayer::allHotCues() const noexcept
{
    std::array<int64_t, 4> res;
    for (int i = 0; i < 4; ++i) {
        res[i] = hot_cues_[i].load(std::memory_order_relaxed);
    }
    return res;
}

bool DeckPlayer::isAtEnd() const noexcept
{
    const int64_t dur = durationMs();
    if (dur <= 1000) return false;
    const int64_t cur = currentTimeMs();
    return (cur >= dur - 300);
}

void DeckPlayer::seek(double normalized_pos)
{
    const int64_t dur = durationMs();
    if (dur <= 0)
        return;
    const int64_t target_ms = static_cast<int64_t>(std::clamp(normalized_pos, 0.0, 1.0) * static_cast<double>(dur));
    seekMs(target_ms);
}

void DeckPlayer::seekMs(int64_t ms)
{
    const int64_t t = std::max<int64_t>(0, ms);
    {
        std::lock_guard<std::mutex> lock(audio_buf_mutex_);
        if (audio_sample_rate_ > 0) {
            playback_sample_pos_ = (static_cast<double>(t) * audio_sample_rate_) / 1000.0;
            if (!audio_pcm_l_.empty() && playback_sample_pos_ >= audio_pcm_l_.size()) {
                playback_sample_pos_ = static_cast<double>(audio_pcm_l_.size() - 1);
            }
            sola_initialized_ = false;
            sola_out_l_.clear();
            sola_out_r_.clear();
            sola_last_grain_l_.clear();
            sola_last_grain_r_.clear();
            sola_out_read_pos_ = 0.0;
        }
    }
    if (source_) {
        obs_source_media_set_time(source_, static_cast<uint64_t>(t));
    }
}

void DeckPlayer::setSpeed(int percent)
{
    const int clamped = std::clamp(percent, 50, 150);
    speed_percent_.store(clamped, std::memory_order_relaxed);
    playback_bpm_.store(0.0, std::memory_order_relaxed); // Csúszkamozgatáskor feloldjuk a rögzített BPM-et (Unlock fixed BPM on slider move)
    is_sync_.store(false, std::memory_order_relaxed);
}

void DeckPlayer::applySpeedSettings(int speed_percent)
{
    if (!source_)
        return;

    const int clamped = std::clamp(speed_percent, 50, 150);
    speed_percent_.store(clamped, std::memory_order_relaxed);
    playback_bpm_.store(0.0, std::memory_order_relaxed);
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
    playback_bpm_.store(0.0, std::memory_order_relaxed);
    saveUserEdits();
}

double DeckPlayer::bpm() const noexcept
{
    return base_bpm_.load(std::memory_order_relaxed);
}

void DeckPlayer::setPlaybackBpm(double target_bpm) noexcept
{
    const double clamped = std::clamp(target_bpm, 20.0, 300.0);
    is_sync_.store(false, std::memory_order_relaxed);
    const double base = bpm();
    if (base > 1.0) {
        const int spd = std::clamp(static_cast<int>(std::round((clamped / base) * 100.0)), 50, 150);
        applySpeedSettings(spd);
    }
    // CRITICAL: Must be stored AFTER applySpeedSettings, because applySpeedSettings resets playback_bpm_ to 0.0!
    playback_bpm_.store(clamped, std::memory_order_relaxed);
}

double DeckPlayer::effectiveBpm() const noexcept
{
    const double pb = playback_bpm_.load(std::memory_order_relaxed);
    if (pb > 1.0) {
        return pb;
    }
    if (is_sync_.load(std::memory_order_relaxed) && !is_master_.load(std::memory_order_relaxed)) {
        return target_sync_bpm_.load(std::memory_order_relaxed);
    }
    const double b = bpm();
    const double spd = static_cast<double>(speed()) / 100.0;
    return b * spd;
}

// =============================================================================
// Egy zenei ütem hossza a hangfájl eredeti idejében (Track Base Beat Duration)
// =============================================================================
double DeckPlayer::beatDurationMs() const noexcept
{
    const double b = bpm();
    return (b > 1.0) ? (60000.0 / b) : 500.0;
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
    extern void vdj_notify_track_bpm_changed(const QString &file_path, double new_bpm);
    vdj_notify_track_bpm_changed(path, bpm());
}

void DeckPlayer::setWaveformSamples(const std::vector<float> &overview, const std::vector<float> &detailed)
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    overview_samples_ = overview;
    detailed_samples_ = detailed;
}

void DeckPlayer::setSpectralSamples(const std::vector<float> &ov_low, const std::vector<float> &ov_mid, const std::vector<float> &ov_high,
                                    const std::vector<float> &det_low, const std::vector<float> &det_mid, const std::vector<float> &det_high)
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    overview_low_ = ov_low;
    overview_mid_ = ov_mid;
    overview_high_ = ov_high;
    detailed_low_ = det_low;
    detailed_mid_ = det_mid;
    detailed_high_ = det_high;
    has_spectral_ = (!overview_low_.empty() && !overview_mid_.empty() && !overview_high_.empty());
}

void DeckPlayer::setTonusSamples(const std::vector<float> &tonus)
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    overview_tonus_ = tonus;
}

std::vector<float> DeckPlayer::overviewTonus() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return overview_tonus_;
}

float DeckPlayer::currentMacroTonus() const noexcept
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    if (overview_tonus_.empty()) {
        return 0.5f;
    }
    const int64_t dur = durationMs();
    if (dur <= 0) {
        return 0.5f;
    }
    const int64_t cur = currentTimeMs();
    const double ratio = std::clamp(static_cast<double>(cur) / static_cast<double>(dur), 0.0, 1.0);
    const double pos = ratio * static_cast<double>(overview_tonus_.size() - 1);
    const size_t idx0 = static_cast<size_t>(pos);
    const size_t idx1 = std::min(idx0 + 1, overview_tonus_.size() - 1);
    const float frac = static_cast<float>(pos - static_cast<double>(idx0));
    return (1.0f - frac) * overview_tonus_[idx0] + frac * overview_tonus_[idx1];
}

std::vector<float> DeckPlayer::overviewSamples() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return overview_samples_;
}

std::vector<float> DeckPlayer::overviewLow() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return overview_low_;
}

std::vector<float> DeckPlayer::overviewMid() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return overview_mid_;
}

std::vector<float> DeckPlayer::overviewHigh() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return overview_high_;
}

std::vector<float> DeckPlayer::detailedSamples() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return detailed_samples_;
}

std::vector<float> DeckPlayer::detailedLow() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return detailed_low_;
}

std::vector<float> DeckPlayer::detailedMid() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return detailed_mid_;
}

std::vector<float> DeckPlayer::detailedHigh() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return detailed_high_;
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

bool DeckPlayer::hasSpectralData() const
{
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    return has_spectral_;
}

float DeckPlayer::currentBassEnergy() const noexcept
{
    if (!isPlaying()) return 0.0f;
    const float live = live_bass_.load(std::memory_order_relaxed);
    if (live > 0.005f) return live;
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    if (detailed_low_.empty()) return 0.0f;
    const int64_t cur = currentTimeMs();
    const size_t idx = static_cast<size_t>(std::clamp<int64_t>((cur * 100) / 1000, 0, static_cast<int64_t>(detailed_low_.size() - 1)));
    return detailed_low_[idx];
}

float DeckPlayer::currentMidEnergy() const noexcept
{
    if (!isPlaying()) return 0.0f;
    const float live = live_mid_.load(std::memory_order_relaxed);
    if (live > 0.005f) return live;
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    if (detailed_mid_.empty()) return 0.0f;
    const int64_t cur = currentTimeMs();
    const size_t idx = static_cast<size_t>(std::clamp<int64_t>((cur * 100) / 1000, 0, static_cast<int64_t>(detailed_mid_.size() - 1)));
    return detailed_mid_[idx];
}

float DeckPlayer::currentHighEnergy() const noexcept
{
    if (!isPlaying()) return 0.0f;
    const float live = live_high_.load(std::memory_order_relaxed);
    if (live > 0.005f) return live;
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    if (detailed_high_.empty()) return 0.0f;
    const int64_t cur = currentTimeMs();
    const size_t idx = static_cast<size_t>(std::clamp<int64_t>((cur * 100) / 1000, 0, static_cast<int64_t>(detailed_high_.size() - 1)));
    return detailed_high_[idx];
}

float DeckPlayer::currentAmplitude() const noexcept
{
    if (!isPlaying()) return 0.0f;
    const float live = live_amp_.load(std::memory_order_relaxed);
    if (live > 0.005f) return live;
    std::lock_guard<std::mutex> lock(waveform_mutex_);
    if (detailed_samples_.empty()) return 0.0f;
    const int64_t cur = currentTimeMs();
    const size_t idx = static_cast<size_t>(std::clamp<int64_t>((cur * 100) / 1000, 0, static_cast<int64_t>(detailed_samples_.size() - 1)));
    return detailed_samples_[idx];
}

VjDeckConfig DeckPlayer::vjConfig() const noexcept
{
    std::lock_guard<std::mutex> lock(vj_config_mutex_);
    return vj_config_;
}

void DeckPlayer::setVjConfig(const VjDeckConfig &cfg) noexcept
{
    {
        std::lock_guard<std::mutex> lock(vj_config_mutex_);
        vj_config_ = cfg;
    }
    saveVjConfigToCache();
}

void DeckPlayer::setVjParamValue(int param_idx, float val) noexcept
{
    if (param_idx >= 0 && param_idx < 8) {
        {
            std::lock_guard<std::mutex> lock(vj_config_mutex_);
            vj_config_.params[param_idx].value = val;
            const int s = std::clamp(vj_config_.shader_idx, 0, 11);
            vj_config_.all_shader_params[s][param_idx].value = val;
        }
        saveVjConfigToCache();
    }
}

void DeckPlayer::setVjParamMod(int param_idx, int mod) noexcept
{
    if (param_idx >= 0 && param_idx < 8) {
        {
            std::lock_guard<std::mutex> lock(vj_config_mutex_);
            vj_config_.params[param_idx].mod_mode = mod;
            const int s = std::clamp(vj_config_.shader_idx, 0, 11);
            vj_config_.all_shader_params[s][param_idx].mod_mode = mod;
        }
        saveVjConfigToCache();
    }
}

void DeckPlayer::setVjShaderIdx(int idx) noexcept
{
    const int new_s = std::clamp(idx, 0, 11);
    {
        std::lock_guard<std::mutex> lock(vj_config_mutex_);
        if (vj_config_.shader_idx != new_s) {
            const int old_s = std::clamp(vj_config_.shader_idx, 0, 11);
            // 1. Save current active params into old shader slot
            vj_config_.all_shader_params[old_s] = vj_config_.params;
            // 2. Switch shader index
            vj_config_.shader_idx = new_s;
            // 3. Restore per-shader customized params
            vj_config_.params = vj_config_.all_shader_params[new_s];
        }
    }
    saveVjConfigToCache();
}

void DeckPlayer::setVjCustomColor(float r, float g, float b, int mod_mode) noexcept
{
    {
        std::lock_guard<std::mutex> lock(vj_config_mutex_);
        vj_config_.custom_color_r = r;
        vj_config_.custom_color_g = g;
        vj_config_.custom_color_b = b;
        vj_config_.color_mod_mode = mod_mode;
    }
    saveVjConfigToCache();
}

void DeckPlayer::saveVjConfigToCache()
{
    const QString fp = filePath();
    if (fp.isEmpty()) return;
    VjDeckConfig cfg = vjConfig();
    AudioAnalyzer::updateCacheVjConfig(fp, cfg);
}

void DeckPlayer::loadVjConfigFromCache()
{
    const QString fp = filePath();
    if (fp.isEmpty()) return;
    TrackAnalysisResult res;
    if (AudioAnalyzer::loadCache(fp, res) && res.has_vj_config) {
        std::lock_guard<std::mutex> lock(vj_config_mutex_);
        vj_config_ = res.vj_config;
        const int s = std::clamp(vj_config_.shader_idx, 0, 11);
        vj_config_.all_shader_params[s] = vj_config_.params;
    }
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
    // A RAM audio motor kezeli a hangot; a belső FFmpeg videodekóder forrás mindig némítva marad
    if (source_) {
        obs_source_set_audio_active(source_, false);
        obs_source_set_muted(source_, true);
        obs_source_set_volume(source_, 0.0f);
        obs_source_set_audio_mixers(source_, 0);
        obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);
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
    // A deck valódi hangerejét és csillapítását a RAM audió szál (processAudioDsp) végzi,
    // a belső FFmpeg forrás soha nem adhat ki natív hangot.
    if (source_) {
        obs_source_set_volume(source_, 0.0f);
        obs_source_set_muted(source_, true);
        obs_source_set_audio_active(source_, false);
        obs_source_set_audio_mixers(source_, 0);
        obs_source_set_monitoring_type(source_, OBS_MONITORING_TYPE_NONE);
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
    LiveEnvelopes envs;
    LiveEnvelopes *penvs = (channel_idx == 0) ? &envs : nullptr;

    EngineDsp::process(channel_data, num_frames, channel_idx, sample_rate,
                       g_low, g_mid, g_hi, f_val, dsp_states_[ch], penvs);

    if (penvs) {
        auto updateEnvelope = [](std::atomic<float> &atom, float target) {
            float cur = atom.load(std::memory_order_relaxed);
            if (target > cur) {
                cur = cur * 0.20f + target * 0.80f; // Fast punchy attack
            } else {
                cur = cur * 0.86f; // Smooth musical decay
            }
            atom.store(cur, std::memory_order_relaxed);
        };

        updateEnvelope(live_amp_, envs.amp);
        updateEnvelope(live_bass_, envs.bass);
        updateEnvelope(live_mid_, envs.mid);
        updateEnvelope(live_high_, envs.high);
    }
}

void DeckPlayer::setLoopBeatIndex(int index) noexcept
{
    loop_beat_index_.store(std::clamp(index, 0, 11), std::memory_order_relaxed);
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

    const int idx = loop_beat_index_.load(std::memory_order_relaxed);
    const double beat_ms = beatDurationMs();
    const int64_t loop_len = EngineLoop::calculateLoopLengthMs(beat_ms, idx);

    int64_t start = 0;
    const bool already_looping = looping_.load(std::memory_order_relaxed);

    if (already_looping) {
        start = loop_start_ms_.load(std::memory_order_relaxed);
    } else {
        const int64_t current = currentTimeMs();
        start = EngineLoop::quantizeLoopStart(current, gridOffsetMs(), beat_ms);
    }

    const int64_t min_len = static_cast<int64_t>(std::max(10.0, beat_ms * 0.05));
    const int64_t end = start + std::max<int64_t>(min_len, loop_len);

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

    // Csak média betöltésekor (Load) kényszerítünk szünetet a CUE (00:00) pozíciónál
    if (pause_on_load_pending_.load(std::memory_order_relaxed)) {
        const obs_media_state st = obs_source_media_get_state(source_);
        obs_source_media_play_pause(source_, true);
        const int64_t cue = std::max<int64_t>(0, cue_point_ms_.load(std::memory_order_relaxed));
        obs_source_media_set_time(source_, static_cast<uint64_t>(cue));
        if (st == OBS_MEDIA_STATE_PAUSED || st == OBS_MEDIA_STATE_STOPPED) {
            pause_on_load_pending_.store(false, std::memory_order_relaxed);
        }
        return;
    }

    // Normál felhasználói megállításkor (Pause/Stop) az aktuális pozíción marad a kép és a hang!
    if (!user_playing_.load(std::memory_order_relaxed)) {
        if (obs_source_media_get_state(source_) == OBS_MEDIA_STATE_PLAYING) {
            obs_source_media_play_pause(source_, true);
        }
        return;
    }

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

    const int64_t loop_video_seek = pending_loop_video_seek_ms_.exchange(-1, std::memory_order_acq_rel);
    if (loop_video_seek >= 0 && source_) {
        obs_source_media_set_time(source_, static_cast<uint64_t>(loop_video_seek));
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
    // Maintain sync flag and exact unrounded target bpm even after applySpeedSettings
    is_sync_.store(true, std::memory_order_relaxed);
    playback_bpm_.store(target_bpm, std::memory_order_relaxed);
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

// =============================================================================
// Ütemfázis szinkronizálása a másik deckhez (Beat-Phase Synchronization)
// =============================================================================
void DeckPlayer::syncPhaseTo(const DeckPlayer &other)
{
    const double other_base = (other.bpm() > 10.0) ? other.bpm() : 128.0;
    const double other_beat = 60000.0 / other_base;
    const int64_t other_cur = other.currentTimeMs();
    const int64_t other_grid = other.gridOffsetMs();
    const double other_rel = static_cast<double>(other_cur - other_grid);
    const double other_phase_frac = std::fmod(std::fmod(other_rel, other_beat) + other_beat, other_beat) / other_beat;

    const double my_base = (bpm() > 10.0) ? bpm() : 128.0;
    const double my_beat = 60000.0 / my_base;
    const int64_t my_cur = currentTimeMs();
    const int64_t my_grid = gridOffsetMs();
    const double my_rel = static_cast<double>(my_cur - my_grid);
    const double my_phase_frac = std::fmod(std::fmod(my_rel, my_beat) + my_beat, my_beat) / my_beat;

    double frac_diff = other_phase_frac - my_phase_frac;
    if (frac_diff > 0.5) frac_diff -= 1.0;
    if (frac_diff < -0.5) frac_diff += 1.0;

    const double shift_ms = frac_diff * my_beat;

    if (hasAudioBuffer()) {
        std::lock_guard<std::mutex> lock(audio_buf_mutex_);
        if (audio_sample_rate_ > 0) {
            const double shift_samples = (shift_ms * static_cast<double>(audio_sample_rate_)) / 1000.0;
            playback_sample_pos_ += shift_samples;
            if (playback_sample_pos_ < 0.0) playback_sample_pos_ = 0.0;
            if (!audio_pcm_l_.empty() && playback_sample_pos_ >= static_cast<double>(audio_pcm_l_.size())) {
                playback_sample_pos_ = static_cast<double>(audio_pcm_l_.size() - 1);
            }
        }
    }

    if (source_) {
        const int64_t new_time = std::max<int64_t>(0, my_cur + static_cast<int64_t>(std::round(shift_ms)));
        obs_source_media_set_time(source_, static_cast<uint64_t>(new_time));
    }
}

// =============================================================================
// Jog Nudge / Pitch Bend: lejátszási pozíció pillanatnyi léptetése (szinkronigazítás)
// (Nudges current playback position forward/backward without changing tempo or grid)
// =============================================================================
void DeckPlayer::nudgePlayback(int64_t delta_ms) noexcept
{
    if (hasAudioBuffer()) {
        std::lock_guard<std::mutex> lock(audio_buf_mutex_);
        if (audio_sample_rate_ > 0) {
            const double delta_samples = (static_cast<double>(delta_ms) * static_cast<double>(audio_sample_rate_)) / 1000.0;
            playback_sample_pos_ += delta_samples;
            if (playback_sample_pos_ < 0.0) playback_sample_pos_ = 0.0;
            if (!audio_pcm_l_.empty() && playback_sample_pos_ >= static_cast<double>(audio_pcm_l_.size())) {
                playback_sample_pos_ = static_cast<double>(audio_pcm_l_.size() - 1);
            }
        }
    }

    if (source_) {
        const int64_t cur = currentTimeMs();
        const int64_t new_t = std::max<int64_t>(0, cur + delta_ms);
        obs_source_media_set_time(source_, static_cast<uint64_t>(new_t));
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
    if (hasAudioBuffer()) {
        std::lock_guard<std::mutex> lock(audio_buf_mutex_);
        if (audio_sample_rate_ > 0) {
            return static_cast<int64_t>((playback_sample_pos_ * 1000.0) / static_cast<double>(audio_sample_rate_));
        }
    }
    if (!source_)
        return cue_point_ms_.load(std::memory_order_relaxed);
    return obs_source_media_get_time(source_);
}

int64_t DeckPlayer::durationMs() const
{
    if (hasAudioBuffer()) {
        std::lock_guard<std::mutex> lock(audio_buf_mutex_);
        if (audio_sample_rate_ > 0 && !audio_pcm_l_.empty()) {
            return static_cast<int64_t>((static_cast<double>(audio_pcm_l_.size()) * 1000.0) / static_cast<double>(audio_sample_rate_));
        }
    }
    if (!source_)
        return 0;
    return obs_source_media_get_duration(source_);
}

// =============================================================================
// RAM Audio Puffer metódusok (Zero-Stutter RAM Audio Engine Methods)
// =============================================================================
void DeckPlayer::setAudioBuffer(const std::vector<float> &l, const std::vector<float> &r, uint32_t sr)
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex_);
    audio_pcm_l_ = l;
    audio_pcm_r_ = r;
    audio_sample_rate_ = (sr > 0) ? sr : 44100;
    const int64_t cur = user_playing_.load(std::memory_order_relaxed) ? ((source_) ? obs_source_media_get_time(source_) : 0) : cue_point_ms_.load(std::memory_order_relaxed);
    playback_sample_pos_ = (static_cast<double>(cur) * static_cast<double>(audio_sample_rate_)) / 1000.0;
}

void DeckPlayer::setAudioBuffer(std::vector<float> &&l, std::vector<float> &&r, uint32_t sr)
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex_);
    audio_pcm_l_ = std::move(l);
    audio_pcm_r_ = std::move(r);
    audio_sample_rate_ = (sr > 0) ? sr : 44100;
    const int64_t cur = user_playing_.load(std::memory_order_relaxed) ? ((source_) ? obs_source_media_get_time(source_) : 0) : cue_point_ms_.load(std::memory_order_relaxed);
    playback_sample_pos_ = (static_cast<double>(cur) * static_cast<double>(audio_sample_rate_)) / 1000.0;
}

bool DeckPlayer::hasAudioBuffer() const noexcept
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex_);
    return !audio_pcm_l_.empty() && !audio_pcm_r_.empty() && audio_sample_rate_ > 0;
}

void DeckPlayer::clearAudioBuffer()
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex_);
    audio_pcm_l_.clear();
    audio_pcm_r_.clear();
    playback_sample_pos_ = 0.0;
    sola_initialized_ = false;
    sola_out_l_.clear();
    sola_out_r_.clear();
    sola_last_grain_l_.clear();
    sola_last_grain_r_.clear();
    sola_out_read_pos_ = 0.0;
}

size_t DeckPlayer::readAudioFrames(float *out_l, float *out_r, size_t frames, uint32_t out_sr)
{
    if (!out_l || !out_r || frames == 0) return 0;
    if (!user_playing_.load(std::memory_order_relaxed)) {
        std::fill(out_l, out_l + frames, 0.0f);
        std::fill(out_r, out_r + frames, 0.0f);
        return frames;
    }

    std::lock_guard<std::mutex> lock(audio_buf_mutex_);
    const size_t total_samples = audio_pcm_l_.size();
    if (total_samples < 2 || audio_sample_rate_ == 0 || out_sr == 0) {
        std::fill(out_l, out_l + frames, 0.0f);
        std::fill(out_r, out_r + frames, 0.0f);
        return frames;
    }

    const double eff_bpm = effectiveBpm();
    const double b_bpm = bpm();
    double speed_ratio = 1.0;
    if (b_bpm > 10.0 && eff_bpm > 10.0) {
        speed_ratio = eff_bpm / b_bpm;
    } else {
        speed_ratio = static_cast<double>(speed()) / 100.0;
    }
    speed_ratio = std::clamp(speed_ratio, 0.5, 2.0);

    const double step = speed_ratio * (static_cast<double>(audio_sample_rate_) / static_cast<double>(out_sr));

    const bool looping = looping_.load(std::memory_order_relaxed);
    const int64_t loop_start_ms = loop_start_ms_.load(std::memory_order_relaxed);
    const int64_t loop_end_ms = loop_end_ms_.load(std::memory_order_relaxed);
    const double loop_start_sample = (static_cast<double>(loop_start_ms) * static_cast<double>(audio_sample_rate_)) / 1000.0;
    const double loop_end_sample = (static_cast<double>(loop_end_ms) * static_cast<double>(audio_sample_rate_)) / 1000.0;

    const bool key_lock_active = key_lock_.load(std::memory_order_relaxed);
    const bool is_near_unity = std::abs(speed_ratio - 1.0) < 0.0005;

    if (key_lock_active && !is_near_unity) {
        // Professzionális WSOLA (Waveform Similarity Overlap-Add) Time-Stretching
        // Valós idejű hangmagasság-tartás recsegés és frekvencia-elcsúszás nélkül
        const size_t W = 1024;
        const size_t O = 512;
        const size_t H_s = 512;
        const int K_max = 256;

        if (!sola_initialized_ || sola_last_grain_l_.size() != O) {
            sola_last_grain_l_.assign(O, 0.0f);
            sola_last_grain_r_.assign(O, 0.0f);
            const size_t p0 = static_cast<size_t>(std::clamp(playback_sample_pos_, 0.0, static_cast<double>(total_samples > (O + 2) ? total_samples - (O + 2) : 0)));
            for (size_t j = 0; j < O; ++j) {
                sola_last_grain_l_[j] = audio_pcm_l_[p0 + j];
                sola_last_grain_r_[j] = audio_pcm_r_[p0 + j];
            }
            sola_prev_in_pos_ = static_cast<int64_t>(p0);
            sola_in_pos_ = static_cast<double>(p0 + O);
            sola_out_l_.clear();
            sola_out_r_.clear();
            sola_out_read_pos_ = 0.0;
            sola_initialized_ = true;
        }

        // Resampling lépés az audio mintavételezésről (pl. 44.1k) az OBS kimenetre (pl. 48.0k)
        const double sr_step = static_cast<double>(audio_sample_rate_) / static_cast<double>(out_sr);
        const double needed_sola_samples = static_cast<double>(frames) * sr_step + 4.0;

        // Generálunk WSOLA szemcséket, amíg nincs elég minta az out pufferben
        while ((static_cast<double>(sola_out_l_.size()) - sola_out_read_pos_) < needed_sola_samples) {
            // Varratmentes kattogásmentes loop visszaugrás (Seamless Loop Wrap)
            if (looping && loop_end_sample > loop_start_sample + 100.0) {
                if (sola_in_pos_ >= loop_end_sample) {
                    const double overshoot = sola_in_pos_ - loop_end_sample;
                    sola_in_pos_ = loop_start_sample + overshoot;
                    playback_sample_pos_ = sola_in_pos_;
                    sola_prev_in_pos_ = static_cast<int64_t>(loop_start_sample);
                    pending_loop_video_seek_ms_.store(loop_start_ms, std::memory_order_release);
                }
            }

            // Szám vége ellenőrzése
            if (sola_in_pos_ + static_cast<double>(W + K_max) >= static_cast<double>(total_samples)) {
                user_playing_.store(false, std::memory_order_relaxed);
                break;
            }

            // Névleges célpozíció a tempóarány (speed_ratio) alapján
            const int64_t p_target = static_cast<int64_t>(std::round(sola_in_pos_));
            // Természetes akusztikai folytatás referencia-pozíciója
            const int64_t p_ref = sola_prev_in_pos_ + static_cast<int64_t>(H_s);

            int best_k = 0;
            float min_diff = 1e12f;

            // Fázisillesztés keresése Sum of Absolute Differences (SAD) segítségével
            for (int k = -K_max; k <= K_max; k += 2) {
                const int64_t cand = p_target + k;
                if (cand < 0 || cand + static_cast<int64_t>(O) >= static_cast<int64_t>(total_samples))
                    continue;
                if (p_ref < 0 || p_ref + static_cast<int64_t>(O) >= static_cast<int64_t>(total_samples))
                    continue;

                float diff = 0.0f;
                for (size_t j = 0; j < O; j += 4) {
                    diff += std::abs(audio_pcm_l_[static_cast<size_t>(p_ref) + j] -
                                     audio_pcm_l_[static_cast<size_t>(cand) + j]);
                }
                if (diff < min_diff) {
                    min_diff = diff;
                    best_k = k;
                }
            }

            int64_t p_actual = p_target + best_k;
            if (p_actual < 0) p_actual = 0;
            if (p_actual + static_cast<int64_t>(W) >= static_cast<int64_t>(total_samples)) {
                user_playing_.store(false, std::memory_order_relaxed);
                break;
            }

            // Hann raised-cosine átfedés és szintézis (512 minta)
            const float inv_O = 3.14159265f / static_cast<float>(O);
            for (size_t j = 0; j < O; ++j) {
                const float w = 0.5f * (1.0f - std::cos(static_cast<float>(j) * inv_O));
                const float s_l = (1.0f - w) * sola_last_grain_l_[j] + w * audio_pcm_l_[static_cast<size_t>(p_actual) + j];
                const float s_r = (1.0f - w) * sola_last_grain_r_[j] + w * audio_pcm_r_[static_cast<size_t>(p_actual) + j];
                sola_out_l_.push_back(s_l);
                sola_out_r_.push_back(s_r);
            }

            // A következő szemcsével való átfedéshez elmentjük az aktuális hullám folytatását
            for (size_t j = 0; j < O; ++j) {
                sola_last_grain_l_[j] = audio_pcm_l_[static_cast<size_t>(p_actual) + H_s + j];
                sola_last_grain_r_[j] = audio_pcm_r_[static_cast<size_t>(p_actual) + H_s + j];
            }

            sola_prev_in_pos_ = p_actual;
            sola_in_pos_ += static_cast<double>(H_s) * speed_ratio;
            playback_sample_pos_ = sola_in_pos_;
        }

        // Másoljuk a szükséges kimeneti mintákat lineáris interpolációval 44.1k -> 48k
        for (size_t i = 0; i < frames; ++i) {
            const size_t idx0 = static_cast<size_t>(sola_out_read_pos_);
            if (idx0 + 1 < sola_out_l_.size()) {
                const double frac = sola_out_read_pos_ - static_cast<double>(idx0);
                out_l[i] = static_cast<float>((1.0 - frac) * sola_out_l_[idx0] + frac * sola_out_l_[idx0 + 1]);
                out_r[i] = static_cast<float>((1.0 - frac) * sola_out_r_[idx0] + frac * sola_out_r_[idx0 + 1]);
            } else if (idx0 < sola_out_l_.size()) {
                out_l[i] = sola_out_l_[idx0];
                out_r[i] = sola_out_r_[idx0];
            } else {
                out_l[i] = 0.0f;
                out_r[i] = 0.0f;
            }
            sola_out_read_pos_ += sr_step;
        }

        // Periodikus puffer-tisztítás memóriafelhalmozódás ellen
        if (sola_out_read_pos_ > 4096.0) {
            const size_t drop = static_cast<size_t>(sola_out_read_pos_);
            sola_out_l_.erase(sola_out_l_.begin(), sola_out_l_.begin() + drop);
            sola_out_r_.erase(sola_out_r_.begin(), sola_out_r_.begin() + drop);
            sola_out_read_pos_ -= static_cast<double>(drop);
        }
    } else {
        // Ha visszatértünk 1.0x sebességre, a WSOLA puffert alaphelyzetbe állítjuk
        if (sola_initialized_) {
            sola_initialized_ = false;
            sola_out_l_.clear();
            sola_out_r_.clear();
            sola_out_read_pos_ = 0.0;
        }

        // Hagyományos vinil/szalag varispeed és 1.0x bit-perfect tiszta lejátszás
        for (size_t i = 0; i < frames; ++i) {
            if (looping && loop_end_sample > loop_start_sample + 100.0) {
                if (playback_sample_pos_ >= loop_end_sample) {
                    loop_fade_old_pos_ = playback_sample_pos_;
                    playback_sample_pos_ = loop_start_sample + (playback_sample_pos_ - loop_end_sample);
                    loop_fade_rem_samples_ = std::max(256, static_cast<int>(audio_sample_rate_ * 15 / 1000)); // 15 ms lágy átfedés
                    loop_fade_total_samples_ = loop_fade_rem_samples_;
                    pending_loop_video_seek_ms_.store(loop_start_ms, std::memory_order_release);
                }
            }

            if (playback_sample_pos_ < 0.0) {
                playback_sample_pos_ = 0.0;
            }

            size_t idx0 = static_cast<size_t>(playback_sample_pos_);
            if (idx0 + 1 >= total_samples) {
                out_l[i] = 0.0f;
                out_r[i] = 0.0f;
                user_playing_.store(false, std::memory_order_relaxed);
                break;
            }

            const double frac = playback_sample_pos_ - static_cast<double>(idx0);
            float sample_l = static_cast<float>((1.0 - frac) * audio_pcm_l_[idx0] + frac * audio_pcm_l_[idx0 + 1]);
            float sample_r = static_cast<float>((1.0 - frac) * audio_pcm_r_[idx0] + frac * audio_pcm_r_[idx0 + 1]);

            if (loop_fade_rem_samples_ > 0 && loop_fade_total_samples_ > 0) {
                const size_t old_idx = static_cast<size_t>(loop_fade_old_pos_);
                if (old_idx + 1 < total_samples) {
                    const double old_frac = loop_fade_old_pos_ - static_cast<double>(old_idx);
                    const float old_l = static_cast<float>((1.0 - old_frac) * audio_pcm_l_[old_idx] + old_frac * audio_pcm_l_[old_idx + 1]);
                    const float old_r = static_cast<float>((1.0 - old_frac) * audio_pcm_r_[old_idx] + old_frac * audio_pcm_r_[old_idx + 1]);

                    const float phase = static_cast<float>(loop_fade_total_samples_ - loop_fade_rem_samples_) / static_cast<float>(loop_fade_total_samples_);
                    // Equal-power crossfade (kattanásmentes, energiát megőrző átmenet)
                    const float fade_in = std::sin(phase * 1.5707963f);
                    const float fade_out = std::cos(phase * 1.5707963f);
                    sample_l = sample_l * fade_in + old_l * fade_out;
                    sample_r = sample_r * fade_in + old_r * fade_out;
                    loop_fade_old_pos_ += step;
                }
                loop_fade_rem_samples_--;
            }

            out_l[i] = sample_l;
            out_r[i] = sample_r;

            playback_sample_pos_ += step;
        }
    }

    return frames;
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
    const int old_mode = master_mode_.load(std::memory_order_relaxed);
    if (mode == MASTER_CLOCK && old_mode != MASTER_CLOCK) {
        // Amikor Deckről váltunk át Master Clockra, átvesszük az aktív tempót
        double cur_tempo = 0.0;
        if (old_mode == MASTER_DECK_A && vj_deck_a()) cur_tempo = vj_deck_a()->effectiveBpm();
        else if (old_mode == MASTER_DECK_B && vj_deck_b()) cur_tempo = vj_deck_b()->effectiveBpm();
        else if (vj_deck_a() && vj_deck_a()->isPlaying()) cur_tempo = vj_deck_a()->effectiveBpm();
        else if (vj_deck_b() && vj_deck_b()->isPlaying()) cur_tempo = vj_deck_b()->effectiveBpm();

        if (cur_tempo > 30.0) {
            master_clock_bpm_.store(cur_tempo, std::memory_order_relaxed);
        }
    }

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
    const double clamped = std::clamp(bpm, 40.0, 300.0);
    master_clock_bpm_.store(clamped, std::memory_order_relaxed);
    master_mode_.store(MASTER_CLOCK, std::memory_order_relaxed);
    if (vj_deck_a()) vj_deck_a()->setMaster(false);
    if (vj_deck_b()) vj_deck_b()->setMaster(false);
}

double DeckPair::masterClockBpm() const noexcept
{
    return master_clock_bpm_.load(std::memory_order_relaxed);
}

double DeckPair::effectiveMasterBpm() const noexcept
{
    const int mode = master_mode_.load(std::memory_order_relaxed);
    const double clk = master_clock_bpm_.load(std::memory_order_relaxed);
    const bool da_p = vj_deck_a() ? vj_deck_a()->isPlaying() : false;
    const double da_bpm = vj_deck_a() ? vj_deck_a()->effectiveBpm() : 128.0;
    const bool db_p = vj_deck_b() ? vj_deck_b()->isPlaying() : false;
    const double db_bpm = vj_deck_b() ? vj_deck_b()->effectiveBpm() : 128.0;

    return EngineSync::calculateEffectiveMasterBpm(mode, clk, da_p, da_bpm, db_p, db_bpm);
}

void DeckPair::setSync(int deck_idx, bool sync_enabled)
{
    DeckPlayer *target = (deck_idx == 0) ? vj_deck_a() : vj_deck_b();
    DeckPlayer *other  = (deck_idx == 0) ? vj_deck_b() : vj_deck_a();
    if (!target) return;
    target->setSync(sync_enabled);
    if (sync_enabled) {
        // Ha a cél deck a Master, a saját tempója a referencia: tilos elállítani a másik deck 128-as alapértékére!
        if (target->isMaster()) {
            if (other && other->isPlaying() && target->isPlaying()) {
                target->syncPhaseTo(*other);
            }
            return;
        }

        const int mode = master_mode_.load(std::memory_order_relaxed);
        const double clk = master_clock_bpm_.load(std::memory_order_relaxed);
        const bool other_master = other ? other->isMaster() : false;
        const bool other_playing = other ? other->isPlaying() : false;
        const double other_eff = other ? other->effectiveBpm() : 128.0;
        const bool other_track = other ? (other->durationMs() > 0) : false;
        const double other_base = other ? other->bpm() : 128.0;

        const double target_bpm = EngineSync::calculateTargetBpmForSync(
            target->isMaster(), mode, clk, other_master, other_playing, other_eff, other_track, other_base);

        if (target_bpm > 30.0) {
            target->syncToBpm(target_bpm);
        }

        // Ütemfázis azonnali összehangolása (Beat Phase Alignment)
        if (other && (other->isPlaying() || target->isPlaying())) {
            target->syncPhaseTo(*other);
        }
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
        if (masterMode() == MASTER_DECK_A) {
            if (vj_deck_a()) master_clock_bpm_.store(vj_deck_a()->effectiveBpm(), std::memory_order_relaxed);
            setMasterMode(MASTER_CLOCK);
        } else {
            setMasterMode(MASTER_DECK_A);
            if (vj_deck_a() && vj_deck_a()->bpm() > 10.0) {
                master_clock_bpm_.store(vj_deck_a()->effectiveBpm(), std::memory_order_relaxed);
            }
        }
    } else {
        if (masterMode() == MASTER_DECK_B) {
            if (vj_deck_b()) master_clock_bpm_.store(vj_deck_b()->effectiveBpm(), std::memory_order_relaxed);
            setMasterMode(MASTER_CLOCK);
        } else {
            setMasterMode(MASTER_DECK_B);
            if (vj_deck_b() && vj_deck_b()->bpm() > 10.0) {
                master_clock_bpm_.store(vj_deck_b()->effectiveBpm(), std::memory_order_relaxed);
            }
        }
    }
}

void DeckPair::updateSync()
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (!da || !db) return;

    const int mode = master_mode_.load(std::memory_order_relaxed);

    // Automatikus Master átadás a másik deckre, ha a Master deck leállt vagy a track végére ért
    if (mode == MASTER_DECK_A && (!da->isPlaying() || da->isAtEnd())) {
        if (db->isPlaying()) {
            setMasterMode(MASTER_DECK_B);
        }
    } else if (mode == MASTER_DECK_B && (!db->isPlaying() || db->isAtEnd())) {
        if (da->isPlaying()) {
            setMasterMode(MASTER_DECK_A);
        }
    }

    const double master_bpm = effectiveMasterBpm();

    if (da->isSync() && !da->isMaster()) {
        double target_bpm = master_bpm;
        if (mode == MASTER_DECK_B) {
            target_bpm = db->effectiveBpm();
        } else if (mode == MASTER_CLOCK || mode == MASTER_EXT_MIDI) {
            target_bpm = master_bpm;
        } else if (db->isMaster()) {
            target_bpm = db->effectiveBpm();
        } else if (db->isPlaying()) {
            target_bpm = db->effectiveBpm();
        }
        const double diff = std::abs(da->effectiveBpm() - target_bpm);
        if (diff > 0.05) da->syncToBpm(target_bpm);
    }
    if (db->isSync() && !db->isMaster()) {
        double target_bpm = master_bpm;
        if (mode == MASTER_DECK_A) {
            target_bpm = da->effectiveBpm();
        } else if (mode == MASTER_CLOCK || mode == MASTER_EXT_MIDI) {
            target_bpm = master_bpm;
        } else if (da->isMaster()) {
            target_bpm = da->effectiveBpm();
        } else if (da->isPlaying()) {
            target_bpm = da->effectiveBpm();
        }
        const double diff = std::abs(db->effectiveBpm() - target_bpm);
        if (diff > 0.05) db->syncToBpm(target_bpm);
    }
}

void DeckPair::syncTempoAtoB()
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (da && db) {
        da->syncToBpm(db->effectiveBpm());
        da->syncPhaseTo(*db);
    }
}

void DeckPair::syncTempoBtoA()
{
    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (da && db) {
        db->syncToBpm(da->effectiveBpm());
        db->syncPhaseTo(*da);
    }
}

void DeckPair::setLoopBeatIndex(int deck_index, int beat_index)
{
    const int clamped = std::clamp(beat_index, 0, 11);
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

obs_source_t *DeckPair::cueSource() const noexcept
{
    std::lock_guard<std::mutex> lock(cue_source_mutex_);
    return cue_source_;
}

void DeckPair::setCueSource(obs_source_t *s) noexcept
{
    std::lock_guard<std::mutex> lock(cue_source_mutex_);
    cue_source_ = s;
}

void DeckPair::setCueDeviceId(const std::string &id) noexcept
{
    std::lock_guard<std::mutex> lock(cue_dev_mutex_);
    if (cue_device_id_ != id) {
        cue_device_id_ = id;
        cue_device_changed_.store(true, std::memory_order_relaxed);
        cue_cv_.notify_all();
    }
}

std::string DeckPair::cueDeviceId() const noexcept
{
    std::lock_guard<std::mutex> lock(cue_dev_mutex_);
    return cue_device_id_;
}

void DeckPair::enableAudioMonitoring() noexcept
{
    if (vj_deck_a()) vj_deck_a()->enableAudioMonitoring();
    if (vj_deck_b()) vj_deck_b()->enableAudioMonitoring();
}

// =============================================================================
// Fejhallgató CUE audió sorbaállítása (Headphone CUE Audio Queueing)
// =============================================================================
void DeckPair::mixAndQueueCue(const float *a_l, const float *a_r,
                              const float *b_l, const float *b_r,
                              size_t frames)
{
    if (frames == 0) return;

    const float hp_vol = headphone_volume_.load(std::memory_order_relaxed);
    if (hp_vol <= 0.001f) return;

    const float hp_mix = headphone_mix_.load(std::memory_order_relaxed); // 0.0 = pure CUE, 1.0 = pure PGM / Master

    DeckPlayer *da = vj_deck_a();
    DeckPlayer *db = vj_deck_b();
    if (!da || !db) return;

    const bool cue_a = da->isHeadphoneCue();
    const bool cue_b = db->isHeadphoneCue();

    // Ha nincs CUE bekapcsolva és a Master CUE/PGM potméter 0 (csak CUE), akkor csend
    if (!cue_a && !cue_b && hp_mix <= 0.001f)
        return;

    const float gain_a = da->gain();
    const float vol_a = da->volume();
    const float mix_gain_a = da->mixGain();

    const float gain_b = db->gain();
    const float vol_b = db->volume();
    const float mix_gain_b = db->mixGain();

    const float cue_weight = 1.0f - hp_mix;
    const float pgm_weight = hp_mix;

    CuePacket pkt;
    pkt.l.resize(frames);
    pkt.r.resize(frames);
    pkt.frames = frames;

    for (size_t i = 0; i < frames; ++i) {
        // 1. CUE (PFL) komponens: PRE-FADER, PRE-CROSSFADER!
        // Teljesen független a csatornahangerőtől és a crossfadertől, csak a Gain hat rá!
        float cue_l = 0.0f;
        float cue_r = 0.0f;
        if (cue_a && a_l && a_r) {
            cue_l += a_l[i] * gain_a;
            cue_r += a_r[i] * gain_a;
        }
        if (cue_b && b_l && b_r) {
            cue_l += b_l[i] * gain_b;
            cue_r += b_r[i] * gain_b;
        }

        // 2. PGM (Program / Master) komponens: POST-FADER, POST-CROSSFADER
        float pgm_l = 0.0f;
        float pgm_r = 0.0f;
        if (a_l && a_r) {
            pgm_l += a_l[i] * (gain_a * vol_a * mix_gain_a);
            pgm_r += a_r[i] * (gain_a * vol_a * mix_gain_a);
        }
        if (b_l && b_r) {
            pgm_l += b_l[i] * (gain_b * vol_b * mix_gain_b);
            pgm_r += b_r[i] * (gain_b * vol_b * mix_gain_b);
        }

        // 3. Fülesbe keverés az arány (CUE / PGM) és a fejhallgató hangerő szerint
        pkt.l[i] = std::clamp((cue_l * cue_weight + pgm_l * pgm_weight) * hp_vol, -1.2f, 1.2f);
        pkt.r[i] = std::clamp((cue_r * cue_weight + pgm_r * pgm_weight) * hp_vol, -1.2f, 1.2f);
    }

    std::lock_guard<std::mutex> lock(cue_mutex_);
    if (cue_queue_.size() > 15)
        cue_queue_.pop();
    cue_queue_.push(std::move(pkt));
    cue_cv_.notify_one();
}

void DeckPair::queueCueAudio(int deck_idx, const float *const *channels, size_t frames)
{
    // Ha a Master Mixer forrás aktív a jelenetben, az a mixAndQueueCue hívással egyben küldi
    // mindkét deck mintáját, így a különálló deck hívásokat átugorjuk a szétcsúszás elkerülésére
    if (master_source_ != nullptr)
        return;

    if (!channels || !channels[0] || frames == 0)
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

    // PRE-fader CUE + POST-fader Master
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

// =============================================================================
// Fejhallgató CUE háttérszál (Headphone CUE Background Streaming Thread)
// =============================================================================
void DeckPair::cueWorkerLoop()
{
    WasapiOutput wasapi_out;
    std::string active_dev;

    uint64_t cue_next_timestamp = 0;
    while (cue_thread_running_.load(std::memory_order_relaxed)) {
        CuePacket pkt;
        {
            std::unique_lock<std::mutex> lock(cue_mutex_);
            cue_cv_.wait_for(lock, std::chrono::milliseconds(20), [this]() {
                return !cue_queue_.empty() || !cue_thread_running_.load(std::memory_order_relaxed) || cue_device_changed_.load(std::memory_order_relaxed);
            });
            if (!cue_thread_running_.load(std::memory_order_relaxed))
                break;

            if (cue_device_changed_.load(std::memory_order_relaxed)) {
                cue_device_changed_.store(false, std::memory_order_relaxed);
                active_dev = cueDeviceId();
                wasapi_out.open(active_dev);
            }

            if (cue_queue_.empty())
                continue;
            pkt = std::move(cue_queue_.front());
            cue_queue_.pop();
        }

        if (!wasapi_out.isRunning()) {
            static auto last_retry = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            if (now - last_retry > std::chrono::seconds(2)) {
                last_retry = now;
                active_dev = cueDeviceId();
                wasapi_out.open(active_dev);
            }
        }

        if (pkt.frames > 0 && !pkt.l.empty() && !pkt.r.empty()) {
            // 1. Közvetlen hardveres WASAPI kimenet a kiválasztott fejhallgató eszközre (pl. OUT 3-4)
            wasapi_out.writeFrames(pkt.l.data(), pkt.r.data(), pkt.frames);

            // 2. Szintmérő kimenet az OBS CUE forrás felé (monitoring NONE mellett), hogy az OBS keverőben ugráljon a zöld csík
            obs_source_t *src = nullptr;
            {
                std::lock_guard<std::mutex> lock(cue_source_mutex_);
                src = cue_source_;
            }

            if (src) {
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

                obs_source_output_audio(src, &audio_out);
            }
        }
    }

    wasapi_out.close();
}
