/**
 * @file audio_output.cpp
 * @brief WASAPI Audio Output Backend Implementation
 *
 * Professional Windows audio output using WASAPI (Windows Audio Session API).
 * Provides low-latency audio rendering with comprehensive error handling.
 */

#include "audio/audio_output.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <system_error>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <avrt.h>  // MMCSS for low-latency audio
#endif

namespace ve::audio {

// Static helper functions
namespace {

#ifdef _WIN32

/**
 * @brief Convert AudioOutputError to string
 */
const char* error_to_string(AudioOutputError error) {
    switch (error) {
        case AudioOutputError::Success: return "Success";
        case AudioOutputError::NotInitialized: return "Not initialized";
        case AudioOutputError::DeviceNotFound: return "Device not found";
        case AudioOutputError::FormatNotSupported: return "Format not supported";
        case AudioOutputError::BufferTooSmall: return "Buffer too small";
        case AudioOutputError::ExclusiveModeFailed: return "Exclusive mode failed";
        case AudioOutputError::HardwareOffloadFailed: return "Hardware offload failed";
        case AudioOutputError::ThreadError: return "Thread error";
        case AudioOutputError::InvalidState: return "Invalid state";
        case AudioOutputError::Unknown: return "Unknown error";
        default: return "Unknown error";
    }
}

/**
 * @brief Convert WAVEFORMATEX to AudioOutputConfig
 */
AudioOutputConfig wave_format_to_config(const WAVEFORMATEX* format) {
    AudioOutputConfig config;
    config.sample_rate = format->nSamplesPerSec;
    config.channel_count = format->nChannels;

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        config.format = SampleFormat::Float32;
    } else if (format->wFormatTag == WAVE_FORMAT_PCM) {
        if (format->wBitsPerSample == 16) {
            config.format = SampleFormat::Int16;
        } else if (format->wBitsPerSample == 32) {
            config.format = SampleFormat::Int32;
        }
    }

    return config;
}

/**
 * @brief Convert AudioOutputConfig to WAVEFORMATEX
 */
WAVEFORMATEX config_to_wave_format(const AudioOutputConfig& config) {
    WAVEFORMATEX format = {};
    format.wFormatTag = (config.format == SampleFormat::Float32) ?
                        WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    format.nChannels = config.channel_count;
    format.nSamplesPerSec = config.sample_rate;
    format.nAvgBytesPerSec = config.sample_rate * config.channel_count *
                            ((config.format == SampleFormat::Float32) ? 4 :
                             (config.format == SampleFormat::Int32) ? 4 : 2);
    format.nBlockAlign = config.channel_count *
                        ((config.format == SampleFormat::Float32) ? 4 :
                         (config.format == SampleFormat::Int32) ? 4 : 2);
    format.wBitsPerSample = (config.format == SampleFormat::Float32) ? 32 :
                           (config.format == SampleFormat::Int32) ? 32 : 16;
    format.cbSize = 0;

    return format;
}

/**
 * @brief Get device friendly name
 */
std::string get_device_name(IMMDevice* device) {
    IPropertyStore* props = nullptr;
    PROPVARIANT varName;
    PropVariantInit(&varName);

    std::string name = "Unknown Device";

    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
            // Convert wide string to narrow string
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, nullptr, 0, nullptr, nullptr);
            if (size_needed > 0) {
                std::vector<char> buffer(size_needed);
                WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, buffer.data(), size_needed, nullptr, nullptr);
                name = buffer.data();
            }
        }
        PropVariantClear(&varName);
        props->Release();
    }

    return name;
}

/**
 * @brief Get device ID string
 */
std::string get_device_id(IMMDevice* device) {
    LPWSTR device_id = nullptr;
    std::string id;

    if (SUCCEEDED(device->GetId(&device_id))) {
        // Convert wide string to narrow string
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, device_id, -1, nullptr, 0, nullptr, nullptr);
        if (size_needed > 0) {
            std::vector<char> buffer(size_needed);
            WideCharToMultiByte(CP_UTF8, 0, device_id, -1, buffer.data(), size_needed, nullptr, nullptr);
            id = buffer.data();
        }
        CoTaskMemFree(device_id);
    }

    return id;
}

#endif // _WIN32

} // anonymous namespace

// AudioOutput implementation

std::unique_ptr<AudioOutput> AudioOutput::create(const AudioOutputConfig& config) {
    return std::unique_ptr<AudioOutput>(new AudioOutput(config));
}

