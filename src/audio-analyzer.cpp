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

namespace {
constexpr size_t kOverviewBuckets = 500;
}

TrackAnalysisResult AudioAnalyzer::analyzeFile(const QString &file_path)
{
    TrackAnalysisResult result;
    result.detected_bpm = 128.0;
    result.grid_offset_ms = 0;
    result.duration_ms = 0;
    result.success = false;

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

    // Try to get track total duration
    PROPVARIANT var;
    PropVariantInit(&var);
    if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,
                                                    MF_PD_DURATION, &var))) {
        result.duration_ms = var.uhVal.QuadPart / 10000; // 100-nanosecond units to ms
        PropVariantClear(&var);
    }

    // Deselect all streams and select first audio stream
    pReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    pReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    // Request uncompressed 16-bit PCM output.
    // Try standard 44100 Hz mono first:
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

    // If 44100 mono was rejected by the demuxer, fallback to generic uncompressed PCM:
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

    // Read audio PCM samples (up to 240 seconds):
    std::vector<float> mono_samples;
    mono_samples.reserve(sample_rate * 120);

    while (mono_samples.size() < static_cast<size_t>(sample_rate * 240)) {
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

    // 1. Overview Waveform (~500 points for full track strip)
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

    // 3. High-Resolution Detailed Waveform (100 points/sec, 10ms window)
    // Normalized to global maximums with non-linear transient punch:
    // Kicks punch sharply to 0.90 - 1.0, while valleys between beats drop down to 0.15 - 0.25!
    const size_t samples_per_10ms = std::max<size_t>(1, static_cast<size_t>(sample_rate / 100));
    const size_t num_detailed = total_samples / samples_per_10ms;
    result.detailed_peaks.resize(num_detailed, 0.05f);

    for (size_t w = 0; w < num_detailed; ++w) {
        const size_t start = w * samples_per_10ms;
        const size_t end = std::min(start + samples_per_10ms, total_samples);

        float peak_kick = 0.0f;
        float peak_full = 0.0f;
        for (size_t i = start; i < end; ++i) {
            peak_kick = std::max(peak_kick, std::abs(kick_samples[i]));
            peak_full = std::max(peak_full, std::abs(mono_samples[i]));
        }

        const float norm_k = peak_kick / max_kick_all;
        const float norm_f = peak_full / max_full_all;

        // Kick transients punch sharply using power law to eliminate flat block saturation
        const float env = 0.18f * norm_f + 0.82f * std::pow(norm_k, 1.8f);
        result.detailed_peaks[w] = std::clamp(env, 0.06f, 1.0f);
    }

    // 4. Onset Novelty Curve at 500 Hz (2 ms windows) for ultra-accurate BPM autocorrelation
    const size_t samples_per_2ms = std::max<size_t>(1, static_cast<size_t>(sample_rate / 500));
    const size_t num_onset_windows = total_samples / samples_per_2ms;
    if (num_onset_windows < 500)
        return result;

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

    // 5. Autocorrelation over 65 to 185 BPM at 500 Hz
    constexpr int kMinLag = 162; // ~185 BPM
    constexpr int kMaxLag = 461; // ~65 BPM

    std::vector<double> autocorr(kMaxLag + 2, 0.0);
    const size_t max_novelty_len = std::min<size_t>(novelty.size(), static_cast<size_t>(500 * 90)); // 90 seconds analysis

    int best_lag = 234; // 128 BPM
    double best_score = -1.0;

    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        double sum = 0.0;
        const size_t count = max_novelty_len - lag;
        for (size_t i = 0; i < count; ++i) {
            sum += static_cast<double>(novelty[i]) * static_cast<double>(novelty[i + lag]);
        }

        const double bpm = (60.0 * 500.0) / static_cast<double>(lag);
        // Gaussian tempo prior centered at 128 BPM (sigma 38)
        const double prior = std::exp(-std::pow(bpm - 128.0, 2.0) / (2.0 * 38.0 * 38.0));
        const double score = sum * prior;
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

    const double detected_bpm = (60.0 * 500.0) / fine_lag;
    result.detected_bpm = std::round(detected_bpm * 10.0) / 10.0;

    // 7. Downbeat (Kick 1) Anchor Alignment
    const int period = std::max(1, static_cast<int>(std::round(fine_lag)));
    const size_t max_search_windows = std::min<size_t>(novelty.size() - period, static_cast<size_t>(500 * 25));

    double best_offset_score = -1.0;
    int best_offset = 0;

    for (int o = 0; o < period && o < static_cast<int>(max_search_windows); ++o) {
        double alignment = 0.0;
        int beats = 0;
        for (size_t idx = o; idx + period < max_search_windows; idx += period) {
            alignment += novelty[idx];
            beats++;
        }
        if (beats > 0)
            alignment /= beats;

        if (alignment > best_offset_score) {
            best_offset_score = alignment;
            best_offset = o;
        }
    }

    // Convert offset windows to milliseconds (each window is 2ms at 500 Hz)
    result.grid_offset_ms = static_cast<int64_t>(best_offset * 2);
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
