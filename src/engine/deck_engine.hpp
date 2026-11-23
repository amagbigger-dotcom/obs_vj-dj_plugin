#pragma once

#include <obs.h>
#include <QString>
#include <QFileInfo>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "effects/vj_config.hpp"
#include "effects/crossfade_fx_config.hpp"
#include "engine/engine_dsp.hpp"
#include "engine/lfo_engine.hpp"
#include "engine/engine_loop.hpp"

class DeckPlayer {
public:
    DeckPlayer(const char *source_name = "vj_deck_source",
               const char *display_name = "OBS VJ Deck");
    ~DeckPlayer();

    void loadFile(const QString &path);

    QString fileName() const;
    void setFileName(const QString &name);
    QString filePath() const;
    void setFilePath(const QString &path);

    void play();
    void pause();
    void togglePlayback();
    void setPlaying(bool playing);
    bool isPlaying() const;

    // RAM Audio Puffer kezelés a varratmentes loophoz (RAM Audio Buffer for Seamless Looping)
    void setAudioBuffer(const std::vector<float> &l, const std::vector<float> &r, uint32_t sr);
    void setAudioBuffer(std::vector<float> &&l, std::vector<float> &&r, uint32_t sr);
    bool hasAudioBuffer() const noexcept;
    void clearAudioBuffer();
    size_t readAudioFrames(float *out_l, float *out_r, size_t frames, uint32_t out_sr);
    bool isPauseOnLoadPending() const noexcept { return pause_on_load_pending_.load(std::memory_order_relaxed); }

    void cue();
    int64_t cuePointMs() const noexcept { return cue_point_ms_.load(std::memory_order_relaxed); }
    void setCuePointMs(int64_t ms) noexcept;

    int64_t hotCueMs(int index) const noexcept;
    void setHotCueMs(int index, int64_t ms) noexcept;
    void clearHotCue(int index) noexcept;
    void triggerHotCue(int index) noexcept;
    std::array<int64_t, 4> allHotCues() const noexcept;

    int volumeFxMode() const noexcept { return volume_fx_mode_.load(std::memory_order_relaxed); }
    void setVolumeFxMode(int m) noexcept { volume_fx_mode_.store(m, std::memory_order_relaxed); }
    bool isVisualEqEnabled() const noexcept { return visual_eq_enabled_.load(std::memory_order_relaxed); }
    void setVisualEqEnabled(bool en) noexcept { visual_eq_enabled_.store(en, std::memory_order_relaxed); }
    bool isRgbInverted() const noexcept { return rgb_inverted_.load(std::memory_order_relaxed); }
    void setRgbInverted(bool inv) noexcept;
    void updateVisualEqMapping() noexcept;

    void seek(double normalized_pos);
    void seekMs(int64_t ms);

    void setSpeed(int percent);
    void applySpeedSettings(int speed_percent);
    int speed() const noexcept;

    void setBpm(double bpm, bool user_locked = true) noexcept;
    double bpm() const noexcept;
    void setPlaybackBpm(double bpm) noexcept;
    double effectiveBpm() const noexcept;
    double beatDurationMs() const noexcept;
    bool isBpmLocked() const noexcept { return user_bpm_locked_.load(std::memory_order_relaxed); }
    void setBpmLocked(bool locked) noexcept { user_bpm_locked_.store(locked, std::memory_order_relaxed); }

    void setGridOffsetMs(int64_t offset_ms, bool user_locked = true) noexcept;
    int64_t gridOffsetMs() const noexcept;
    void nudgeGrid(int64_t delta_ms, bool user_locked = true) noexcept;
    void setGridAnchorAtCurrent(bool user_locked = true) noexcept;
    bool isGridLocked() const noexcept { return user_grid_locked_.load(std::memory_order_relaxed); }
    void setGridLocked(bool locked) noexcept { user_grid_locked_.store(locked, std::memory_order_relaxed); }

    void saveUserEdits();