AudioOutput::AudioOutput(const AudioOutputConfig& config)
    : config_(config) {
}

AudioOutput::~AudioOutput() {
    shutdown();
}

// ---- Audio format helpers (local) -------------------------------------------------
namespace {
    // Convert various integer formats to float32 [-1,1], interleaved
    inline void int16_to_float32(const int16_t* in, float* out, size_t samples) {
        constexpr float k = 1.0f / 32768.0f;
        for (size_t i = 0; i < samples; ++i) out[i] = std::clamp(in[i] * k, -1.0f, 1.0f);
    }
    inline void int32_to_float32(const int32_t* in, float* out, size_t samples) {
        constexpr double k = 1.0 / 2147483648.0; // 2^31
        for (size_t i = 0; i < samples; ++i) out[i] = static_cast<float>(std::clamp(in[i] * k, -1.0, 1.0));
    }

    // Downmix/Upmix simple rules (interleaved)
    // mono->stereo: duplicate; stereo->mono: average; N>2 -> take first two channels
    inline void convert_channels(const float* in, int in_ch, float* out, int out_ch, size_t frames) {
        if (in_ch == out_ch) {
            std::memcpy(out, in, sizeof(float) * frames * in_ch);
            return;
        }
        if (in_ch == 1 && out_ch == 2) {
            for (size_t i = 0; i < frames; ++i) {
                float s = in[i];
                out[2*i + 0] = s;
                out[2*i + 1] = s;
            }
            return;
        }
        if (in_ch == 2 && out_ch == 1) {
            for (size_t i = 0; i < frames; ++i) {
                float l = in[2*i + 0];
                float r = in[2*i + 1];
                out[i] = 0.5f * (l + r);
            }
            return;
        }
        // Fallback: map first min(in_ch, out_ch) channels, zero the rest
        int copy_ch = std::min(in_ch, out_ch);
        for (size_t i = 0; i < frames; ++i) {
            for (int c = 0; c < copy_ch; ++c) {
                out[i*out_ch + c] = in[i*in_ch + c];
            }
            for (int c = copy_ch; c < out_ch; ++c) out[i*out_ch + c] = 0.0f;
        }
    }

    // Very small, fast linear resampler. Good enough for preview; production can swap to libswresample.
    inline void resample_linear(const float* in, size_t in_frames, int channels,
                                float* out, size_t out_frames) {
        if (in_frames == 0 || out_frames == 0) return;
        const double ratio = static_cast<double>(in_frames - 1) / static_cast<double>(out_frames - 1);
        for (size_t j = 0; j < out_frames; ++j) {
            double pos = j * ratio;
            size_t i0 = static_cast<size_t>(pos);
            size_t i1 = std::min(i0 + 1, in_frames - 1);
            float t = static_cast<float>(pos - i0);
            const float* a = &in[i0 * channels];
            const float* b = &in[i1 * channels];
            float* o = &out[j * channels];
            for (int c = 0; c < channels; ++c) {
                o[c] = a[c] + (b[c] - a[c]) * t;
            }
        }
    }

    inline void float32_to_int16(const float* in, int16_t* out, size_t samples) {
        for (size_t i = 0; i < samples; ++i) {
            float v = std::clamp(in[i], -1.0f, 1.0f);
            int s = static_cast<int>(std::lrintf(v * 32767.0f));
            out[i] = static_cast<int16_t>(std::clamp(s, -32768, 32767));
        }
    }

    inline void float32_to_int32(const float* in, int32_t* out, size_t samples) {
        for (size_t i = 0; i < samples; ++i) {
            double v = std::clamp(static_cast<double>(in[i]), -1.0, 1.0);
            long long s = llround(v * 2147483647.0);
            if (s < -2147483648LL) s = -2147483648LL;
            if (s >  2147483647LL) s =  2147483647LL;
            out[i] = static_cast<int32_t>(s);
        }
    }
}
// -----------------------------------------------------------------------------------

AudioOutputError AudioOutput::initialize() {
    if (initialized_) {
        return AudioOutputError::Success;
    }

#ifdef _WIN32
    AudioOutputError result = initialize_wasapi();
    if (result == AudioOutputError::Success) {
        initialized_ = true;
    }
    return result;
#else
    set_error(AudioOutputError::Unknown, "WASAPI audio output only supported on Windows");
    return AudioOutputError::Unknown;
#endif
}

