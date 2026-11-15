#include "wasapi-output.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <obs.h>

#include <vector>
#include <algorithm>
#include <cmath>

struct WasapiOutput::Impl {
    std::string device_id_{};
    IMMDeviceEnumerator *enumerator_ = nullptr;
    IMMDevice *device_ = nullptr;
    IAudioClient *client_ = nullptr;
    IAudioRenderClient *render_ = nullptr;
    WAVEFORMATEX *format_ = nullptr;
    UINT32 buffer_frames_ = 0;
    bool is_running_ = false;
    bool is_float_ = true;
    WORD channels_ = 2;
    DWORD sample_rate_ = 48000;
    WORD bits_per_sample_ = 32;

    void cleanup() {
        if (client_) {
            client_->Stop();
        }
        if (render_) {
            render_->Release();
            render_ = nullptr;
        }
        if (client_) {
            client_->Release();
            client_ = nullptr;
        }
        if (device_) {
            device_->Release();
            device_ = nullptr;
        }
        if (enumerator_) {
            enumerator_->Release();
            enumerator_ = nullptr;
        }
        if (format_) {
            CoTaskMemFree(format_);
            format_ = nullptr;
        }
        is_running_ = false;
    }
};

WasapiOutput::WasapiOutput()
    : impl_(std::make_unique<Impl>())
{
}

WasapiOutput::~WasapiOutput()
{
    close();
}

bool WasapiOutput::open(const std::string &device_id)
{
    close();

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&impl_->enumerator_));
    if (FAILED(hr) || !impl_->enumerator_) {
        blog(LOG_WARNING, "[wasapi_cue] Failed to create MMDeviceEnumerator: 0x%08X", hr);
        impl_->cleanup();
        return false;
    }

    if (device_id.empty() || device_id == "default") {
        hr = impl_->enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &impl_->device_);
    } else {
        const int len = MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, nullptr, 0);
        std::wstring wid;
        if (len > 1) {
            wid.resize(len - 1);
            MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, &wid[0], len);
        }
        hr = impl_->enumerator_->GetDevice(wid.c_str(), &impl_->device_);

        // Fallback: enumerate active render endpoints and search by ID substring
        if (FAILED(hr) || !impl_->device_) {
            IMMDeviceCollection *collection = nullptr;
            if (SUCCEEDED(impl_->enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)) && collection) {
                UINT count = 0;
                collection->GetCount(&count);
                for (UINT i = 0; i < count; ++i) {
                    IMMDevice *dev = nullptr;
                    if (SUCCEEDED(collection->Item(i, &dev)) && dev) {
                        LPWSTR dev_id_w = nullptr;
                        if (SUCCEEDED(dev->GetId(&dev_id_w)) && dev_id_w) {
                            if (wcsstr(dev_id_w, wid.c_str()) != nullptr ||
                                wcsstr(wid.c_str(), dev_id_w) != nullptr) {
                                impl_->device_ = dev;
                                dev->AddRef();
                                CoTaskMemFree(dev_id_w);
                                dev->Release();
                                hr = S_OK;
                                break;
                            }
                            CoTaskMemFree(dev_id_w);
                        }
                        dev->Release();
                    }
                }
                collection->Release();
            }
        }
    }

    if (FAILED(hr) || !impl_->device_) {
        blog(LOG_WARNING, "[wasapi_cue] Failed to get audio device for '%s': 0x%08X, falling back to default", device_id.c_str(), hr);
        hr = impl_->enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &impl_->device_);
        if (FAILED(hr) || !impl_->device_) {
            impl_->cleanup();
            return false;
        }
    }

    hr = impl_->device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&impl_->client_));
    if (FAILED(hr) || !impl_->client_) {
        blog(LOG_WARNING, "[wasapi_cue] Failed to activate IAudioClient: 0x%08X", hr);
        impl_->cleanup();
        return false;
    }

    hr = impl_->client_->GetMixFormat(&impl_->format_);
    if (FAILED(hr) || !impl_->format_) {
        blog(LOG_WARNING, "[wasapi_cue] Failed to get mix format: 0x%08X", hr);
        impl_->cleanup();
        return false;
    }

    impl_->sample_rate_ = impl_->format_->nSamplesPerSec;
    impl_->channels_ = impl_->format_->nChannels;
    impl_->bits_per_sample_ = impl_->format_->wBitsPerSample;

    impl_->is_float_ = false;
    if (impl_->format_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        impl_->is_float_ = true;
    } else if (impl_->format_->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto *ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(impl_->format_);
        if (IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            impl_->is_float_ = true;
        }
    }

    // 50ms buffer
    const REFERENCE_TIME buf_duration = 500000;
    hr = impl_->client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buf_duration, 0, impl_->format_, nullptr);
    if (FAILED(hr)) {
        blog(LOG_WARNING, "[wasapi_cue] Failed to initialize IAudioClient: 0x%08X", hr);
        impl_->cleanup();
        return false;
    }

    hr = impl_->client_->GetBufferSize(&impl_->buffer_frames_);
    if (FAILED(hr)) {
        impl_->cleanup();
        return false;
    }

    hr = impl_->client_->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&impl_->render_));
    if (FAILED(hr) || !impl_->render_) {
        impl_->cleanup();
        return false;
    }

    hr = impl_->client_->Start();
    if (FAILED(hr)) {
        impl_->cleanup();
        return false;
    }

    impl_->device_id_ = device_id;
    impl_->is_running_ = true;
    blog(LOG_INFO, "[wasapi_cue] Successfully opened audio device '%s' (%u Hz, %d ch, %s)",
         device_id.c_str(), impl_->sample_rate_, impl_->channels_, impl_->is_float_ ? "float" : "pcm");
    return true;
}

