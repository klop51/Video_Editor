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
    SampleFormat format = SampleFormat::Float32;  ///< Sample format
    uint32_t buffer_duration_ms = 20;    ///< Buffer duration in milliseconds
    uint32_t min_periodicity_ms = 3;     ///< Minimum periodicity in milliseconds
    bool exclusive_mode = false;         ///< Use exclusive mode (low latency)
    bool enable_hardware_offload = false; ///< Enable hardware offload if available
    std::string device_id;               ///< Specific device ID (empty = default)
};

/**
 * @brief Audio output statistics
 */
struct AudioOutputStats {
    uint64_t frames_rendered = 0;        ///< Total frames rendered
    uint64_t buffer_underruns = 0;       ///< Buffer underrun count
    uint64_t buffer_overruns = 0;        ///< Buffer overrun count
    double avg_latency_ms = 0.0;         ///< Average latency in milliseconds
    double cpu_usage_percent = 0.0;      ///< CPU usage percentage
    uint32_t buffer_size_frames = 0;     ///< Current buffer size in frames
};

/**
 * @brief Audio output error codes
 */
enum class AudioOutputError {
    Success,
    NotInitialized,
    DeviceNotFound,
    FormatNotSupported,
    BufferTooSmall,
    ExclusiveModeFailed,
    HardwareOffloadFailed,
    ThreadError,
    InvalidState,
    Unknown
};

/**
 * @brief WASAPI Audio Output Backend
 *
 * Professional audio output implementation providing:
 * - WASAPI exclusive/shared mode support
 * - Low-latency audio rendering
 * - Device enumeration and selection
 * - Automatic format conversion
 * - Buffer management and underrun detection
 * - Thread-safe operations
 */
class AudioOutput {
public:
    /**
     * @brief Create audio output instance
     *
     * @param config Output configuration
     * @return AudioOutput instance or nullptr on failure
     */
    static std::unique_ptr<AudioOutput> create(const AudioOutputConfig& config = {});

    ~AudioOutput();

    // Disable copy and move
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;

    /**
     * @brief Initialize audio output
     *
     * @return Success or error code
     */
    AudioOutputError initialize();

    /**
     * @brief Shutdown audio output
     */
    void shutdown();

    /**
     * @brief Check if output is initialized
     */
    bool is_initialized() const { return initialized_; }

    /**
     * @brief Start audio playback
     *
     * @return Success or error code
     */
    AudioOutputError start();

    /**
     * @brief Stop audio playback
     *
     * @return Success or error code
     */
    AudioOutputError stop();

    /**
     * @brief Check if currently playing
     */
    bool is_playing() const { return playing_; }

    /**
     * @brief Submit audio frame for playback
     *
     * @param frame Audio frame to render
     * @return Success or error code
     */
    AudioOutputError submit_frame(std::shared_ptr<AudioFrame> frame);

    /**
     * @brief Submit raw audio data for playback
     *
     * @param data Raw audio data (interleaved)
     * @param frame_count Number of frames
     * @param timestamp Frame timestamp
     * @return Success or error code
     */
    AudioOutputError submit_data(const void* data, uint32_t frame_count,
                                const TimePoint& timestamp = TimePoint(0, 1));

    /**
     * @brief Flush audio buffers
     */
    AudioOutputError flush();

    /**
     * @brief Set master volume (0.0 to 1.0)
     */
    AudioOutputError set_volume(float volume);

    /**
     * @brief Get current master volume
     */
    float get_volume() const { return volume_; }

    /**
     * @brief Set mute state
     */
    AudioOutputError set_muted(bool muted);

    /**
     * @brief Get current mute state
     */
    bool is_muted() const { return muted_; }

    /**
     * @brief Get current output statistics
     */
    AudioOutputStats get_stats() const;

    /**
     * @brief Get output configuration
     */
    const AudioOutputConfig& get_config() const { return config_; }

    /**
     * @brief Update output configuration (requires restart)
     */
    AudioOutputError set_config(const AudioOutputConfig& config);

    // Device enumeration

    /**
     * @brief Enumerate available audio devices
     *
     * @param include_inputs Include input devices in enumeration
     * @return Vector of available devices
     */
    static std::vector<AudioDeviceInfo> enumerate_devices(bool include_inputs = false);

    /**
     * @brief Get default output device
     */
    static AudioDeviceInfo get_default_device();

    /**
     * @brief Get device by ID
     */
    static AudioDeviceInfo get_device_by_id(const std::string& device_id);

    // Error handling

    /**
     * @brief Get last error message
     */
    std::string get_last_error() const;

    /**
     * @brief Clear error state
     */
    void clear_error();

    // Callback interface

    /**
     * @brief Set callback for buffer underrun events
     */
    void set_underrun_callback(std::function<void()> callback);

    /**
     * @brief Set callback for device change events
     */
    void set_device_change_callback(std::function<void(const std::string& device_id)> callback);

private:
    /**
     * @brief Private constructor
     */
    AudioOutput(const AudioOutputConfig& config);

    /**
     * @brief Initialize WASAPI components
     */
    AudioOutputError initialize_wasapi();

    /**
     * @brief Create audio client
     */
    AudioOutputError create_audio_client();

    /**
     * @brief Initialize audio format
     */
    AudioOutputError initialize_format();

    /**
     * @brief Create render client
     */
    AudioOutputError create_render_client();

    /**
     * @brief Audio rendering thread function
     */
    void render_thread_function();

    /**
     * @brief Handle buffer underrun
     */
    void handle_underrun();

    /**
     * @brief Update statistics
     */
    void update_stats();

    /**
     * @brief Set error with message
     */
    void set_error(AudioOutputError error, const std::string& message);

    // Configuration
    AudioOutputConfig config_;
    bool initialized_ = false;
    bool playing_ = false;

    // State
    float volume_ = 1.0f;
    bool muted_ = false;

    // WASAPI components
#ifdef _WIN32
    IMMDeviceEnumerator* device_enumerator_ = nullptr;
    IMMDevice* audio_device_ = nullptr;
    IAudioClient* audio_client_ = nullptr;
    IAudioRenderClient* render_client_ = nullptr;
    HANDLE render_event_ = nullptr;
#endif

    // Threading
    std::thread render_thread_;
    std::atomic<bool> thread_should_exit_{false};
    std::mutex state_mutex_;

    // Buffer management
    uint32_t buffer_frame_count_ = 0;
    REFERENCE_TIME buffer_duration_ = 0;
    REFERENCE_TIME min_periodicity_ = 0;

    // Statistics
    mutable std::mutex stats_mutex_;
    AudioOutputStats stats_;

    // Error handling
    mutable std::mutex error_mutex_;
    AudioOutputError last_error_ = AudioOutputError::Success;
    std::string last_error_message_;

    // Callbacks
    std::function<void()> underrun_callback_;
    std::function<void(const std::string&)> device_change_callback_;
};

} // namespace ve::audio</content>
<parameter name="filePath">c:\Users\braul\Desktop\Video_Editor\src\audio\include\audio\audio_output.hpp