void AudioOutput::shutdown() {
    if (!initialized_) {
        return;
    }

    // Stop playback first
    stop();

    // Shutdown WASAPI components
#ifdef _WIN32
    if (render_client_) {
        render_client_->Release();
        render_client_ = nullptr;
    }

    if (audio_client_) {
        audio_client_->Release();
        audio_client_ = nullptr;
    }

    if (audio_device_) {
        audio_device_->Release();
        audio_device_ = nullptr;
    }

    if (device_enumerator_) {
        device_enumerator_->Release();
        device_enumerator_ = nullptr;
    }

    if (render_event_) {
        CloseHandle(render_event_);
        render_event_ = nullptr;
    }
#endif

    initialized_ = false;
}

AudioOutputError AudioOutput::start() {
    if (!initialized_) {
        set_error(AudioOutputError::NotInitialized, "Audio output not initialized");
        return AudioOutputError::NotInitialized;
    }

    if (playing_) {
        return AudioOutputError::Success;
    }

#ifdef _WIN32
    HRESULT hr = audio_client_->Start();
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to start audio client: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    playing_ = true;

    // Start render thread
    thread_should_exit_ = false;
    render_thread_ = std::thread(&AudioOutput::render_thread_function, this);
#endif

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::stop() {
    if (!playing_) {
        return AudioOutputError::Success;
    }

#ifdef _WIN32
    // Stop audio client
    if (audio_client_) {
        audio_client_->Stop();
    }

    // Stop render thread
    thread_should_exit_ = true;
    if (render_thread_.joinable()) {
        render_thread_.join();
    }
#endif

    playing_ = false;
    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::submit_frame(std::shared_ptr<AudioFrame> frame) {
    if (!initialized_ || !playing_) {
        return AudioOutputError::NotInitialized;
    }

    if (!frame || frame->sample_count() == 0) {
        return AudioOutputError::InvalidState;
    }

    // PTS-based deduplication to prevent echo from duplicate submissions
    {
        std::lock_guard<std::mutex> lock(pts_mutex_);
        int64_t pts = frame->timestamp().to_rational().num;
        if (submitted_pts_.count(pts) > 0) {
            return AudioOutputError::Success; // Skip duplicate - already submitted
        }
        submitted_pts_.insert(pts);
        
        // Clean up old PTS entries to prevent memory growth (keep last 1000)
        if (submitted_pts_.size() > 1000) {
            submitted_pts_.clear();
        }
    }

    // ---- NEW: Normalize incoming frame to WASAPI client format (rate/channels/sample fmt) ----
    const uint32_t in_rate = frame->sample_rate();
    const uint32_t in_ch   = frame->channel_count();
    const uint32_t in_frames = frame->sample_count();
    const SampleFormat in_fmt = frame->format();

    const uint32_t out_rate = config_.sample_rate;
    const uint32_t out_ch   = config_.channel_count;
    const SampleFormat out_fmt = config_.format; // typically Float32 for shared-mode mix format

    // Thread-local scratch buffers to avoid realloc every call
    static thread_local std::vector<float> tl_in_float;   // interleaved float (input rate/channels)
    static thread_local std::vector<float> tl_ch_float;   // channel-converted float (still input rate)
    static thread_local std::vector<float> tl_rs_float;   // resampled float (output rate/ch)
    static thread_local std::vector<uint8_t> tl_out_bytes; // final bytes in out_fmt

    // 1) Convert incoming samples to float32 interleaved
    const size_t in_total_samples = static_cast<size_t>(in_frames) * in_ch; // interleaved samples
    tl_in_float.resize(in_total_samples);
    {
        const void* src = frame->data();
        if (in_fmt == SampleFormat::Float32) {
            std::memcpy(tl_in_float.data(), src, in_total_samples * sizeof(float));
        } else if (in_fmt == SampleFormat::Int16) {
            int16_to_float32(static_cast<const int16_t*>(src), tl_in_float.data(), in_total_samples);
        } else { // Int32 or fallback
            int32_to_float32(static_cast<const int32_t*>(src), tl_in_float.data(), in_total_samples);
        }
    }

    // 2) Channel convert to desired out_ch (still at input rate)
    tl_ch_float.resize(static_cast<size_t>(in_frames) * out_ch);
    convert_channels(tl_in_float.data(), static_cast<int>(in_ch),
                     tl_ch_float.data(), static_cast<int>(out_ch), in_frames);

    // 3) Resample if needed
    size_t out_frames = in_frames;
    if (in_rate != 0 && out_rate != 0 && in_rate != out_rate) {
        // Compute target frames (round to nearest)
        out_frames = static_cast<size_t>( (static_cast<uint64_t>(in_frames) * out_rate + (in_rate/2)) / in_rate );
        tl_rs_float.resize(out_frames * out_ch);
        resample_linear(tl_ch_float.data(), in_frames, static_cast<int>(out_ch),
                        tl_rs_float.data(), out_frames);
    } else {
        // No rate change: reuse channel-converted buffer
        tl_rs_float.swap(tl_ch_float);
    }

    // 4) Pack to target WASAPI sample format
    const size_t out_samples_interleaved = out_frames * out_ch;
    if (out_fmt == SampleFormat::Float32) {
        tl_out_bytes.resize(out_samples_interleaved * sizeof(float));
        std::memcpy(tl_out_bytes.data(), tl_rs_float.data(), tl_out_bytes.size());
    } else if (out_fmt == SampleFormat::Int16) {
        tl_out_bytes.resize(out_samples_interleaved * sizeof(int16_t));
        float32_to_int16(tl_rs_float.data(), reinterpret_cast<int16_t*>(tl_out_bytes.data()), out_samples_interleaved);
    } else { // Int32
        tl_out_bytes.resize(out_samples_interleaved * sizeof(int32_t));
        float32_to_int32(tl_rs_float.data(), reinterpret_cast<int32_t*>(tl_out_bytes.data()), out_samples_interleaved);
    }

    // 5) Submit in manageable chunks (in FRAMES @ output format)
    const uint32_t max_chunk_frames = 480; // ~10ms @ 48k
    uint32_t submitted = 0;
    while (submitted < out_frames) {
        const uint32_t chunk = std::min<uint32_t>(max_chunk_frames, static_cast<uint32_t>(out_frames - submitted));
        const size_t   bytes_per_frame = out_ch * ((out_fmt == SampleFormat::Float32 || out_fmt == SampleFormat::Int32) ? 4 : 2);
        const uint8_t* ptr = tl_out_bytes.data() + submitted * bytes_per_frame;

        AudioOutputError r = submit_data(ptr, chunk, frame->timestamp());
        if (r == AudioOutputError::BufferTooSmall) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue; // retry same chunk
        }
        if (r != AudioOutputError::Success) return r;
        submitted += chunk;
    }

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::submit_data(const void* data, uint32_t frame_count,
                                         const TimePoint& timestamp) {
    if (!initialized_ || !playing_) {
        return AudioOutputError::NotInitialized;
    }

    if (!data || frame_count == 0) {
        return AudioOutputError::InvalidState;
    }

#ifdef _WIN32
    // Check available buffer space before submitting
    UINT32 padding_frames = 0;
    HRESULT hr = audio_client_->GetCurrentPadding(&padding_frames);
    if (FAILED(hr)) {
        return AudioOutputError::Unknown;
    }
    
    UINT32 available_frames = buffer_frame_count_ - padding_frames;
    
    // If not enough space available, only submit what fits
    UINT32 frames_to_submit = std::min(frame_count, available_frames);
    if (frames_to_submit == 0) {
        handle_underrun();
        return AudioOutputError::BufferTooSmall;
    }

    // Calculate bytes to copy
    size_t bytes_to_copy = frames_to_submit * config_.channel_count *
                          ((config_.format == SampleFormat::Float32) ? 4 :
                           (config_.format == SampleFormat::Int32) ? 4 : 2);
    
    BYTE* buffer_data = nullptr;

    hr = render_client_->GetBuffer(frames_to_submit, &buffer_data);
    if (FAILED(hr)) {
        handle_underrun();
        return AudioOutputError::BufferTooSmall;
    }

    // Copy data to buffer
    memcpy(buffer_data, data, bytes_to_copy);

    hr = render_client_->ReleaseBuffer(frames_to_submit, 0);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to release buffer: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.frames_rendered += frames_to_submit;
    }
#endif

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::flush() {
    if (!initialized_) {
        return AudioOutputError::NotInitialized;
    }

#ifdef _WIN32
    if (audio_client_) {
        audio_client_->Stop();
    }
#endif

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::set_volume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);

#ifdef _WIN32
    // Note: This would require additional WASAPI interfaces for volume control
    // For now, we'll handle volume in the submit_data method
#endif

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::set_muted(bool muted) {
    muted_ = muted;
    return AudioOutputError::Success;
}

AudioOutputStats AudioOutput::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

AudioOutputError AudioOutput::set_config(const AudioOutputConfig& config) {
    if (initialized_) {
        set_error(AudioOutputError::InvalidState, "Cannot change config while initialized");
        return AudioOutputError::InvalidState;
    }

    config_ = config;
    return AudioOutputError::Success;
}

std::vector<AudioDeviceInfo> AudioOutput::enumerate_devices(bool include_inputs) {
    std::vector<AudioDeviceInfo> devices;

#ifdef _WIN32
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* collection = nullptr;

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               __uuidof(IMMDeviceEnumerator), (void**)&enumerator))) {
        return devices;
    }

    EDataFlow data_flow = include_inputs ? eAll : eRender;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(data_flow, DEVICE_STATE_ACTIVE, &collection))) {
        UINT count;
        if (SUCCEEDED(collection->GetCount(&count))) {
            for (UINT i = 0; i < count; ++i) {
                IMMDevice* device = nullptr;
                if (SUCCEEDED(collection->Item(i, &device))) {
                    AudioDeviceInfo info;
                    info.id = AudioOutput::get_device_id(device);
                    info.name = AudioOutput::get_device_name(device);
                    info.is_input = (data_flow == eAll); // Simplified

                    devices.push_back(info);
                    device->Release();
                }
            }
        }
        collection->Release();
    }

    enumerator->Release();
