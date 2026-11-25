#include "audio/asio_backend.hpp"
#include "audio/asio_defs.hpp"

#include <obs.h>
#include <windows.h>
#include <array>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {

template<size_t Capacity = 8192>
class SpscRingBuffer {
    std::array<float, Capacity> buffer_{};
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};

public:
    void write(const float *src, size_t count) noexcept {
        if (!src || count == 0) return;
        const size_t w = write_pos_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i) {
            buffer_[(w + i) % Capacity] = src[i];
        }
        write_pos_.store((w + count) % Capacity, std::memory_order_release);
    }

    void read(float *dst, size_t count) noexcept {
        if (!dst || count == 0) return;
        const size_t r = read_pos_.load(std::memory_order_relaxed);
        const size_t w = write_pos_.load(std::memory_order_acquire);
        const size_t available = (w >= r) ? (w - r) : (Capacity - r + w);
        const size_t to_read = (count < available) ? count : available;

        for (size_t i = 0; i < to_read; ++i) {
            dst[i] = buffer_[(r + i) % Capacity];
        }
        for (size_t i = to_read; i < count; ++i) {
            dst[i] = 0.0f;
        }
        read_pos_.store((r + to_read) % Capacity, std::memory_order_release);
    }

    void reset() noexcept {
        write_pos_.store(0, std::memory_order_relaxed);
        read_pos_.store(0, std::memory_order_relaxed);
        buffer_.fill(0.0f);
    }
};

static AsioBackend::Impl *g_asio_impl = nullptr;

void asio_buffer_switch_callback(long doubleBufferIndex, ASIOBool directProcess);
void asio_sample_rate_changed_callback(ASIOSampleRate sRate);
long asio_message_callback(long selector, long value, void *message, double *opt);
ASIOTime *asio_buffer_switch_time_info_callback(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);

} // namespace

struct AsioBackend::Impl {
    IASIO *driver_ = nullptr;
    std::string driver_name_;
    int sample_rate_ = 48000;
    int buffer_size_ = 256;
    long num_inputs_ = 0;
    long num_outputs_ = 0;
    std::atomic<bool> is_running_{false};
    bool output_ready_supported_ = false;

    AsioChannelRouting routing_{};
    std::atomic<bool> routing_updated_{false};

    std::vector<ASIOBufferInfo> buffer_infos_;
    std::vector<ASIOChannelInfo> channel_infos_;
    std::array<SpscRingBuffer<8192>, 32> channel_rings_{};
    std::array<float, 8192> temp_render_buf_{};

    ASIOCallbacks callbacks_{
        &asio_buffer_switch_callback,
        &asio_sample_rate_changed_callback,
        &asio_message_callback,
        &asio_buffer_switch_time_info_callback
    };

