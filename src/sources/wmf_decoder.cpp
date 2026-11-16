#include "sources/wmf_decoder.hpp"

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

#include <cmath>
#include <algorithm>

bool WmfDecoder::decodePcm(const QString &file_path,
                           std::vector<float> &out_l,
                           std::vector<float> &out_r,
                           std::vector<float> *out_mono,
                           uint32_t &out_sample_rate,
                           int64_t &out_duration_ms)
{
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool co_initialized = SUCCEEDED(hr);

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        if (co_initialized)
            CoUninitialize();
        return false;
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
        return false;
    }

    // Teljes időtartam lekérdezése (Read duration)
    PROPVARIANT var;
    PropVariantInit(&var);
    if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,
                                                    MF_PD_DURATION, &var))) {
        out_duration_ms = var.uhVal.QuadPart / 10000; // 100-nanoszekundum -> ms
        PropVariantClear(&var);
    }

    // Első audió sáv kiválasztása (Select first audio stream)
    pReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    pReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    // Kétcsatornás (sztereó) 16-bites PCM kérés 44100 Hz-en
    IMFMediaType *pPartialType = nullptr;
    hr = MFCreateMediaType(&pPartialType);
    if (SUCCEEDED(hr)) {
        pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        pPartialType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
        pPartialType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
        pPartialType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
        pPartialType->Release();
    }

    // Általános dekódoló fallback ha a szigorú formátumot elutasította
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
        return false;
    }

    // Ténylegesen beállított formátum lekérése
    UINT32 channels = 2;
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
    if (channels == 0) channels = 2;
    if (sample_rate == 0) sample_rate = 44100;
    if (bits_per_sample == 0) bits_per_sample = 16;
    out_sample_rate = sample_rate;

    out_l.clear();
    out_r.clear();
    out_l.reserve(sample_rate * 240);
    out_r.reserve(sample_rate * 240);
    if (out_mono) {
        out_mono->clear();
        out_mono->reserve(sample_rate * 240);
    }

    // Maximum 4 óra sávhossz (4 hours = 14400s)
    const size_t max_total_samples = static_cast<size_t>(sample_rate * 14400);
    while (out_l.size() < max_total_samples) {
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
                            const float sl = static_cast<float>(pcm[f * channels + 0]) / 32768.0f;
                            const float sr = static_cast<float>(pcm[f * channels + (channels > 1 ? 1 : 0)]) / 32768.0f;
                            out_l.push_back(sl);
                            out_r.push_back(sr);
                            if (out_mono) {
                                out_mono->push_back(0.5f * (sl + sr));
                            }
                        }
                    } else if (bits_per_sample == 32) {
                        const auto *flt = reinterpret_cast<const float *>(pAudioData);
                        const size_t num_frames = cbCurrentLength / (sizeof(float) * channels);
                        for (size_t f = 0; f < num_frames; ++f) {
                            const float sl = flt[f * channels + 0];
                            const float sr = flt[f * channels + (channels > 1 ? 1 : 0)];
                            out_l.push_back(sl);
                            out_r.push_back(sr);
                            if (out_mono) {
                                out_mono->push_back(0.5f * (sl + sr));
                            }
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

    if (out_duration_ms <= 0 && sample_rate > 0 && !out_l.empty()) {
        out_duration_ms = static_cast<int64_t>((static_cast<double>(out_l.size()) / static_cast<double>(sample_rate)) * 1000.0);
    }

    return !out_l.empty();
#else
    return false;
#endif
}