    void setWaveformSamples(const std::vector<float> &overview, const std::vector<float> &detailed);
    void setSpectralSamples(const std::vector<float> &ov_low, const std::vector<float> &ov_mid, const std::vector<float> &ov_high,
                            const std::vector<float> &det_low, const std::vector<float> &det_mid, const std::vector<float> &det_high);
    void setTonusSamples(const std::vector<float> &tonus);
    std::vector<float> overviewSamples() const;
    std::vector<float> overviewLow() const;
    std::vector<float> overviewMid() const;
    std::vector<float> overviewHigh() const;
    std::vector<float> overviewTonus() const;
    std::vector<float> detailedSamples() const;
    std::vector<float> detailedLow() const;
    std::vector<float> detailedMid() const;
    std::vector<float> detailedHigh() const;
    std::vector<float> waveformSamples() const;
    bool hasAnalyzedWaveform() const;
    bool hasSpectralData() const;
    float currentMacroTonus() const noexcept;

    void setEqHi(float val) noexcept;
    void setEqMid(float val) noexcept;
    void setEqLow(float val) noexcept;
    void setFilterVal(float val) noexcept;
    void setMixerFxType(int type) noexcept;

    float eqHi() const noexcept;
    float eqMid() const noexcept;
    float eqLow() const noexcept;
    float filterVal() const noexcept;
    int mixerFxType() const noexcept;

    // Per-deck 4-slot FX konfiguráció (Build #362)
    int activeSlot() const noexcept { return active_slot_.load(std::memory_order_relaxed); }
    void selectSlot(int slot) noexcept;
    int slotFxType(int slot) const noexcept;
    void setSlotFxType(int slot, int fx_type) noexcept;

    void processAudioDsp(float *channel_data, size_t num_frames, size_t channel_idx, size_t sample_rate);

    void setGain(float gain) noexcept;
    float gain() const noexcept;

    void setVolume(float volume);
    float volume() const noexcept;

    void setMixGain(float gain) noexcept;
    float mixGain() const noexcept;
    void enableAudioMonitoring() noexcept;

    void setHeadphoneCue(bool cue) noexcept;
    bool isHeadphoneCue() const noexcept;

    void setRed(float value) noexcept;
    void setGreen(float value) noexcept;
    void setBlue(float value) noexcept;
    void setSaturation(float value) noexcept;

    float red() const noexcept;
    float green() const noexcept;
    float blue() const noexcept;
    float saturation() const noexcept;

    void setLoopBeatIndex(int index) noexcept;
    int loopBeatIndex() const noexcept;
    void setLooping(bool looping);
    bool isLooping() const noexcept;
    int64_t loopStartMs() const noexcept;
    int64_t loopEndMs() const noexcept;
    void updateLoop();

    bool hasVideo() const noexcept { return has_video_.load(std::memory_order_relaxed); }
    bool hasAudioOnlyTrack() const noexcept {
        return hasAudioBuffer() && !hasVideo() && !filePath().isEmpty();
    }
    bool isSpeedRestarting() const noexcept { return speed_restart_phase_.load(std::memory_order_relaxed) != 0; }
    bool hasFileChanged() const noexcept { return file_changed_.load(std::memory_order_relaxed); }
    void clearFileChanged() noexcept { file_changed_.store(false, std::memory_order_relaxed); }

    QString camelotKey() const;
    QString detectedKey() const;
    void setKeys(const QString &camelot, const QString &key);

    float currentBassEnergy() const noexcept;
    float currentMidEnergy() const noexcept;
    float currentHighEnergy() const noexcept;
    float currentAmplitude() const noexcept;

    // VJ Shader Configuration & Parameter Modulation (Per-deck & Per-track)
    VjDeckConfig vjConfig() const noexcept;
    void setVjConfig(const VjDeckConfig &cfg) noexcept;
    void setVjParamValue(int param_idx, float val) noexcept;
    void setVjParamMod(int param_idx, int mod) noexcept;
    void setVjParamModDepth(int param_idx, float depth) noexcept;
    void setVjShaderIdx(int idx) noexcept;
    int vjShaderIdx() const noexcept;
    int vjShaderIdxPrev() const noexcept { return vj_shader_idx_prev_.load(std::memory_order_relaxed); }
    float vjShaderTransProgress() const noexcept { return vj_shader_trans_progress_.load(std::memory_order_relaxed); }
    void updateVjShaderTransition(float dt_sec) noexcept;
    int vjShaderTransBeats() const noexcept { return vj_shader_trans_beats_.load(std::memory_order_relaxed); }
    void setVjShaderTransBeats(int beats) noexcept { vj_shader_trans_beats_.store(std::clamp(beats, 0, 16), std::memory_order_relaxed); }
    void setVjCustomColor(float r, float g, float b, int mod_mode) noexcept;
    void saveVjConfigToCache();
    void loadVjConfigFromCache();