#endif

    return devices;
}

AudioDeviceInfo AudioOutput::get_default_device() {
    AudioDeviceInfo info;

#ifdef _WIN32
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;

    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator))) {
        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device))) {
            info.id = AudioOutput::get_device_id(device);
            info.name = AudioOutput::get_device_name(device);
            info.is_default = true;
            device->Release();
        }
        enumerator->Release();
    }
#endif

    return info;
}

AudioDeviceInfo AudioOutput::get_device_by_id(const std::string& device_id) {
    AudioDeviceInfo info;

#ifdef _WIN32
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;

    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator))) {
        std::wstring wide_id(device_id.begin(), device_id.end());
        if (SUCCEEDED(enumerator->GetDevice(wide_id.c_str(), &device))) {
            info.id = AudioOutput::get_device_id(device);
            info.name = AudioOutput::get_device_name(device);
            device->Release();
        }
        enumerator->Release();
    }
#endif

    return info;
}

std::string AudioOutput::get_last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_message_;
}

void AudioOutput::clear_error() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = AudioOutputError::Success;
    last_error_message_.clear();
}

void AudioOutput::set_underrun_callback(std::function<void()> callback) {
    underrun_callback_ = callback;
}

void AudioOutput::set_device_change_callback(std::function<void(const std::string&)> callback) {
    device_change_callback_ = callback;
}

