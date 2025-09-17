/**
 * @file audio_output.hpp
 * @brief WASAPI Audio Output Backend for Windows
 *
 * Professional audio output implementation using Windows Audio Session API (WASAPI).
 * Provides low-latency audio output with device enumeration and format conversion.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <functional>
#include <cstdint>

#include "audio/audio_frame.hpp"
#include "core/time.hpp"

#ifdef _WIN32
// Windows Audio Headers
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#endif

namespace ve::audio {

/**
 * @brief Audio device information
 */
struct AudioDeviceInfo {
    std::string id;           ///< Device ID
    std::string name;         ///< Friendly device name
    std::string description;  ///< Device description
    bool is_default = false;  ///< Is default device
    bool is_input = false;    ///< Is input device (vs output)
    uint32_t sample_rate = 0; ///< Preferred sample rate
    uint16_t channels = 0;    ///< Channel count
};

/**
 * @brief Audio output configuration
 */
struct AudioOutputConfig {
    uint32_t sample_rate = 48000;        ///< Target sample rate (Hz)
    uint16_t channel_count = 2;          ///< Target channel count
    SampleFormat format = SampleFormat::Float32; ///< Audio format
    std::string device_id;               ///< Device ID (empty for default)
    bool exclusive_mode = false;         ///< Use exclusive mode
    uint32_t buffer_duration_ms = 20;    ///< Buffer duration in milliseconds
    uint32_t min_periodicity_ms = 3;     ///< Minimum periodicity in milliseconds
};

/**
 * @brief Audio output statistics
 */
struct AudioOutputStats {
    uint64_t frames_rendered = 0;        ///< Total frames rendered
    uint64_t buffer_underruns = 0;       ///< Number of buffer underruns
    uint32_t buffer_size_frames = 0;     ///< Current buffer size in frames
    double cpu_usage_percent = 0.0;      ///< CPU usage percentage
    TimePoint last_render_time;          ///< Time of last render
};

/**
 * @brief Audio output error codes
 */
enum class AudioOutputError {
    Success = 0,
    NotInitialized = 1,
    DeviceNotFound = 2,
    FormatNotSupported = 3,
    BufferTooSmall = 4,
    ExclusiveModeFailed = 5,
    InvalidState = 6,
    HardwareOffloadFailed = 7,
    ThreadError = 8,
    Unknown = 9
};

/**
 * @brief WASAPI Audio Output Backend
 *
 * Professional audio output implementation for Windows using WASAPI.
 * Supports both shared and exclusive modes with low-latency operation.
 */
class AudioOutput {
public:
    AudioOutput();
    explicit AudioOutput(const AudioOutputConfig& config);
    ~AudioOutput();

    // Prevent copying
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // Initialization and configuration
    AudioOutputError initialize();
    void shutdown();
    bool is_initialized() const { return initialized_; }

    // Audio rendering
    AudioOutputError start();
    AudioOutputError stop();
    bool is_running() const { return running_; }

    // Audio data submission
    AudioOutputError submit_frame(std::shared_ptr<AudioFrame> frame);
    AudioOutputError submit_data(const float* data, size_t frame_count);
    AudioOutputError submit_data(const void* data, uint32_t frame_count, const TimePoint& timestamp);
    AudioOutputError flush();

    // Factory method
    static std::unique_ptr<AudioOutput> create(const AudioOutputConfig& config);

    // Device management
    static std::vector<AudioDeviceInfo> enumerate_devices(bool include_inputs = false);
    static AudioDeviceInfo get_default_device();
    static AudioDeviceInfo get_device_by_id(const std::string& device_id);

    // Configuration
    AudioOutputError set_config(const AudioOutputConfig& config);
    const AudioOutputConfig& get_config() const { return config_; }

    // Control
    AudioOutputError set_volume(float volume);
    AudioOutputError set_muted(bool muted);

    // Statistics and monitoring
    AudioOutputStats get_stats() const;

    // Error handling
    AudioOutputError get_last_error_code() const { return last_error_; }
    std::string get_last_error() const;
    void clear_error();

    // Callbacks
    void set_underrun_callback(std::function<void()> callback);
    void set_device_change_callback(std::function<void(const std::string&)> callback);

private:
    // Configuration
    AudioOutputConfig config_;
    bool initialized_ = false;
    bool running_ = false;
    bool playing_ = false;

    // Audio control
    float volume_ = 1.0f;
    bool muted_ = false;

    // Audio rendering thread
    std::thread render_thread_;
    std::atomic<bool> thread_should_exit_{false};

#ifdef _WIN32
    // WASAPI components
    IMMDeviceEnumerator* device_enumerator_ = nullptr;
    IMMDevice* audio_device_ = nullptr;
    IAudioClient* audio_client_ = nullptr;
    IAudioRenderClient* render_client_ = nullptr;
    HANDLE render_event_ = nullptr;

    // Buffer management
    uint32_t buffer_frame_count_ = 0;
    REFERENCE_TIME buffer_duration_ = 0;
    REFERENCE_TIME min_periodicity_ = 0;

    // Statistics
    mutable std::mutex stats_mutex_;
    AudioOutputStats stats_;

    // PTS-based deduplication for echo prevention
    std::unordered_set<int64_t> submitted_pts_;
    std::mutex pts_mutex_;

    // Error handling
    mutable std::mutex error_mutex_;
    AudioOutputError last_error_ = AudioOutputError::Success;
    std::string last_error_message_;

    // Callbacks
    std::function<void()> underrun_callback_;
    std::function<void(const std::string&)> device_change_callback_;

    // WASAPI implementation
    AudioOutputError initialize_wasapi();
    AudioOutputError create_audio_client();
    AudioOutputError initialize_format();
    AudioOutputError create_render_client();

    // Helper functions
    static std::string get_device_id(IMMDevice* device);
    static std::string get_device_name(IMMDevice* device);

    // Render thread
    void render_thread_function();
    void handle_underrun();
    void update_stats();
#endif

    // Error handling
    void set_error(AudioOutputError error, const std::string& message);
};

} // namespace ve::audio