    enum class DeckSide { A, B };
    void setSide(DeckSide side) noexcept { side_.store(side, std::memory_order_relaxed); }
    DeckSide side() const noexcept { return side_.load(std::memory_order_relaxed); }
    const char *deckId() const noexcept { return side() == DeckSide::B ? "Deck B" : "Deck A"; }
    QString deckName() const { return QString::fromUtf8(deckId()); }

    bool isMaster() const noexcept { return is_master_.load(std::memory_order_relaxed); }
    void setMaster(bool m) noexcept { is_master_.store(m, std::memory_order_relaxed); }
    bool isSync() const noexcept { return is_sync_.load(std::memory_order_relaxed); }
    void setSync(bool s) noexcept { is_sync_.store(s, std::memory_order_relaxed); }
    bool isKeyLock() const noexcept { return key_lock_.load(std::memory_order_relaxed); }
    void setKeyLock(bool enable) noexcept { key_lock_.store(enable, std::memory_order_relaxed); }
    void syncToBpm(double target_bpm);
    void syncPhase(int64_t beat_time_ms);
    void syncPhaseTo(const DeckPlayer &other);
    void nudgePlayback(int64_t delta_ms) noexcept;

    double position() const;
    int64_t currentTimeMs() const;
    int64_t durationMs() const;
    bool isAtEnd() const noexcept;

    obs_source_t *source() const noexcept { return source_; }

private:
    void applyAudioVolume() noexcept;

    obs_source_t *source_ = nullptr;
    obs_source_t *eq_filter_ = nullptr;
    bool is_deck_a_ = true;

    mutable std::mutex file_mutex_;
    QString file_name_{};
    QString file_path_{};
    QString camelot_key_{QStringLiteral("--")};
    QString detected_key_{QStringLiteral("--")};

    std::atomic<bool> looping_{false};
    std::atomic<bool> has_video_{false};
    std::atomic<bool> file_changed_{false};
    std::atomic<DeckSide> side_{DeckSide::A};
    std::atomic<int> loop_beat_index_{EngineLoop::DEFAULT_LOOP_INDEX}; // 4 beats default (index 6 in 17-beat array)

    std::atomic<int64_t> loop_start_ms_{0};
    std::atomic<int64_t> loop_end_ms_{0};
    std::atomic<int64_t> cue_point_ms_{0};
    std::atomic<int64_t> hot_cues_[4]{-1, -1, -1, -1};
    std::atomic<int> volume_fx_mode_{0};
    std::atomic<bool> visual_eq_enabled_{true};
    std::atomic<bool> rgb_inverted_{false};

    std::atomic<double> base_bpm_{128.0};
    std::atomic<int64_t> grid_offset_ms_{0};
    std::atomic<bool> headphone_cue_{false};

    std::atomic<int> speed_percent_{100};
    std::atomic<float> gain_{1.0f};
    std::atomic<float> volume_{1.0f};
    std::atomic<float> mix_gain_{1.0f};

    std::atomic<float> red_{1.0f};
    std::atomic<float> green_{1.0f};
    std::atomic<float> blue_{1.0f};
    std::atomic<float> saturation_{1.0f};

    std::atomic<float> eq_hi_{1.0f};
    std::atomic<float> eq_mid_{1.0f};
    std::atomic<float> eq_low_{1.0f};
    std::atomic<float> filter_val_{0.0f};
    std::atomic<int> mixer_fx_type_{0};
    std::atomic<int> active_slot_{0};
    std::atomic<int> slot_fx_types_[4]{0, 1, 2, 3};

    std::atomic<bool> is_master_{false};
    std::atomic<bool> is_sync_{false};
    std::atomic<bool> key_lock_{false};
    std::atomic<bool> user_bpm_locked_{false};
    std::atomic<bool> user_grid_locked_{false};
    std::atomic<double> target_sync_bpm_{128.0};
    std::atomic<double> playback_bpm_{0.0};
    std::atomic<bool> pause_on_load_pending_{false};
    std::atomic<bool> user_playing_{false};
    std::atomic<int64_t> pending_speed_seek_ms_{0};
    std::atomic<int> speed_restart_phase_{0};
    std::chrono::steady_clock::time_point speed_restart_tp_{};
    std::atomic<bool> is_loop_seeking_{false};
    std::chrono::steady_clock::time_point last_loop_seek_tp_{};