#ifdef _WIN32

AudioOutputError AudioOutput::initialize_wasapi() {
    HRESULT hr;

    // Initialize COM
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        set_error(AudioOutputError::Unknown, "Failed to initialize COM: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    // Create device enumerator
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator), (void**)&device_enumerator_);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to create device enumerator: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    // Get audio device
    if (!config_.device_id.empty()) {
        std::wstring wide_id(config_.device_id.begin(), config_.device_id.end());
        hr = device_enumerator_->GetDevice(wide_id.c_str(), &audio_device_);
    } else {
        hr = device_enumerator_->GetDefaultAudioEndpoint(eRender, eMultimedia, &audio_device_);
    }

    if (FAILED(hr)) {
        set_error(AudioOutputError::DeviceNotFound, "Failed to get audio device: " + std::to_string(hr));
        return AudioOutputError::DeviceNotFound;
    }

    // Create audio client
    AudioOutputError result = create_audio_client();
    if (result != AudioOutputError::Success) {
        return result;
    }

    // Initialize format
    result = initialize_format();
    if (result != AudioOutputError::Success) {
        return result;
    }

    // Create render client
    result = create_render_client();
    if (result != AudioOutputError::Success) {
        return result;
    }

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::create_audio_client() {
    if (!audio_device_) {
        return AudioOutputError::DeviceNotFound;
    }

    HRESULT hr = audio_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client_);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to activate audio device: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::initialize_format() {
    if (!audio_client_) {
        return AudioOutputError::NotInitialized;
    }

    WAVEFORMATEX* mix_format = nullptr;
    HRESULT hr = audio_client_->GetMixFormat(&mix_format);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to get mix format: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    // Use the mix format as our target format
    config_.sample_rate = mix_format->nSamplesPerSec;
    config_.channel_count = mix_format->nChannels;

    if (mix_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        config_.format = SampleFormat::Float32;
    } else if (mix_format->wFormatTag == WAVE_FORMAT_PCM) {
        if (mix_format->wBitsPerSample == 16) {
            config_.format = SampleFormat::Int16;
        } else if (mix_format->wBitsPerSample == 32) {
            config_.format = SampleFormat::Int32;
        }
    }

    // Calculate buffer sizes
    REFERENCE_TIME requested_duration = config_.buffer_duration_ms * 10000; // Convert to 100-nanosecond units
    REFERENCE_TIME min_periodicity = config_.min_periodicity_ms * 10000;

    hr = audio_client_->Initialize(
        config_.exclusive_mode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
        0, // flags
        requested_duration,
        config_.exclusive_mode ? requested_duration : 0,
        mix_format,
        nullptr
    );

    CoTaskMemFree(mix_format);

    if (FAILED(hr)) {
        if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
            set_error(AudioOutputError::BufferTooSmall, "Buffer size not aligned");
            return AudioOutputError::BufferTooSmall;
        } else if (hr == AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED) {
            set_error(AudioOutputError::ExclusiveModeFailed, "Exclusive mode not allowed");
            return AudioOutputError::ExclusiveModeFailed;
        } else {
            set_error(AudioOutputError::Unknown, "Failed to initialize audio client: " + std::to_string(hr));
            return AudioOutputError::Unknown;
        }
    }

    // Get buffer size
    hr = audio_client_->GetBufferSize(&buffer_frame_count_);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to get buffer size: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.buffer_size_frames = buffer_frame_count_;
    }

    return AudioOutputError::Success;
}

