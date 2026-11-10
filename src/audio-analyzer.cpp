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
#include <QFileInfo>

namespace {
constexpr uint32_t kSampleRate = 22050; // 22.05 kHz mono PCM
constexpr size_t kMaxAnalysisSeconds = 240; // Analyze first 4 minutes
constexpr size_t kMaxSamples = kSampleRate * kMaxAnalysisSeconds;
constexpr size_t kOverviewBuckets = 500;
constexpr size_t kOnsetWindowSize = 44; // ~2 ms (501.1 windows/sec) for 0.05 BPM accuracy
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

    // Configure uncompressed 22050 Hz 16-bit mono PCM output
    IMFMediaType *pPartialType = nullptr;
    hr = MFCreateMediaType(&pPartialType);
    if (SUCCEEDED(hr)) {
        pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        pPartialType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
        pPartialType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kSampleRate);
        pPartialType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
        pPartialType->Release();
    }

    if (FAILED(hr)) {
        pReader->Release();
        MFShutdown();
        if (co_initialized)
            CoUninitialize();
        return result;
    }

    // Read audio PCM samples
    std::vector<int16_t> audio_samples;
    audio_samples.reserve(kSampleRate * 120);

    while (audio_samples.size() < kMaxSamples) {
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
                if (SUCCEEDED(hr) && pAudioData && cbCurrentLength >= 2) {
                    const auto *pcm = reinterpret_cast<const int16_t *>(pAudioData);
                    const size_t num_samples = cbCurrentLength / sizeof(int16_t);
                    const size_t needed = std::min(num_samples, kMaxSamples - audio_samples.size());
                    audio_samples.insert(audio_samples.end(), pcm, pcm + needed);
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

    if (audio_samples.empty())
        return result;

    result.success = true;
    const size_t total_samples = audio_samples.size();

    // 1. Overview Waveform (whole track progress bar, ~500 points)
    result.overview_peaks.resize(kOverviewBuckets, 0.05f);
    const size_t samples_per_bucket = std::max<size_t>(1, total_samples / kOverviewBuckets);
    for (size_t b = 0; b < kOverviewBuckets; ++b) {
        const size_t start = b * samples_per_bucket;
        const size_t end = std::min(start + samples_per_bucket, total_samples);

        int32_t max_val = 0;
        for (size_t i = start; i < end; ++i) {
            max_val = std::max(max_val, static_cast<int32_t>(std::abs(audio_samples[i])));
        }
        result.overview_peaks[b] = std::clamp(static_cast<float>(max_val) / 32768.0f, 0.05f, 1.0f);
    }
    result.waveform_peaks = result.overview_peaks;

    // 2. Low-Pass Filter (180 Hz) to isolate kick drum & bass transients from vocals/synths
    const float fc = 180.0f;
    const float alpha = std::clamp(2.0f * 3.14159f * fc / static_cast<float>(kSampleRate), 0.001f, 0.99f);
    float lp1 = 0.0f;
    float lp2 = 0.0f;
    std::vector<float> kick_samples(total_samples, 0.0f);
    for (size_t i = 0; i < total_samples; ++i) {
        const float in = static_cast<float>(audio_samples[i]) / 32768.0f;
        lp1 += alpha * (in - lp1);
        lp2 += alpha * (lp1 - lp2);
        kick_samples[i] = lp2;
    }

    // 3. High-Resolution Detailed Waveform (100 points/sec, 10ms window = 220 samples)
    // Dynamic envelope combines kick transients and overall dynamics so kicks visibly pop out!
    constexpr size_t kDetailedWindow = 220; // 10 ms at 22.05 kHz
    const size_t num_detailed = total_samples / kDetailedWindow;
    result.detailed_peaks.resize(num_detailed, 0.05f);

    for (size_t w = 0; w < num_detailed; ++w) {
        const size_t start = w * kDetailedWindow;
        double sum_sq_kick = 0.0;
        double sum_sq_full = 0.0;

        for (size_t i = 0; i < kDetailedWindow; ++i) {
            const float k = kick_samples[start + i];
            sum_sq_kick += k * k;

            const double s = static_cast<double>(audio_samples[start + i]) / 32768.0;
            sum_sq_full += s * s;
        }

        const float rms_kick = static_cast<float>(std::sqrt(sum_sq_kick / static_cast<double>(kDetailedWindow)));
        const float rms_full = static_cast<float>(std::sqrt(sum_sq_full / static_cast<double>(kDetailedWindow)));

        // Kick transients punch up sharply to 0.9 - 1.0, dipping between beats to 0.15 - 0.3
        const float env = 0.10f + rms_kick * 2.8f + rms_full * 0.4f;
        result.detailed_peaks[w] = std::clamp(env, 0.05f, 1.0f);
    }

    // 4. Onset Novelty Curve at 500 Hz (~2 ms windows = 44 samples)
    const size_t num_onset_windows = total_samples / kOnsetWindowSize;
    if (num_onset_windows < 500)
        return result;

    std::vector<float> kick_energy(num_onset_windows, 0.0f);
    for (size_t w = 0; w < num_onset_windows; ++w) {
        const size_t start = w * kOnsetWindowSize;
        double sum_sq = 0.0;
        for (size_t i = 0; i < kOnsetWindowSize; ++i) {
            const float val = kick_samples[start + i];
            sum_sq += val * val;
        }
        kick_energy[w] = static_cast<float>(std::sqrt(sum_sq / static_cast<double>(kOnsetWindowSize)));
    }

    std::vector<float> novelty(num_onset_windows, 0.0f);
    for (size_t w = 1; w < num_onset_windows; ++w) {
        const float diff = kick_energy[w] - kick_energy[w - 1];
        novelty[w] = (diff > 0.0f) ? diff : 0.0f;
    }

    // 5. Autocorrelation over 65 to 185 BPM at 500 Hz resolution
    // Lag in steps of 2ms:
    // 65 BPM  -> lag ~ 462
    // 185 BPM -> lag ~ 162
    constexpr int kMinLag = 160; // ~187 BPM
    constexpr int kMaxLag = 470; // ~63 BPM

    std::vector<double> autocorr(kMaxLag + 2, 0.0);
    const size_t max_novelty_len = std::min<size_t>(novelty.size(), 500 * 90); // 90 seconds analysis

    int best_lag = 234; // ~128 BPM
    double best_score = -1.0;

    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        double sum = 0.0;
        const size_t count = max_novelty_len - lag;
        for (size_t i = 0; i < count; ++i) {
            sum += static_cast<double>(novelty[i]) * static_cast<double>(novelty[i + lag]);
        }

        const double bpm = (60.0 * static_cast<double>(kSampleRate)) / (static_cast<double>(kOnsetWindowSize) * lag);
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

    const double samples_per_beat = fine_lag * static_cast<double>(kOnsetWindowSize);
    const double detected_bpm = (60.0 * static_cast<double>(kSampleRate)) / samples_per_beat;
    result.detected_bpm = std::round(detected_bpm * 10.0) / 10.0;

    // 7. Downbeat (Kick 1) Anchor Alignment
    // Scan the first 25 seconds for the beat phase offset that aligns with maximum kick spikes
    const int period = std::max(1, static_cast<int>(std::round(fine_lag)));
    const size_t max_search_windows = std::min<size_t>(novelty.size() - period, 500 * 25);

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

    // Convert offset windows to milliseconds (each window is kOnsetWindowSize / kSampleRate seconds)
    const double ms_per_window = (static_cast<double>(kOnsetWindowSize) / static_cast<double>(kSampleRate)) * 1000.0;
    result.grid_offset_ms = static_cast<int64_t>(best_offset * ms_per_window);
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
