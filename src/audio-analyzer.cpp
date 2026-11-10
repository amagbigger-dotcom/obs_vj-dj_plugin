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
constexpr uint32_t kSampleRate = 11025; // 11.025 kHz is ideal for fast onset/beat tracking
constexpr size_t kMaxAnalysisSeconds = 180; // First 3 minutes is plenty for accurate BPM & downbeat
constexpr size_t kMaxSamples = kSampleRate * kMaxAnalysisSeconds;
constexpr size_t kWaveformBuckets = 600;
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

    // Configure uncompressed 11025 Hz 16-bit mono PCM output
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
    audio_samples.reserve(kSampleRate * 60); // Reserve 1 minute

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

    // 1. Calculate Real Waveform Envelope (Peak Buckets)
    result.waveform_peaks.resize(kWaveformBuckets, 0.05f);
    const size_t total_samples = audio_samples.size();
    const size_t samples_per_bucket = std::max<size_t>(1, total_samples / kWaveformBuckets);

    for (size_t b = 0; b < kWaveformBuckets; ++b) {
        const size_t start = b * samples_per_bucket;
        const size_t end = std::min(start + samples_per_bucket, total_samples);

        int32_t max_val = 0;
        for (size_t i = start; i < end; ++i) {
            max_val = std::max(max_val, static_cast<int32_t>(std::abs(audio_samples[i])));
        }

        const float peak = static_cast<float>(max_val) / 32768.0f;
        result.waveform_peaks[b] = std::clamp(peak, 0.05f, 1.0f);
    }

    // 2. Onset Detection Function / Novelty Curve
    // Window of ~10 ms (110 samples at 11025 Hz)
    constexpr size_t kWindowSize = 110;
    const size_t num_windows = total_samples / kWindowSize;
    if (num_windows < 500)
        return result; // Not enough audio for beat tracking

    std::vector<float> energy(num_windows, 0.0f);
    for (size_t w = 0; w < num_windows; ++w) {
        double sum_sq = 0.0;
        const size_t offset = w * kWindowSize;
        for (size_t i = 0; i < kWindowSize; ++i) {
            const double s = static_cast<double>(audio_samples[offset + i]) / 32768.0;
            sum_sq += s * s;
        }
        energy[w] = static_cast<float>(std::sqrt(sum_sq / static_cast<double>(kWindowSize)));
    }

    // Half-wave rectified first difference (spectral flux proxy)
    std::vector<float> novelty(num_windows, 0.0f);
    for (size_t w = 1; w < num_windows; ++w) {
        const float diff = energy[w] - energy[w - 1];
        novelty[w] = (diff > 0.0f) ? diff : 0.0f;
    }

    // 3. Autocorrelation Tempo Estimation (Range 60 - 190 BPM)
    // Step size is ~10 ms (100 windows per second)
    // BPM = 60000 / (lag_in_ms) = (60 * 100) / lag = 6000 / lag
    // For BPM in [60, 190]: lag in [6000/190, 6000/60] = [31, 100]
    constexpr int kMinLag = 31;  // ~193 BPM
    constexpr int kMaxLag = 100; // ~60 BPM

    double best_score = -1.0;
    int best_lag = 47; // Default ~127.6 BPM

    const size_t max_novelty_len = std::min<size_t>(novelty.size(), 100 * 90); // Use up to 90 seconds

    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        double sum = 0.0;
        const size_t count = max_novelty_len - lag;
        for (size_t i = 0; i < count; ++i) {
            sum += static_cast<double>(novelty[i]) * static_cast<double>(novelty[i + lag]);
        }

        const double bpm = 6000.0 / static_cast<double>(lag);
        // Gaussian tempo prior centered around 125 BPM (sigma = 35) to favor dance tempos and resolve octaves
        const double prior = std::exp(-std::pow(bpm - 125.0, 2.0) / (2.0 * 35.0 * 35.0));
        const double score = sum * prior;

        if (score > best_score) {
            best_score = score;
            best_lag = lag;
        }
    }

    const double detected_bpm = 6000.0 / static_cast<double>(best_lag);
    result.detected_bpm = std::round(detected_bpm * 10.0) / 10.0; // 1 decimal place

    // 4. Downbeat Anchor Detection (First Beat Offset)
    // Check offsets in first 15 seconds to maximize alignment with beat period
    const int period = best_lag;
    const size_t max_search_windows = std::min<size_t>(novelty.size() - period, 100 * 15); // First 15 seconds

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

    // Convert offset windows to milliseconds (each window is ~10 ms)
    result.grid_offset_ms = static_cast<int64_t>(best_offset) * 10;
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