    // RAM Audio Engine puffer és lejátszási mutató (Zero-stutter RAM playback buffer)
    mutable std::mutex audio_buf_mutex_;
    std::vector<float> audio_pcm_l_;
    std::vector<float> audio_pcm_r_;
    uint32_t audio_sample_rate_ = 44100;
    double playback_sample_pos_ = 0.0;
    double loop_fade_old_pos_ = 0.0;
    int loop_fade_rem_samples_ = 0;
    int loop_fade_total_samples_ = 0;
    std::atomic<int64_t> pending_loop_video_seek_ms_{-1};
    std::vector<float> sola_out_l_;
    std::vector<float> sola_out_r_;
    std::vector<float> sola_last_grain_l_;
    std::vector<float> sola_last_grain_r_;
    double sola_out_read_pos_ = 0.0;
    int64_t sola_prev_in_pos_ = 0;
    double sola_in_pos_ = 0.0;
    bool sola_initialized_ = false;

    EngineDspState dsp_states_[2];

    mutable std::mutex waveform_mutex_;
    std::vector<float> overview_samples_;
    std::vector<float> overview_low_;
    std::vector<float> overview_mid_;
    std::vector<float> overview_high_;
    std::vector<float> overview_tonus_;
    std::vector<float> detailed_samples_;
    std::vector<float> detailed_low_;
    std::vector<float> detailed_mid_;
    std::vector<float> detailed_high_;
    bool has_spectral_{false};

    // Live DSP Envelope Follower for instantaneous real-time audio reactivity
    std::atomic<float> live_amp_{0.0f};
    std::atomic<float> live_bass_{0.0f};
    std::atomic<float> live_mid_{0.0f};
    std::atomic<float> live_high_{0.0f};

    // VJ Shader Configuration & Parameter Modulation
    mutable std::mutex vj_config_mutex_;
    VjDeckConfig vj_config_ = getDefaultVjConfig(0);
    std::atomic<int> vj_shader_idx_prev_{0};
    std::atomic<float> vj_shader_trans_progress_{1.0f};
    std::atomic<float> vj_shader_trans_duration_{1.0f};
    std::atomic<int> vj_shader_trans_beats_{2};
};

using DeckEngine = DeckPlayer;

class DeckPair final {
public:
    DeckPair();
    ~DeckPair();

    DeckPair(const DeckPair &) = delete;
    DeckPair &operator=(const DeckPair &) = delete;

    DeckPlayer &deckA() noexcept;
    DeckPlayer &deckB() noexcept;

    obs_source_t *masterSource() const noexcept { return master_source_; }
    void setMasterSource(obs_source_t *s) noexcept { master_source_ = s; }
    obs_source_t *cueSource() const noexcept;
    void setCueSource(obs_source_t *s) noexcept;
    void setCueDeviceId(const std::string &id) noexcept;
    std::string cueDeviceId() const noexcept;

    void queueCueAudio(int deck_idx, const float *const *channels, size_t frames);
    void mixAndQueueCue(const float *a_l, const float *a_r, const float *b_l, const float *b_r, size_t frames);

    void setCrossfader(double value) noexcept;
    double crossfader() const noexcept;

    void setTransitionMode(int mode) noexcept;
    int transitionMode() const noexcept;

    void setCrossfadeFxMode(int mode) noexcept;
    int crossfadeFxMode() const noexcept { return crossfade_fx_mode_.load(std::memory_order_relaxed); }
    int crossfadeFxModePrev() const noexcept { return cf_fx_mode_prev_.load(std::memory_order_relaxed); }
    float crossfadeFxTransProgress() const noexcept { return cf_fx_trans_progress_.load(std::memory_order_relaxed); }
    void updateCrossfadeFxTransition(float dt_sec) noexcept;
    int crossfadeFxTransBeats() const noexcept { return cf_fx_trans_beats_.load(std::memory_order_relaxed); }
    void setCrossfadeFxTransBeats(int beats) noexcept { cf_fx_trans_beats_.store(std::clamp(beats, 0, 16), std::memory_order_relaxed); }

