#include "audio-analyzer.hpp"

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")
#endif

#include <thread>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {
constexpr size_t kOverviewBuckets = 500;
}

QString AudioAnalyzer::cacheFilePath(const QString &file_path)
{
    return file_path + QStringLiteral(".vdj.json");
}

bool AudioAnalyzer::loadCache(const QString &file_path, TrackAnalysisResult &out_res)
{
    const QString cache_path = cacheFilePath(file_path);
    QFile file(cache_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    out_res.detected_bpm = root.value(QStringLiteral("bpm")).toDouble(128.0);
    out_res.grid_offset_ms = static_cast<int64_t>(root.value(QStringLiteral("grid_offset_ms")).toVariant().toLongLong());
    out_res.cue_point_ms = static_cast<int64_t>(root.value(QStringLiteral("cue_point_ms")).toVariant().toLongLong());
    out_res.duration_ms = static_cast<int64_t>(root.value(QStringLiteral("duration_ms")).toVariant().toLongLong());

    const QJsonArray ov_arr = root.value(QStringLiteral("overview_peaks")).toArray();
    out_res.overview_peaks.clear();
    out_res.overview_peaks.reserve(ov_arr.size());
    for (const auto &val : ov_arr) {
        out_res.overview_peaks.push_back(static_cast<float>(val.toDouble(0.1)));
    }

    const QJsonArray det_arr = root.value(QStringLiteral("detailed_peaks")).toArray();
    out_res.detailed_peaks.clear();
    out_res.detailed_peaks.reserve(det_arr.size());
    for (const auto &val : det_arr) {
        out_res.detailed_peaks.push_back(static_cast<float>(val.toDouble(0.1)));
    }

    out_res.waveform_peaks = out_res.overview_peaks;
    out_res.success = !out_res.overview_peaks.empty() && (out_res.detected_bpm > 30.0);
    return out_res.success;
}

bool AudioAnalyzer::saveCache(const QString &file_path, const TrackAnalysisResult &res)
{
    if (!res.success)
        return false;

    const QString cache_path = cacheFilePath(file_path);
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("bpm"), res.detected_bpm);
    root.insert(QStringLiteral("grid_offset_ms"), static_cast<qint64>(res.grid_offset_ms));
    root.insert(QStringLiteral("cue_point_ms"), static_cast<qint64>(res.cue_point_ms));
    root.insert(QStringLiteral("duration_ms"), static_cast<qint64>(res.duration_ms));

    QJsonArray ov_arr;
    for (float v : res.overview_peaks) {
        ov_arr.append(std::round(v * 1000.0) / 1000.0);
    }
    root.insert(QStringLiteral("overview_peaks"), ov_arr);

    QJsonArray det_arr;
    for (float v : res.detailed_peaks) {
        det_arr.append(std::round(v * 1000.0) / 1000.0);
    }
    root.insert(QStringLiteral("detailed_peaks"), det_arr);

    QFile file(cache_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

bool AudioAnalyzer::updateCacheUserEdits(const QString &file_path, double bpm, int64_t grid_offset_ms, int64_t cue_point_ms)
{
    const QString cache_path = cacheFilePath(file_path);
    QFile file(cache_path);
    QJsonObject root;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    if (bpm > 30.0)
        root.insert(QStringLiteral("bpm"), bpm);
    if (grid_offset_ms >= 0)
        root.insert(QStringLiteral("grid_offset_ms"), static_cast<qint64>(grid_offset_ms));
    if (cue_point_ms >= 0)
        root.insert(QStringLiteral("cue_point_ms"), static_cast<qint64>(cue_point_ms));

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
        return true;
    }
    return false;
}

TrackAnalysisResult AudioAnalyzer::analyzeFile(const QString &file_path)
{
    TrackAnalysisResult result;
    result.detected_bpm = 128.0;
    result.grid_offset_ms = 0;
    result.cue_point_ms = 0;
    result.duration_ms = 0;
    result.success = false;

    // 1. Instant check for existing .vdj.json sidecar cache file
    if (loadCache(file_path, result)) {
        return result;
    }

#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool co_initialized = SUCCEEDED(hr);

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        if (co_initialized)
            CoUninitialize();
        return result;
    }

    IMFAttributes *pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) {
        pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
    }

    IMFSourceReader *pReader = nullptr;
    const std::wstring wide_path = file_path.toStdWString();
    hr = MFCreateSourceReaderFromURL(wide_path.c_str(), pAttributes, &pReader);
    if (pAttributes)
        pAttributes->Release();

    if (FAILED(hr) || !pReader) {
        MFShutdown();
        if (co_initialized)
            CoUninitialize();
        return result;
    }

    // Read track total duration
    PROPVARIANT var;
    PropVariantInit(&var);
    if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,
                                                    MF_PD_DURATION, &var))) {
        result.duration_ms = var.uhVal.QuadPart / 10000; // 100-nanosecond units to ms
        PropVariantClear(&var);
    }

    // Select first audio stream
    pReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    pReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    // Request uncompressed 16-bit PCM output at 44100 Hz mono
    IMFMediaType *pPartialType = nullptr;
    hr = MFCreateMediaType(&pPartialType);
    if (SUCCEEDED(hr)) {
        pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        pPartialType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
        pPartialType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
        pPartialType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
        pPartialType->Release();
    }

    // Fallback to generic uncompressed PCM if 44100 mono was rejected
    if (FAILED(hr)) {
        hr = MFCreateMediaType(&pPartialType);
        if (SUCCEEDED(hr)) {
            pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
            pPartialType->Release();
        }
    }

    if (FAILED(hr)) {
        pReader->Release();
        MFShutdown();
        if (co_initialized)
            CoUninitialize();
        return result;
    }

    // Query actual decoded audio format:
    UINT32 channels = 1;
    UINT32 sample_rate = 44100;
    UINT32 bits_per_sample = 16;
    IMFMediaType *pActualType = nullptr;
    hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualType);
    if (SUCCEEDED(hr) && pActualType) {
        pActualType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        pActualType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate);
        pActualType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
        pActualType->Release();
    }
    if (channels == 0) channels = 1;
    if (sample_rate == 0) sample_rate = 44100;
    if (bits_per_sample == 0) bits_per_sample = 16;

    // Read full audio PCM samples (up to 12 minutes of audio)
    std::vector<float> mono_samples;
    mono_samples.reserve(sample_rate * 240);

    const size_t max_total_samples = static_cast<size_t>(sample_rate * 720); // 12 minutes limit
    while (mono_samples.size() < max_total_samples) {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG llTimestamp = 0;
        IMFSample *pSample = nullptr;

        hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                                 &streamIndex, &flags, &llTimestamp, &pSample);

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (pSample)
                pSample->Release();
            break;
        }

        if (pSample) {
            IMFMediaBuffer *pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            if (SUCCEEDED(hr) && pBuffer) {
                BYTE *pAudioData = nullptr;
                DWORD cbCurrentLength = 0;
                hr = pBuffer->Lock(&pAudioData, nullptr, &cbCurrentLength);
                if (SUCCEEDED(hr) && pAudioData && cbCurrentLength >= sizeof(int16_t)) {
                    if (bits_per_sample == 16) {
                        const auto *pcm = reinterpret_cast<const int16_t *>(pAudioData);
                        const size_t num_frames = cbCurrentLength / (sizeof(int16_t) * channels);
                        for (size_t f = 0; f < num_frames; ++f) {
                            float sum = 0.0f;
                            for (size_t c = 0; c < channels; ++c) {
                                sum += static_cast<float>(pcm[f * channels + c]) / 32768.0f;
                            }
                            mono_samples.push_back(sum / static_cast<float>(channels));
                        }
                    } else if (bits_per_sample == 32) {
                        const auto *flt = reinterpret_cast<const float *>(pAudioData);
                        const size_t num_frames = cbCurrentLength / (sizeof(float) * channels);
                        for (size_t f = 0; f < num_frames; ++f) {
                            float sum = 0.0f;
                            for (size_t c = 0; c < channels; ++c) {
                                sum += flt[f * channels + c];
                            }
                            mono_samples.push_back(sum / static_cast<float>(channels));
                        }
                    }
                    pBuffer->Unlock();
                }
                pBuffer->Release();
            }
            pSample->Release();
        }
    }

    pReader->Release();
    MFShutdown();
    if (co_initialized)
        CoUninitialize();

    if (mono_samples.empty())
        return result;

    result.success = true;
    const size_t total_samples = mono_samples.size();
    if (result.duration_ms <= 0 && sample_rate > 0) {
        result.duration_ms = static_cast<int64_t>((static_cast<double>(total_samples) / static_cast<double>(sample_rate)) * 1000.0);
    }

    // 1. Overview Waveform (500 points for full track strip)
    result.overview_peaks.resize(kOverviewBuckets, 0.05f);
    const size_t samples_per_bucket = std::max<size_t>(1, total_samples / kOverviewBuckets);
    for (size_t b = 0; b < kOverviewBuckets; ++b) {
        const size_t start = b * samples_per_bucket;
        const size_t end = std::min(start + samples_per_bucket, total_samples);

        float max_val = 0.0f;
        for (size_t i = start; i < end; ++i) {
            max_val = std::max(max_val, std::abs(mono_samples[i]));
        }
        result.overview_peaks[b] = std::clamp(max_val, 0.05f, 1.0f);
    }
    result.waveform_peaks = result.overview_peaks;

    // 2. Low-Pass Filter (160 Hz) on mono samples to isolate kick drum punch
    const float fc = 160.0f;
    const float alpha = std::clamp(2.0f * 3.14159f * fc / static_cast<float>(sample_rate), 0.001f, 0.99f);
    float lp1 = 0.0f;
    float lp2 = 0.0f;
    std::vector<float> kick_samples(total_samples, 0.0f);
    float max_kick_all = 0.001f;
    float max_full_all = 0.001f;

    for (size_t i = 0; i < total_samples; ++i) {
        const float in = mono_samples[i];
        lp1 += alpha * (in - lp1);
        lp2 += alpha * (lp1 - lp2);
        kick_samples[i] = lp2;

        const float abs_k = std::abs(lp2);
        const float abs_f = std::abs(in);
        if (abs_k > max_kick_all) max_kick_all = abs_k;
        if (abs_f > max_full_all) max_full_all = abs_f;
    }

    // 3. High-Resolution Detailed Waveform (50 points/sec, 20ms window)
    const size_t samples_per_20ms = std::max<size_t>(1, static_cast<size_t>(sample_rate / 50));
    const size_t num_detailed = total_samples / samples_per_20ms;
    result.detailed_peaks.resize(num_detailed, 0.05f);

    for (size_t w = 0; w < num_detailed; ++w) {
        const size_t start = w * samples_per_20ms;
        const size_t end = std::min(start + samples_per_20ms, total_samples);

        float peak_kick = 0.0f;
        float peak_full = 0.0f;
        for (size_t i = start; i < end; ++i) {
            peak_kick = std::max(peak_kick, std::abs(kick_samples[i]));
            peak_full = std::max(peak_full, std::abs(mono_samples[i]));
        }

        const float norm_k = peak_kick / max_kick_all;
        const float norm_f = peak_full / max_full_all;
        const float env = 0.20f * norm_f + 0.80f * std::pow(norm_k, 1.8f);
        result.detailed_peaks[w] = std::clamp(env, 0.06f, 1.0f);
    }

    // 4. Onset Novelty Curve at ~500 Hz (exact onset sample rate) for BPM autocorrelation
    const size_t samples_per_2ms = std::max<size_t>(1, static_cast<size_t>(sample_rate / 500));
    const double onset_rate = static_cast<double>(sample_rate) / static_cast<double>(samples_per_2ms);
    const size_t num_onset_windows = total_samples / samples_per_2ms;
    if (num_onset_windows < 500) {
        saveCache(file_path, result);
        return result;
    }

    std::vector<float> kick_energy(num_onset_windows, 0.0f);
    for (size_t w = 0; w < num_onset_windows; ++w) {
        const size_t start = w * samples_per_2ms;
        const size_t end = std::min(start + samples_per_2ms, total_samples);
        double sum_sq = 0.0;
        for (size_t i = start; i < end; ++i) {
            const float val = kick_samples[i];
            sum_sq += val * val;
        }
        const float count = static_cast<float>(end - start);
        kick_energy[w] = (count > 0.0f) ? static_cast<float>(std::sqrt(sum_sq / count)) : 0.0f;
    }

    std::vector<float> novelty(num_onset_windows, 0.0f);
    for (size_t w = 1; w < num_onset_windows; ++w) {
        const float diff = kick_energy[w] - kick_energy[w - 1];
        novelty[w] = (diff > 0.0f) ? diff : 0.0f;
    }

    // 5. Autocorrelation over 65 to 190 BPM with Harmonic Enhancement
    constexpr int kMinLag = 158; // ~190 BPM
    constexpr int kMaxLag = 465; // ~65 BPM
    const int max_search_lag = kMaxLag * 2 + 4;

    std::vector<double> raw_corr(max_search_lag + 1, 0.0);
    const size_t max_novelty_len = std::min<size_t>(novelty.size(), static_cast<size_t>(500 * 120)); // analyze up to 2 minutes

    for (int lag = kMinLag / 2; lag <= max_search_lag; ++lag) {
        if (static_cast<size_t>(lag) >= max_novelty_len) break;
        double sum = 0.0;
        const size_t count = max_novelty_len - lag;
        for (size_t i = 0; i < count; ++i) {
            sum += static_cast<double>(novelty[i]) * static_cast<double>(novelty[i + lag]);
        }
        raw_corr[lag] = (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
    }

    std::vector<double> autocorr(kMaxLag + 2, 0.0);
    int best_lag = 234; // 128 BPM
    double best_score = -1.0;

    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        // Harmonic summation reinforces true fundamental beat and suppresses 5/4, 3/4 cross-rhythms
        double score = raw_corr[lag];
        if (lag * 2 <= max_search_lag) score += 0.6 * raw_corr[lag * 2];
        if (lag / 2 >= kMinLag / 2)   score += 0.4 * raw_corr[lag / 2];
        if (lag * 3 <= max_search_lag) score += 0.25 * raw_corr[lag * 3];

        const double bpm = (60.0 * onset_rate) / static_cast<double>(lag);
        // Wide DJ prior centered at 128 BPM (sigma 40)
        const double prior = std::exp(-std::pow(bpm - 128.0, 2.0) / (2.0 * 40.0 * 40.0));
        score *= prior;
        autocorr[lag] = score;

        if (score > best_score) {
            best_score = score;
            best_lag = lag;
        }
    }

    // 6. Sub-sample Parabolic Peak Interpolation for 0.05 BPM accuracy
    double fine_lag = static_cast<double>(best_lag);
    if (best_lag > kMinLag && best_lag < kMaxLag) {
        const double y0 = autocorr[best_lag - 1];
        const double y1 = autocorr[best_lag];
        const double y2 = autocorr[best_lag + 1];
        const double denom = 2.0 * (y0 - 2.0 * y1 + y2);
        if (std::abs(denom) > 1e-6) {
            const double delta = (y0 - y2) / denom;
            if (std::abs(delta) < 1.0)
                fine_lag += delta;
        }
    }

    const double detected_bpm = (60.0 * onset_rate) / fine_lag;
    double final_bpm = detected_bpm;
    const double nearest_int = std::round(detected_bpm);
    if (std::abs(detected_bpm - nearest_int) <= 0.42) {
        final_bpm = nearest_int;
    } else {
        const double nearest_half = std::round(detected_bpm * 2.0) / 2.0;
        if (std::abs(detected_bpm - nearest_half) <= 0.12) {
            final_bpm = nearest_half;
        } else {
            final_bpm = std::round(detected_bpm * 10.0) / 10.0;
        }
    }
    result.detected_bpm = final_bpm;

    // 7. Find highest energy drop / kick section for Downbeat (Kick 1) Anchor Alignment
    const int period = std::max(1, static_cast<int>(std::round(fine_lag)));
    
    // Find window with highest kick density across the track
    size_t best_window_start = 0;
    float max_density = -1.0f;
    const size_t search_span = std::min<size_t>(novelty.size(), static_cast<size_t>(500 * 90));
    const size_t chunk_size = 500 * 16; // 16 second window
    for (size_t s = 0; s + chunk_size < search_span; s += (500 * 4)) {
        float chunk_energy = 0.0f;
        for (size_t i = s; i < s + chunk_size; ++i) {
            chunk_energy += novelty[i];
        }
        if (chunk_energy > max_density) {
            max_density = chunk_energy;
            best_window_start = s;
        }
    }

    double best_offset_score = -1.0;
    int best_offset = 0;
    const size_t align_end = std::min(best_window_start + chunk_size, novelty.size() - period);

    for (int o = 0; o < period; ++o) {
        double alignment = 0.0;
        int beats = 0;
        for (size_t idx = best_window_start + o; idx + period < align_end; idx += period) {
            alignment += novelty[idx];
            beats++;
        }
        if (beats > 0)
            alignment /= beats;

        if (alignment > best_offset_score) {
            best_offset_score = alignment;
            best_offset = (best_window_start + o) % period;
        }
    }

    // Convert offset windows to milliseconds (each window is 2ms at 500 Hz)
    result.grid_offset_ms = static_cast<int64_t>(best_offset * 2);
    result.success = (result.detected_bpm > 30.0) && !result.overview_peaks.empty();

    // Save newly analyzed results to .vdj.json sidecar file
    saveCache(file_path, result);
#endif

    return result;
}

void AudioAnalyzer::analyzeFileAsync(const QString &file_path,
                                     std::function<void(TrackAnalysisResult)> callback)
{
    std::thread([file_path, callback]() {
        TrackAnalysisResult res = analyzeFile(file_path);
        if (callback)
            callback(res);
    }).detach();
}