void WasapiOutput::close()
{
    if (impl_) {
        impl_->cleanup();
    }
}

bool WasapiOutput::writeFrames(const float *l, const float *r, size_t frames)
{
    if (!impl_ || !impl_->is_running_ || !impl_->render_ || frames == 0)
        return false;

    // Resample if device sample rate differs from 48000 Hz
    const float *src_l = l;
    const float *src_r = r;
    size_t out_frames = frames;

    std::vector<float> resamp_l;
    std::vector<float> resamp_r;
    if (impl_->sample_rate_ != 48000 && frames > 1) {
        out_frames = static_cast<size_t>(std::round(static_cast<double>(frames) * impl_->sample_rate_ / 48000.0));
        resamp_l.resize(out_frames);
        resamp_r.resize(out_frames);
        const double step = static_cast<double>(frames - 1) / static_cast<double>(out_frames > 1 ? out_frames - 1 : 1);
        for (size_t i = 0; i < out_frames; ++i) {
            const double pos = i * step;
            const size_t idx = static_cast<size_t>(pos);
            const double frac = pos - idx;
            const size_t next_idx = std::min(idx + 1, frames - 1);
            resamp_l[i] = static_cast<float>(l[idx] * (1.0 - frac) + l[next_idx] * frac);
            resamp_r[i] = static_cast<float>(r[idx] * (1.0 - frac) + r[next_idx] * frac);
        }
        src_l = resamp_l.data();
        src_r = resamp_r.data();
    }

    UINT32 padding = 0;
    HRESULT hr = impl_->client_->GetCurrentPadding(&padding);
    if (FAILED(hr)) {
        return false;
    }

    const UINT32 available = impl_->buffer_frames_ - padding;
    const UINT32 to_write = std::min(static_cast<UINT32>(out_frames), available);
    if (to_write == 0)
        return true;

    BYTE *data = nullptr;
    hr = impl_->render_->GetBuffer(to_write, &data);
    if (FAILED(hr) || !data)
        return false;

    const int ch = impl_->channels_;
    if (impl_->is_float_) {
        float *out = reinterpret_cast<float*>(data);
        for (UINT32 i = 0; i < to_write; ++i) {
            out[i * ch + 0] = src_l[i];
            if (ch > 1) {
                out[i * ch + 1] = src_r[i];
            }
            for (int c = 2; c < ch; ++c) {
                out[i * ch + c] = 0.0f;
            }
        }
    } else {
        int16_t *out = reinterpret_cast<int16_t*>(data);
        for (UINT32 i = 0; i < to_write; ++i) {
            const float sl = std::clamp(src_l[i], -1.0f, 1.0f);
            const float sr = std::clamp(src_r[i], -1.0f, 1.0f);
            out[i * ch + 0] = static_cast<int16_t>(sl * 32767.0f);
            if (ch > 1) {
                out[i * ch + 1] = static_cast<int16_t>(sr * 32767.0f);
            }
            for (int c = 2; c < ch; ++c) {
                out[i * ch + c] = 0;
            }
        }
    }

    impl_->render_->ReleaseBuffer(to_write, 0);
    return true;
}

bool WasapiOutput::isRunning() const
{
    return impl_ && impl_->is_running_;
}

std::string WasapiOutput::currentDeviceId() const
{
    return impl_ ? impl_->device_id_ : std::string();
}