    float crossfadeFxIntensity() const noexcept { return cf_fx_intensity_.load(std::memory_order_relaxed); }
    void setCrossfadeFxIntensity(float val) noexcept;
    int crossfadeFxIntensityMod() const noexcept { return cf_fx_intensity_mod_.load(std::memory_order_relaxed); }
    void setCrossfadeFxIntensityMod(int m) noexcept;

    float crossfadeFxRate() const noexcept { return cf_fx_rate_.load(std::memory_order_relaxed); }
    void setCrossfadeFxRate(float val) noexcept;
    int crossfadeFxRateMod() const noexcept { return cf_fx_rate_mod_.load(std::memory_order_relaxed); }
    void setCrossfadeFxRateMod(int m) noexcept;

    float crossfadeFxBlendBoost() const noexcept { return cf_fx_blend_boost_.load(std::memory_order_relaxed); }
    void setCrossfadeFxBlendBoost(float val) noexcept;

    float crossfadeFxParamValue(int fx_mode, int param_idx) const noexcept;
    void setCrossfadeFxParamValue(int fx_mode, int param_idx, float val) noexcept;
    int crossfadeFxParamMod(int fx_mode, int param_idx) const noexcept;
    void setCrossfadeFxParamMod(int fx_mode, int param_idx, int mod) noexcept;
    float crossfadeFxParamModDepth(int fx_mode, int param_idx) const noexcept;
    void setCrossfadeFxParamModDepth(int fx_mode, int param_idx, float depth) noexcept;
    void resetCrossfadeFxParams(int fx_mode) noexcept;

    // Master FX Priority & Delayed Handover
    void notifyTrackLoaded(int deck_idx, const QString &file_path);
    void checkAndExecutePendingHandover() noexcept;
    void applyMasterFxConfig(const MasterFxTrackConfig &cfg) noexcept;
    void saveMasterFxToPriorityTrack() noexcept;
    MasterFxTrackConfig currentMasterFxConfig() const noexcept;
    int masterFxPriorityDeck() const noexcept { return master_fx_priority_deck_.load(std::memory_order_relaxed); }

    // Dual BPM-Synced LFO Engine
    LfoConfig lfo1Config() const noexcept;
    void setLfo1Config(const LfoConfig &cfg) noexcept;
    LfoConfig lfo2Config() const noexcept;
    void setLfo2Config(const LfoConfig &cfg) noexcept;
    float currentLfo1Value() const noexcept;
    float currentLfo2Value() const noexcept;

    void setObsOutputMode(int mode) noexcept;
    int obsOutputMode() const noexcept;

    enum class VideoScaleMode {
        Fit = 0,
        Fill = 1,
        Stretch = 2
    };
    void setVideoScaleMode(int mode) noexcept { video_scale_mode_.store(mode, std::memory_order_relaxed); }
    int videoScaleMode() const noexcept { return video_scale_mode_.load(std::memory_order_relaxed); }

    void setVisualizerMode(int m) noexcept { visualizer_mode_.store(m, std::memory_order_relaxed); }
    int visualizerMode() const noexcept { return visualizer_mode_.load(std::memory_order_relaxed); }

    void setVisualizerPulse(bool en) noexcept { visualizer_pulse_.store(en, std::memory_order_relaxed); }
    bool isVisualizerPulse() const noexcept { return visualizer_pulse_.load(std::memory_order_relaxed); }

    void setVisualizerTone(bool en) noexcept { visualizer_tone_.store(en, std::memory_order_relaxed); }
    bool isVisualizerTone() const noexcept { return visualizer_tone_.load(std::memory_order_relaxed); }

    void setHeadphoneVolume(float v) noexcept;
    float headphoneVolume() const noexcept;

    void setHeadphoneMix(float m) noexcept;
    float headphoneMix() const noexcept;

    enum MasterMode {
        MASTER_CLOCK = 0,
        MASTER_DECK_A = 1,
        MASTER_DECK_B = 2,
        MASTER_EXT_MIDI = 3
    };

    void setMasterMode(int mode) noexcept;
    int masterMode() const noexcept;

    void setMasterClockBpm(double bpm) noexcept;
    double masterClockBpm() const noexcept;
    double effectiveMasterBpm() const noexcept;

    void setSync(int deck_idx, bool sync_enabled);
    bool isSync(int deck_idx) const noexcept;
    void setDeckMaster(int deck_idx);
    void updateSync();

    void syncTempoAtoB();
    void syncTempoBtoA();