    void processBufferSwitch(long doubleBufferIndex) noexcept {
        if (!is_running_.load(std::memory_order_acquire) || !driver_) return;

        const size_t frames = std::min(static_cast<size_t>(buffer_size_), temp_render_buf_.size());
        if (frames == 0) return;
        const size_t ch_count = buffer_infos_.size();

        for (size_t i = 0; i < ch_count; ++i) {
            const long ch_idx = buffer_infos_[i].channelNum;
            void *dst_buf = buffer_infos_[i].buffers[doubleBufferIndex];
            if (!dst_buf) continue;

            float *temp = temp_render_buf_.data();
            if (ch_idx >= 0 && ch_idx < 32) {
                channel_rings_[ch_idx].read(temp, frames);
            } else {
                std::fill(temp, temp + frames, 0.0f);
            }

            const long sample_type = (i < channel_infos_.size()) ? channel_infos_[i].type : ASIOSTInt32LSB;
            switch (sample_type) {
            case ASIOSTInt32LSB: {
                auto *out32 = reinterpret_cast<int32_t *>(dst_buf);
                for (size_t s = 0; s < frames; ++s) {
                    out32[s] = static_cast<int32_t>(std::clamp(temp[s], -1.0f, 1.0f) * 2147483520.0f);
                }
                break;
            }
            case ASIOSTFloat32LSB: {
                auto *out_f = reinterpret_cast<float *>(dst_buf);
                for (size_t s = 0; s < frames; ++s) {
                    out_f[s] = std::clamp(temp[s], -1.0f, 1.0f);
                }
                break;
            }
            case ASIOSTInt24LSB: {
                auto *out24 = reinterpret_cast<uint8_t *>(dst_buf);
                for (size_t s = 0; s < frames; ++s) {
                    const int32_t val = static_cast<int32_t>(std::clamp(temp[s], -1.0f, 1.0f) * 8388600.0f);
                    out24[s * 3 + 0] = static_cast<uint8_t>(val & 0xFF);
                    out24[s * 3 + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
                    out24[s * 3 + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
                }
                break;
            }
            case ASIOSTInt16LSB: {
                auto *out16 = reinterpret_cast<int16_t *>(dst_buf);
                for (size_t s = 0; s < frames; ++s) {
                    out16[s] = static_cast<int16_t>(std::clamp(temp[s], -1.0f, 1.0f) * 32767.0f);
                }
                break;
            }
            default:
                std::memset(dst_buf, 0, frames * sizeof(int32_t));
                break;
            }
        }

        if (output_ready_supported_) {
            driver_->outputReady();
        }
    }
};

namespace {
void asio_buffer_switch_callback(long doubleBufferIndex, ASIOBool)
{
    if (g_asio_impl) {
        g_asio_impl->processBufferSwitch(doubleBufferIndex);
    }
}
void asio_sample_rate_changed_callback(ASIOSampleRate) {}
long asio_message_callback(long selector, long, void *, double *)
{
    switch (selector) {
    case 1: // kAsioSelectorSupported
        return 1;
    case 2: // kAsioEngineVersion
        return 2;
    case 3: // kAsioResetRequest
        blog(LOG_WARNING, "[asio_backend] ASIO driver requested reset (kAsioResetRequest)");
        return 1;
    default:
        break;
    }
    return 0;
}
ASIOTime *asio_buffer_switch_time_info_callback(ASIOTime *, long doubleBufferIndex, ASIOBool)
{
    if (g_asio_impl) {
        g_asio_impl->processBufferSwitch(doubleBufferIndex);
    }
    return nullptr;
}
} // namespace

AsioBackend &AsioBackend::instance()
{
    static AsioBackend s_inst;
    return s_inst;
}

AsioBackend::AsioBackend()
    : impl_(std::make_unique<Impl>())
{
    g_asio_impl = impl_.get();
}

AsioBackend::~AsioBackend()
{
    close();
    if (g_asio_impl == impl_.get())
        g_asio_impl = nullptr;
}

bool AsioBackend::open(const std::string &driver_name, int sample_rate, int preferred_buffer_size)
{
    close();
    if (driver_name.empty()) return false;

    // Registry CLSID lookup
    const std::string subkey = "SOFTWARE\\ASIO\\" + driver_name;
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        blog(LOG_WARNING, "[asio_backend] Cannot find registry key: %s", subkey.c_str());
        return false;
    }

    char clsid_str[128] = {};
    DWORD clsid_len = sizeof(clsid_str);
    if (RegQueryValueExA(hKey, "CLSID", nullptr, nullptr, reinterpret_cast<LPBYTE>(clsid_str), &clsid_len) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        blog(LOG_WARNING, "[asio_backend] Missing CLSID for ASIO driver: %s", driver_name.c_str());
        return false;
    }
    RegCloseKey(hKey);

    wchar_t clsid_w[128] = {};
    MultiByteToWideChar(CP_ACP, 0, clsid_str, -1, clsid_w, 128);
    CLSID clsid = {};
    if (FAILED(CLSIDFromString(clsid_w, &clsid))) {
        blog(LOG_WARNING, "[asio_backend] Invalid CLSID string: %s", clsid_str);
        return false;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IASIO *driver = nullptr;
    HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, clsid, reinterpret_cast<void **>(&driver));
    if (FAILED(hr) || !driver) {
        hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_IASIO, reinterpret_cast<void **>(&driver));
    }
    if (FAILED(hr) || !driver) {
        blog(LOG_WARNING, "[asio_backend] CoCreateInstance failed for %s (0x%08X)", driver_name.c_str(), hr);
        return false;
    }

    if (!driver->init(nullptr)) {
        driver->Release();
        blog(LOG_WARNING, "[asio_backend] ASIO init() returned false for %s", driver_name.c_str());
        return false;
    }

    long num_in = 0, num_out = 0;
    driver->getChannels(&num_in, &num_out);
    if (num_out <= 0) {
        driver->Release();
        blog(LOG_WARNING, "[asio_backend] %s has 0 output channels", driver_name.c_str());
        return false;
    }

    if (driver->setSampleRate(static_cast<ASIOSampleRate>(sample_rate)) != ASE_OK) {
        blog(LOG_WARNING, "[asio_backend] setSampleRate(%d) returned non-OK for %s", sample_rate, driver_name.c_str());
    }

    long min_sz = 0, max_sz = 0, pref_sz = 0, gran = 0;
    driver->getBufferSize(&min_sz, &max_sz, &pref_sz, &gran);
    long actual_buf = (preferred_buffer_size > 0) ? preferred_buffer_size : pref_sz;
    if (min_sz > 0 && actual_buf < min_sz) actual_buf = min_sz;
    if (max_sz > 0 && actual_buf > max_sz) actual_buf = max_sz;

    impl_->temp_render_buf_.fill(0.0f);

    const long channels_to_alloc = std::min(num_out, 32L);
    impl_->buffer_infos_.resize(channels_to_alloc);
    impl_->channel_infos_.resize(channels_to_alloc);

    for (long i = 0; i < channels_to_alloc; ++i) {
        impl_->buffer_infos_[i].isInput = ASIOFalse;
        impl_->buffer_infos_[i].channelNum = i;
        impl_->buffer_infos_[i].buffers[0] = nullptr;
        impl_->buffer_infos_[i].buffers[1] = nullptr;

        impl_->channel_infos_[i].channel = i;
        impl_->channel_infos_[i].isInput = ASIOFalse;
        driver->getChannelInfo(&impl_->channel_infos_[i]);
    }

