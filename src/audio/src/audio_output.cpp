/**
 * @file audio_output.cpp
 * @brief WASAPI Audio Output Backend Implementation
 *
 * Professional Windows audio output using WASAPI (Windows Audio Session API).
 * Provides low-latency audio rendering with comprehensive error handling.
 */

#include "audio/audio_output.hpp"
#include "core/log.hpp"
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

    clock_running_.store(false, std::memory_order_release);

    // Stop playback first
    stop();

    // Shutdown WASAPI components
#ifdef _WIN32
    if (render_client_) {
        render_client_->Release();
        render_client_ = nullptr;
    }

    if (audio_clock_) {
        audio_clock_->Release();
        audio_clock_ = nullptr;
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

    if (audio_clock_ && audio_clock_frequency_ > 0.0) {
        std::lock_guard<std::mutex> lock(clock_mutex_);
        UINT64 position = 0;
        UINT64 qpc = 0;
        if (SUCCEEDED(audio_clock_->GetPosition(&position, &qpc))) {
            clock_start_position_.store(position, std::memory_order_release);
            clock_last_position_.store(position, std::memory_order_release);
            clock_running_.store(true, std::memory_order_release);
        } else {
            clock_running_.store(false, std::memory_order_release);
        }
    } else {
        clock_running_.store(false, std::memory_order_release);
    }

    // Preroll: Fill device buffer before starting event-driven loop
    UINT32 preroll_padding = 0;
    if (SUCCEEDED(audio_client_->GetCurrentPadding(&preroll_padding))) {
        UINT32 preroll_available = buffer_frame_count_ - preroll_padding;
        if (preroll_available > 0) {
            BYTE* preroll_data = nullptr;
            if (SUCCEEDED(render_client_->GetBuffer(preroll_available, &preroll_data))) {
                const size_t sample_width =
                    (config_.format == SampleFormat::Float32) ? sizeof(float) :
                    (config_.format == SampleFormat::Int32) ? sizeof(int32_t) : sizeof(int16_t);
                const size_t bytes_per_frame = static_cast<size_t>(config_.channel_count) * sample_width;
                std::memset(preroll_data, 0, static_cast<size_t>(preroll_available) * bytes_per_frame);
                if (FAILED(render_client_->ReleaseBuffer(preroll_available, AUDCLNT_BUFFERFLAGS_SILENT))) {
                    ve::log::error("AudioOutput: preroll ReleaseBuffer failed");
                }
                ve::log::info("WASAPI_PREROLL: filled " + std::to_string(preroll_available) + " frames of silence");
            }
        }
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

    clock_running_.store(false, std::memory_order_release);

#ifdef _WIN32
    // Stop audio client
    if (audio_client_) {
        audio_client_->Stop();
    }

    // Stop render thread
    thread_should_exit_ = true;
    
    // PHASE 2: Wake up render thread from WaitForSingleObject if it's waiting
    if (render_event_) {
        SetEvent(render_event_);
    }
    
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

    // Push-model submission is no longer required; AudioPipeline feeds the device FIFO directly.
    return AudioOutputError::Success;
}

// submit_data method removed - now using direct FIFO writes

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

void AudioOutput::set_render_callback(std::function<size_t(void* buffer, uint32_t frame_count, SampleFormat format, uint16_t channels)> callback) {
    render_callback_ = callback;
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

    // Store original requested configuration for comparison
    uint32_t requested_sample_rate = config_.sample_rate;
    uint32_t requested_channels = config_.channel_count;
    
    // Create a WAVEFORMATEX for our requested format to test device support
    WAVEFORMATEX requested_format = {};
    requested_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    requested_format.nChannels = static_cast<WORD>(config_.channel_count);
    requested_format.nSamplesPerSec = config_.sample_rate;
    requested_format.wBitsPerSample = 32; // Float32
    requested_format.nBlockAlign = static_cast<WORD>((requested_format.wBitsPerSample / 8) * requested_format.nChannels);
    requested_format.nAvgBytesPerSec = requested_format.nSamplesPerSec * requested_format.nBlockAlign;
    requested_format.cbSize = 0;
    
    // Test if device supports our requested format
    WAVEFORMATEX* closest_match = nullptr;
    hr = audio_client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &requested_format, &closest_match);
    
    if (hr == S_OK) {
        // Device supports our exact format - keep our configuration
        ve::log::info("WASAPI device supports requested format: " + std::to_string(config_.sample_rate) + " Hz, " + 
                     std::to_string(config_.channel_count) + " channels");
        config_.format = SampleFormat::Float32; // We always prefer Float32
    } else if (hr == S_FALSE && closest_match) {
        // Device suggests a close format - use it for better compatibility
        config_.sample_rate = closest_match->nSamplesPerSec;
        config_.channel_count = closest_match->nChannels;
        
        if (closest_match->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            config_.format = SampleFormat::Float32;
        } else if (closest_match->wFormatTag == WAVE_FORMAT_PCM) {
            if (closest_match->wBitsPerSample == 16) {
                config_.format = SampleFormat::Int16;
            } else if (closest_match->wBitsPerSample == 32) {
                config_.format = SampleFormat::Int32;
            }
        }
        
        ve::log::warn("WASAPI device suggested different format: " + std::to_string(config_.sample_rate) + " Hz, " + 
                     std::to_string(config_.channel_count) + " channels (requested: " + 
                     std::to_string(requested_sample_rate) + " Hz, " + std::to_string(requested_channels) + " channels)");
        CoTaskMemFree(closest_match);
    } else {
        // Device doesn't support our format - fall back to device mix format
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
        
        ve::log::warn("WASAPI device rejected requested format, using device mix format: " + 
                     std::to_string(config_.sample_rate) + " Hz, " + std::to_string(config_.channel_count) + 
                     " channels (requested: " + std::to_string(requested_sample_rate) + " Hz, " + 
                     std::to_string(requested_channels) + " channels)");
    }
    
    // Log the negotiation result
    if (config_.sample_rate != requested_sample_rate || config_.channel_count != requested_channels) {
        ve::log::info("Audio format negotiation: video=" + std::to_string(requested_sample_rate) + "Hz/" + 
                     std::to_string(requested_channels) + "ch → device=" + std::to_string(config_.sample_rate) + 
                     "Hz/" + std::to_string(config_.channel_count) + "ch (resampling will be applied)");
    } else {
        ve::log::info("Audio format negotiation: perfect match - no resampling needed");
    }

    // Calculate buffer sizes
    REFERENCE_TIME requested_duration = config_.buffer_duration_ms * 10000; // Convert to 100-nanosecond units
    REFERENCE_TIME min_periodicity = config_.min_periodicity_ms * 10000;

    // PHASE 2: Enable event-driven WASAPI for device-clock driven rendering
    DWORD stream_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    
    hr = audio_client_->Initialize(
        config_.exclusive_mode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
        stream_flags, // Enable event-driven rendering
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

    // PHASE 2: Create and set event handle for device-driven rendering
    if (!render_event_) {
        render_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!render_event_) {
            set_error(AudioOutputError::Unknown, "Failed to create render event: " + std::to_string(GetLastError()));
            return AudioOutputError::Unknown;
        }
    }
    
    hr = audio_client_->SetEventHandle(render_event_);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to set event handle: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    // Get buffer size
    hr = audio_client_->GetBufferSize(&buffer_frame_count_);
    if (FAILED(hr)) {
        set_error(AudioOutputError::Unknown, "Failed to get buffer size: " + std::to_string(hr));
        return AudioOutputError::Unknown;
    }

    // Log negotiated buffer frame count once for sensitivity correlation
    ve::log::info("WASAPI_BUFFER_NEGOTIATED: buffer_frame_count=" + std::to_string(buffer_frame_count_));

    // PHASE 1 DIAGNOSTIC: Get device period and quantum information
    REFERENCE_TIME default_device_period = 0;
    REFERENCE_TIME minimum_device_period = 0;
    hr = audio_client_->GetDevicePeriod(&default_device_period, &minimum_device_period);
    if (SUCCEEDED(hr)) {
        // Convert 100-nanosecond units to milliseconds and frames
        double default_period_ms = default_device_period / 10000.0;
        double minimum_period_ms = minimum_device_period / 10000.0;
        UINT32 default_period_frames = (UINT32)(config_.sample_rate * default_period_ms / 1000.0);
        UINT32 minimum_period_frames = (UINT32)(config_.sample_rate * minimum_period_ms / 1000.0);
        
        ve::log::info("PHASE1_DEVICE_INFO: buffer_frame_count=" + std::to_string(buffer_frame_count_) +
                     ", default_device_period_ms=" + std::to_string(default_period_ms) +
                     ", minimum_device_period_ms=" + std::to_string(minimum_period_ms) +
                     ", default_period_frames=" + std::to_string(default_period_frames) +
                     ", minimum_period_frames=" + std::to_string(minimum_period_frames) +
                     ", sample_rate=" + std::to_string(config_.sample_rate) +
                     ", channel_count=" + std::to_string(config_.channel_count));
    } else {
        ve::log::warn("PHASE1_DEVICE_INFO: Failed to get device period: " + std::to_string(hr));
    }

    if (audio_clock_) {
        audio_clock_->Release();
        audio_clock_ = nullptr;
    }

    UINT64 freq = 0;
    hr = audio_client_->GetService(__uuidof(IAudioClock), (void**)&audio_clock_);
    if (SUCCEEDED(hr) && audio_clock_) {
        if (SUCCEEDED(audio_clock_->GetFrequency(&freq)) && freq > 0) {
            audio_clock_frequency_ = static_cast<double>(freq);
            clock_running_.store(false, std::memory_order_release);
            clock_start_position_.store(0, std::memory_order_relaxed);
            clock_last_position_.store(0, std::memory_order_relaxed);
        } else {
            ve::log::warn("AudioOutput: IAudioClock returned invalid frequency, disabling device clock telemetry");
            audio_clock_->Release();
            audio_clock_ = nullptr;
            audio_clock_frequency_ = 0.0;
        }
    } else {
        ve::log::warn("AudioOutput: Failed to acquire IAudioClock (hr=" + std::to_string(hr) + ")");
        audio_clock_ = nullptr;
        audio_clock_frequency_ = 0.0;
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

    ve::log::info("PHASE2_RENDER_THREAD: Event-driven render thread started with MMCSS priority");

    while (!thread_should_exit_) {
        // PHASE 2: Wait for device event (device-clock driven timing)
        if (render_event_) {
            DWORD wait_result = WaitForSingleObject(render_event_, INFINITE);
            if (wait_result != WAIT_OBJECT_0 || thread_should_exit_) {
                break;
            }
        } else {
            // Fallback if no event (shouldn't happen with event-driven mode)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // PHASE 2: Device-driven audio rendering 
        if (!audio_client_ || !render_client_) {
            continue;
        }

        // Query padding and compute how much we should write this event
        UINT32 padding_frames = 0;
        HRESULT hr = audio_client_->GetCurrentPadding(&padding_frames);
        if (FAILED(hr)) {
            continue;
        }

        UINT32 available_frames = buffer_frame_count_ - padding_frames;
        if (available_frames == 0) {
            continue; // buffer full, wait next event
        }

        // Use actual device period; fall back to 10ms if unknown
        const double period_seconds = min_periodicity_ / 10000000.0; // convert REFERENCE_TIME to seconds
        const UINT32 period_frames = std::max<UINT32>(1u, (UINT32)std::round(config_.sample_rate * std::max(0.010, period_seconds)));
        const UINT32 target_frames = std::min<UINT32>(buffer_frame_count_, period_frames * 3); // keep ~30ms in device

        UINT32 want = 0;
        if (padding_frames < target_frames) {
            // top-up toward target this event
            want = std::min<UINT32>(available_frames, target_frames - padding_frames);
        }
        // Always write at least one period if we can
        want = std::max<UINT32>(want, std::min<UINT32>(available_frames, period_frames));
        if (want == 0) {
            continue;
        }

        // === Direct write path (no extra copies, no second GetBuffer) ===
        BYTE* pData = nullptr;
        hr = render_client_->GetBuffer(want, &pData);
        if (FAILED(hr) || !pData) {
            handle_underrun();
            continue;
        }

        // Ask the pipeline to fill exactly 'want' frames into pData
        size_t frames_filled = 0;
        if (render_callback_) {
            frames_filled = render_callback_(pData, want, config_.format, config_.channel_count);
        }
        if (frames_filled < want) {
            // Zero any remainder
            const size_t bpf = config_.channel_count * ((config_.format == SampleFormat::Float32 || config_.format == SampleFormat::Int32) ? 4 : 2);
            std::memset(pData + frames_filled * bpf, 0, (want - (UINT32)frames_filled) * bpf);
        }

        // Release exactly what we wrote
        hr = render_client_->ReleaseBuffer((UINT32)want, 0);
        if (FAILED(hr)) {
            set_error(AudioOutputError::Unknown, "ReleaseBuffer failed");
            continue;
        }
        
        // Light logging every ~5s
        static auto last_log = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 5) {
            ve::log::info("PHASE2_DEVICE_DRIVEN: wrote=" + std::to_string(want) +
                          " padding_before=" + std::to_string(padding_frames) +
                          " target=" + std::to_string(target_frames) +
                          " avail=" + std::to_string(available_frames));
            last_log = now;
        }
    }

    ve::log::info("PHASE2_RENDER_THREAD: Event-driven render thread exiting");

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

double AudioOutput::device_clock_seconds() const {
#ifdef _WIN32
    if (!audio_clock_ || audio_clock_frequency_ <= 0.0) {
        return -1.0;
    }

    std::lock_guard<std::mutex> lock(clock_mutex_);
    if (!clock_running_.load(std::memory_order_acquire)) {
        return -1.0;
    }

    UINT64 position = 0;
    UINT64 qpc = 0;
    if (FAILED(audio_clock_->GetPosition(&position, &qpc))) {
        return -1.0;
    }

    UINT64 start = clock_start_position_.load(std::memory_order_acquire);
    if (position < start) {
        start = position;
        clock_start_position_.store(position, std::memory_order_release);
    }
    clock_last_position_.store(position, std::memory_order_release);
    const double seconds = static_cast<double>(position - start) / audio_clock_frequency_;
    return seconds;
#else
    return -1.0;
#endif
}

double AudioOutput::device_playhead_seconds() const {
#ifdef _WIN32
    const double clock_sec = device_clock_seconds();
    if (!std::isfinite(clock_sec) || clock_sec < 0.0) {
        return -1.0;
    }
    return clock_sec;
#else
    return -1.0;
#endif
}

bool AudioOutput::get_buffer_metrics(uint32_t& padding_frames, uint32_t& buffer_frames) const {
#ifdef _WIN32
    if (!audio_client_) {
        return false;
    }
    UINT32 padding = 0;
    HRESULT hr = audio_client_->GetCurrentPadding(&padding);
    if (FAILED(hr)) {
        return false;
    }
    padding_frames = padding;
    buffer_frames = buffer_frame_count_;
    return true;
#else
    (void)padding_frames;
    (void)buffer_frames;
    return false;
#endif
}

double AudioOutput::buffer_fill_percent() const {
    uint32_t padding = 0;
    uint32_t total = 0;
    if (!get_buffer_metrics(padding, total) || total == 0) {
        return 0.0;
    }
    double percent = static_cast<double>(padding) / static_cast<double>(total) * 100.0;
    if (!std::isfinite(percent)) {
        return 0.0;
    }
    return std::clamp(percent, 0.0, 100.0);
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