AudioOutputError AudioOutput::create_render_client() {
    if (!audio_client_) {
        return AudioOutputError::NotInitialized;
    }

    HRESULT hr = audio_client_->GetService(__uuidof(IAudioRenderClient), (void**)&render_client_);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to create render client: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    return AudioOutputError::Success;
}

void AudioOutput::render_thread_function() {
    // Set thread priority for low-latency audio
    DWORD task_index = 0;
    HANDLE task_handle = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &task_index);
    if (task_handle) {
        AvSetMmThreadPriority(task_handle, AVRT_PRIORITY_HIGH);
    }

    while (!thread_should_exit_) {
        // Wait for buffer to be available
        if (render_event_) {
            WaitForSingleObject(render_event_, INFINITE);
        }

        // In a real implementation, you'd render audio here
        // For now, just sleep to simulate processing
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (task_handle) {
        AvRevertMmThreadCharacteristics(task_handle);
    }
}

void AudioOutput::handle_underrun() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.buffer_underruns++;

    if (underrun_callback_) {
        underrun_callback_();
    }
}

void AudioOutput::update_stats() {
    // Update statistics periodically
    // This would be called from the render thread
}

#endif // _WIN32

void AudioOutput::set_error(AudioOutputError error, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    last_error_message_ = message;
}

#ifdef _WIN32

std::string AudioOutput::get_device_id(IMMDevice* device) {
    if (!device) return "";

    LPWSTR device_id = nullptr;
    if (SUCCEEDED(device->GetId(&device_id))) {
        // Convert wide string to narrow string
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, device_id, -1, nullptr, 0, nullptr, nullptr);
        std::string result(size_needed - 1, '\0'); // -1 to exclude null terminator
        WideCharToMultiByte(CP_UTF8, 0, device_id, -1, &result[0], size_needed, nullptr, nullptr);
        CoTaskMemFree(device_id);
        return result;
    }
    return "";
}

std::string AudioOutput::get_device_name(IMMDevice* device) {
    if (!device) return "";

    IPropertyStore* props = nullptr;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
        PROPVARIANT var;
        PropVariantInit(&var);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
            // Convert wide string to narrow string
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, nullptr, 0, nullptr, nullptr);
            std::string result(size_needed - 1, '\0'); // -1 to exclude null terminator
            WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, &result[0], size_needed, nullptr, nullptr);
            PropVariantClear(&var);
            props->Release();
            return result;
        }
        PropVariantClear(&var);
        props->Release();
    }
    return "";
}

#endif // _WIN32

} // namespace ve::audio