    void setLoopBeatIndex(int deck_index, int beat_index);
    int getLoopBeatIndex(int deck_index) const noexcept;

    void updateAudioMix() noexcept;
    void enableAudioMonitoring() noexcept;

    void setMixerFxType(int type) noexcept;
    int mixerFxType() const noexcept { return master_mixer_fx_type_.load(std::memory_order_relaxed); }

    int activeSlot() const noexcept;
    void selectSlot(int slot) noexcept;
    int slotFxType(int slot) const noexcept;
    void setSlotFxType(int slot, int fx_type) noexcept;

private:
    obs_source_t *master_source_ = nullptr;
    mutable std::mutex cue_source_mutex_;
    obs_source_t *cue_source_ = nullptr;
    mutable std::mutex cue_dev_mutex_;
    std::string cue_device_id_{"default"};
    std::atomic<bool> cue_device_changed_{false};

    std::atomic<double> crossfader_{0.5};
    std::atomic<int> transition_mode_{0};
    std::atomic<int> crossfade_fx_mode_{0};
    std::atomic<int> cf_fx_mode_prev_{0};
    std::atomic<float> cf_fx_trans_progress_{1.0f};
    std::atomic<float> cf_fx_trans_duration_{1.0f};
    std::atomic<int> cf_fx_trans_beats_{2};
    std::atomic<float> cf_fx_intensity_{1.0f};
    std::atomic<int> cf_fx_intensity_mod_{0};
    std::atomic<float> cf_fx_rate_{1.0f};
    std::atomic<int> cf_fx_rate_mod_{0};
    std::atomic<float> cf_fx_blend_boost_{1.0f};
    std::atomic<float> cf_fx_params_val_[16][8];
    std::atomic<int> cf_fx_params_mod_[16][8];
    std::atomic<float> cf_fx_params_depth_[16][8];
    std::atomic<int> master_fx_priority_deck_{-1};
    std::atomic<bool> pending_fx_handover_ready_{false};
    std::atomic<bool> is_applying_cached_fx_{false};
    std::atomic<uint64_t> track_loaded_ns_a_{0};
    std::atomic<uint64_t> track_loaded_ns_b_{0};
    MasterFxTrackConfig cached_fx_config_a_{};
    MasterFxTrackConfig cached_fx_config_b_{};
    mutable std::mutex fx_cache_mutex_;
    mutable std::mutex lfo_mutex_;
    LfoConfig lfo1_config_{0, 6, 1.0f, 0.0f, 0.0f};
    LfoConfig lfo2_config_{1, 4, 1.0f, 0.0f, 90.0f};
    std::atomic<int> obs_output_mode_{0};
    std::atomic<int> video_scale_mode_{1};
    std::atomic<int> visualizer_mode_{0};
    std::atomic<bool> visualizer_pulse_{true};
    std::atomic<bool> visualizer_tone_{true};
    std::atomic<float> headphone_volume_{1.0f};
    std::atomic<float> headphone_mix_{0.5f};
    std::atomic<int> master_mixer_fx_type_{0};
    std::atomic<int> active_slot_{0};
    std::atomic<int> slot_fx_types_[4]{0, 1, 2, 3};
    std::atomic<int> master_mode_{0};
    std::atomic<double> master_clock_bpm_{128.0};
    std::atomic<int> loop_beat_idx_a_{EngineLoop::DEFAULT_LOOP_INDEX};
    std::atomic<int> loop_beat_idx_b_{EngineLoop::DEFAULT_LOOP_INDEX};

    struct CuePacket {
        std::vector<float> l;
        std::vector<float> r;
        size_t frames = 0;
    };
    std::mutex cue_mutex_;
    std::condition_variable cue_cv_;
    std::queue<CuePacket> cue_queue_;
    std::thread cue_thread_;
    std::atomic<bool> cue_thread_running_{false};

    void cueWorkerLoop();
};

// Global deck and mixer accessors
DeckPlayer *vj_active_deck();
DeckPlayer *vj_deck_a();
DeckPlayer *vj_deck_b();
void vdj_set_active_deck(DeckPlayer *deck);
void vdj_set_deck_a(DeckPlayer *deck);
void vdj_set_deck_b(DeckPlayer *deck);
extern "C" obs_source_t *vdj_get_obs_source_deck(int channel);

DeckPair *vdj_decks() noexcept;
void vdj_set_decks(DeckPair *decks) noexcept;
