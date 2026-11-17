#include "audio/audio_analyzer.hpp"
#include "track/track_cache.hpp"
#include "sources/wmf_decoder.hpp"
#include "analyzer/analyzer_waveform.hpp"
#include "analyzer/analyzer_beats.hpp"
#include "analyzer/analyzer_silence.hpp"
#include "analyzer/analyzer_key.hpp"
#include "analyzer/analyzer_gain.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <thread>
#include <QFile>

QString AudioAnalyzer::cacheFilePath(const QString &file_path)
{
    return TrackCache::filePath(file_path);
}

bool AudioAnalyzer::loadCache(const QString &file_path, TrackAnalysisResult &out_res)
{
    return TrackCache::load(file_path, out_res);
}

bool AudioAnalyzer::saveCache(const QString &file_path, const TrackAnalysisResult &res)
{
    return TrackCache::save(file_path, res);
}

bool AudioAnalyzer::updateCacheUserEdits(const QString &file_path, double bpm, int64_t grid_offset_ms, int64_t cue_point_ms)
{
    return TrackCache::updateUserEdits(file_path, bpm, grid_offset_ms, cue_point_ms);
}

bool AudioAnalyzer::updateCacheHotCues(const QString &file_path, const int64_t hot_cues[4])
{
    return TrackCache::updateHotCues(file_path, hot_cues);
}

bool AudioAnalyzer::updateCacheVjConfig(const QString &file_path, const VjDeckConfig &config)
{
    return TrackCache::updateVjConfig(file_path, config);
}

TrackAnalysisResult AudioAnalyzer::analyzeFile(const QString &file_path, bool force_reanalyze)
{
    TrackAnalysisResult result;
    result.detected_bpm = 128.0;
    result.grid_offset_ms = 0;
    result.cue_point_ms = 0;
    result.duration_ms = 0;
    result.audio_sample_rate = 44100;
    result.success = false;

    // 1. Gyorsítótár ellenőrzése (Check existing sidecar .vdj.json cache)
    if (!force_reanalyze) {
        if (loadCache(file_path, result)) {
            // Csak akkor fogadjuk el késznek, ha az új hangnem (Camelot Key) már ki lett elemezve
            const bool has_key = !result.camelot_key.isEmpty() && result.camelot_key != QStringLiteral("--");
            if (has_key) {
                // Gyorsítótár megléte esetén is betöltjük a sztereó RAM audio puffert
                extractAudioOnly(file_path, result.audio_pcm_l, result.audio_pcm_r, result.audio_sample_rate);
                return result;
            }
        }
    }

    // 2. Média dekódolása PCM lebegőpontos mintákká
    std::vector<float> mono_samples;
    const bool ok = WmfDecoder::decodePcm(file_path,
                                          result.audio_pcm_l,
                                          result.audio_pcm_r,
                                          &mono_samples,
                                          result.audio_sample_rate,
                                          result.duration_ms);
    if (!ok || mono_samples.empty()) {
        return result;
    }

    // 3. Spektrális szűrés és hullámforma generálás (Waveform & Spectral Analysis)
    auto wave_res = AnalyzerWaveform::generate(mono_samples, result.audio_sample_rate);
    result.overview_peaks = std::move(wave_res.overview_peaks);
    result.overview_low   = std::move(wave_res.overview_low);
    result.overview_mid   = std::move(wave_res.overview_mid);
    result.overview_high  = std::move(wave_res.overview_high);
    result.overview_tonus = std::move(wave_res.overview_tonus);
    result.detailed_peaks = std::move(wave_res.detailed_peaks);
    result.detailed_low   = std::move(wave_res.detailed_low);
    result.detailed_mid   = std::move(wave_res.detailed_mid);
    result.detailed_high  = std::move(wave_res.detailed_high);
    result.waveform_peaks = result.overview_peaks;

    // 4. BPM detektálás, autokorreláció és Downbeat ütemrács igazítás (Beat Analysis)
    auto beat_res = AnalyzerBeats::analyze(wave_res.kick_samples, mono_samples.size(), result.audio_sample_rate);
    result.detected_bpm = beat_res.bpm;
    result.grid_offset_ms = beat_res.grid_offset_ms;

    // 5. Csend-detektálás & Auto-CUE pont (-60 dBFS küszöb)
    auto silence_res = AnalyzerSilence::analyze(mono_samples, result.audio_sample_rate, -60.0f);
    if (silence_res.has_sound && silence_res.first_sound_ms > 0) {
        result.cue_point_ms = silence_res.first_sound_ms;
    } else {
        result.cue_point_ms = 0;
    }

    // 6. Zenei Hangnem & Camelot Kerék Analízis (Musical Key & Camelot)
    auto key_res = AnalyzerKey::analyze(mono_samples, result.audio_sample_rate);
    if (key_res.success) {
        result.detected_key = key_res.key_name;
        result.camelot_key = key_res.camelot_code;
    }

    // 7. EBU R128 Hangerő Normalizálás & ReplayGain Analízis
    auto gain_res = AnalyzerGain::analyze(mono_samples, result.audio_sample_rate);
    if (gain_res.success) {
        result.replay_gain_db = gain_res.replay_gain_db;
        result.peak_amplitude = gain_res.peak_amplitude;
    }

    result.success = (result.detected_bpm > 30.0) && !result.overview_peaks.empty();

    // 8. Eredmény mentése .vdj.json gyorsítótárba
    saveCache(file_path, result);
    return result;
}

void AudioAnalyzer::analyzeFileAsync(const QString &file_path,
                                     std::function<void(TrackAnalysisResult)> callback,
                                     bool force_reanalyze)
{
    std::thread([file_path, callback, force_reanalyze]() {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        TrackAnalysisResult res = analyzeFile(file_path, force_reanalyze);
        if (callback)
            callback(std::move(res));
    }).detach();
}

bool AudioAnalyzer::extractAudioOnly(const QString &file_path,
                                     std::vector<float> &out_l,
                                     std::vector<float> &out_r,
                                     uint32_t &out_sr)
{
    int64_t dur = 0;
    return WmfDecoder::decodePcm(file_path, out_l, out_r, nullptr, out_sr, dur);
}

void AudioAnalyzer::extractAudioOnlyAsync(const QString &file_path,
                                         std::function<void(std::vector<float>, std::vector<float>, uint32_t)> callback)
{
    std::thread([file_path, callback]() {
        std::vector<float> l, r;
        uint32_t sr = 44100;
        const bool ok = extractAudioOnly(file_path, l, r, sr);
        if (ok && callback) {
            callback(std::move(l), std::move(r), sr);
        }
    }).detach();
}