    if (driver->createBuffers(impl_->buffer_infos_.data(), channels_to_alloc, actual_buf, &impl_->callbacks_) != ASE_OK) {
        driver->Release();
        blog(LOG_WARNING, "[asio_backend] createBuffers failed for %s", driver_name.c_str());
        return false;
    }

    for (auto &ring : impl_->channel_rings_) {
        ring.reset();
    }

    impl_->output_ready_supported_ = (driver->outputReady() == ASE_OK);
    if (driver->start() != ASE_OK) {
        driver->disposeBuffers();
        driver->Release();
        blog(LOG_WARNING, "[asio_backend] ASIO start() failed for %s", driver_name.c_str());
        return false;
    }

    impl_->driver_ = driver;
    impl_->driver_name_ = driver_name;
    impl_->sample_rate_ = sample_rate;
    impl_->buffer_size_ = static_cast<int>(actual_buf);
    impl_->num_inputs_ = num_in;
    impl_->num_outputs_ = num_out;
    impl_->is_running_.store(true, std::memory_order_release);

    blog(LOG_INFO, "[asio_backend] Successfully started ASIO driver '%s' (%d outputs, %d Hz, %d samples buffer)",
         driver_name.c_str(), static_cast<int>(num_out), sample_rate, static_cast<int>(actual_buf));
    return true;
}

void AsioBackend::close()
{
    if (!impl_ || !impl_->driver_) return;
    impl_->is_running_.store(false, std::memory_order_release);
    impl_->driver_->stop();
    impl_->driver_->disposeBuffers();
    impl_->driver_->Release();
    impl_->driver_ = nullptr;
    impl_->driver_name_.clear();
    impl_->buffer_infos_.clear();
    impl_->channel_infos_.clear();
    for (auto &ring : impl_->channel_rings_) ring.reset();
    blog(LOG_INFO, "[asio_backend] ASIO driver closed");
}

bool AsioBackend::isRunning() const noexcept
{
    return impl_ && impl_->is_running_.load(std::memory_order_acquire);
}

std::string AsioBackend::currentDriverName() const
{
    return impl_ ? impl_->driver_name_ : std::string();
}

void AsioBackend::setRouting(const AsioChannelRouting &routing) noexcept
{
    if (impl_) {
        impl_->routing_ = routing;
    }
}

AsioChannelRouting AsioBackend::routing() const noexcept
{
    return impl_ ? impl_->routing_ : AsioChannelRouting{};
}

void AsioBackend::writeMasterFrames(const float *l, const float *r, size_t frames) noexcept
{
    if (!impl_ || !impl_->is_running_.load(std::memory_order_acquire) || frames == 0) return;
    const int ch_l = impl_->routing_.master_l - 1;
    const int ch_r = impl_->routing_.master_r - 1;
    if (ch_l >= 0 && ch_l < 32 && l) impl_->channel_rings_[ch_l].write(l, frames);
    if (ch_r >= 0 && ch_r < 32 && r) impl_->channel_rings_[ch_r].write(r, frames);
}

void AsioBackend::writeCueFrames(const float *l, const float *r, size_t frames) noexcept
{
    if (!impl_ || !impl_->is_running_.load(std::memory_order_acquire) || frames == 0) return;
    const int ch_l = impl_->routing_.cue_l - 1;
    const int ch_r = impl_->routing_.cue_r - 1;
    if (ch_l >= 0 && ch_l < 32 && l) impl_->channel_rings_[ch_l].write(l, frames);
    if (ch_r >= 0 && ch_r < 32 && r) impl_->channel_rings_[ch_r].write(r, frames);
}

void AsioBackend::writeDeckFrames(int deck_idx, const float *l, const float *r, size_t frames) noexcept
{
    if (!impl_ || !impl_->is_running_.load(std::memory_order_acquire) || frames == 0) return;
    int ch_l = (deck_idx == 0) ? (impl_->routing_.deck_a_l - 1) : (impl_->routing_.deck_b_l - 1);
    int ch_r = (deck_idx == 0) ? (impl_->routing_.deck_a_r - 1) : (impl_->routing_.deck_b_r - 1);
    if (ch_l >= 0 && ch_l < 32 && l) impl_->channel_rings_[ch_l].write(l, frames);
    if (ch_r >= 0 && ch_r < 32 && r) impl_->channel_rings_[ch_r].write(r, frames);
}

bool AsioBackend::openControlPanel()
{
    if (impl_ && impl_->driver_) {
        return impl_->driver_->controlPanel() == ASE_OK;
    }
    return false;
}

int AsioBackend::outputChannelCount() const noexcept
{
    return impl_ ? static_cast<int>(impl_->num_outputs_) : 0;
}

int AsioBackend::sampleRate() const noexcept
{
    return impl_ ? impl_->sample_rate_ : 48000;
}

int AsioBackend::bufferSize() const noexcept
{
    return impl_ ? impl_->buffer_size_ : 256;
}
