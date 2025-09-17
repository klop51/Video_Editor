/**
 * @file audio_engine.hpp
 * @brief Phase 2 Week 1: Basic Audio Pipeline - Professional Audio Engine
 * 
 * Foundational audio engine providing:
 * - Audio loading and format detection
 * - Basic playback infrastructure (play/pause/stop/seek)
 * - Timeline integration for synchronized playback
 * - Multi-format support through decoder factory
 * - Thread-safe operations and error handling
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <functional>
#include <thread>

#include "core/time.hpp"
#include "audio/audio_frame.hpp"
#include "audio/decoder.hpp"
#include "audio/audio_clock.hpp"
#include "audio/mixing_graph.hpp"

namespace ve::audio {

/**
 * @brief Audio engine state enumeration
 */
enum class AudioEngineState {
    Uninitialized,  ///< Engine not initialized
    Stopped,        ///< Engine initialized but not playing
    Playing,        ///< Currently playing audio
    Paused,         ///< Playback paused
    Seeking,        ///< Seeking to new position
    Error           ///< Error state
};

/**
 * @brief Audio engine configuration
 */
struct AudioEngineConfig {
    uint32_t sample_rate = 48000;           ///< Output sample rate (Hz)
    uint16_t channel_count = 2;             ///< Output channel count
    SampleFormat output_format = SampleFormat::Float32;  ///< Output sample format
    uint32_t buffer_size = 512;             ///< Audio buffer size (samples)
    uint32_t max_concurrent_decoders = 8;   ///< Max simultaneous decoders
    bool enable_hardware_acceleration = true; ///< Enable hardware decoding if available
    bool enable_simd_optimization = true;   ///< Enable SIMD optimizations
    uint32_t max_loaded_sources = 64;       ///< Max audio sources in memory
};

/**
 * @brief Audio source handle for loaded audio files
 */
using AudioSourceID = uint32_t;
constexpr AudioSourceID INVALID_AUDIO_SOURCE_ID = 0;

/**
 * @brief Audio source information
 */
struct AudioSourceInfo {
    AudioSourceID id = INVALID_AUDIO_SOURCE_ID;
    std::string file_path;
    AudioStreamInfo stream_info;
    bool is_loaded = false;
    bool hardware_accelerated = false;
    TimeDuration duration;
    
    bool is_valid() const {
        return id != INVALID_AUDIO_SOURCE_ID && 
               stream_info.is_valid() && 
               is_loaded;
    }
};

/**
 * @brief Playback state information
 */
struct PlaybackState {
    AudioEngineState state = AudioEngineState::Uninitialized;
    TimePoint current_position{0, 1};       ///< Current playback position
    TimeDuration duration{0, 1};               ///< Total duration
    float volume = 1.0f;                    ///< Master volume (0.0-1.0)
    bool muted = false;                     ///< Master mute state
    uint32_t active_sources = 0;            ///< Number of active audio sources
    uint32_t buffer_underruns = 0;          ///< Buffer underrun count
    float cpu_usage = 0.0f;                 ///< CPU usage percentage
};

/**
 * @brief Audio engine callback interface for notifications
 */
class AudioEngineCallback {
public:
    virtual ~AudioEngineCallback() = default;
    
    /**
     * @brief Called when playback state changes
     */
    virtual void on_state_changed(AudioEngineState old_state, AudioEngineState new_state) {}
    
    /**
     * @brief Called when playback position updates
     */
    virtual void on_position_changed(const TimePoint& position) {}
    
    /**
     * @brief Called when an error occurs
     */
    virtual void on_error(const std::string& error_message) {}
    
    /**
     * @brief Called when audio source is loaded
     */
    virtual void on_source_loaded(AudioSourceID source_id, const AudioSourceInfo& info) {}
    
    /**
     * @brief Called when buffer underrun occurs
     */
    virtual void on_buffer_underrun() {}
};

/**
 * @brief Professional Audio Engine - Phase 2 Week 1 Foundation
 * 
 * Core audio engine providing:
 * - Multi-format audio loading and playback
 * - Timeline integration for synchronized playback
 * - Professional playback controls (play/pause/stop/seek)
 * - Thread-safe operations with callback notifications
 * - Integration with mixing graph architecture
 * 
 * Architecture:
 * AudioEngine → MixingGraph → [Decoder + Effects] → AudioOutput
 */
class AudioEngine {
public:
    /**
     * @brief Construct audio engine with default configuration
     */
    AudioEngine();
    
    /**
     * @brief Construct audio engine with custom configuration
     */
    explicit AudioEngine(const AudioEngineConfig& config);
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~AudioEngine();

    // Core initialization and shutdown
    
    /**
     * @brief Initialize the audio engine
     * 
     * @return true on success, false on failure
     */
    bool initialize();
    
    /**
     * @brief Shutdown the audio engine
     * 
     * Stops playback, releases resources, and shuts down all subsystems.
     */
    void shutdown();
    
    /**
     * @brief Check if engine is initialized
     */
    bool is_initialized() const;

    // Audio source management
    
    /**
     * @brief Load audio source from file
     * 
     * @param file_path Path to audio file
     * @return Audio source ID, or INVALID_AUDIO_SOURCE_ID on failure
     */
    AudioSourceID load_audio_source(const std::string& file_path);
    
    /**
     * @brief Unload audio source
     * 
     * @param source_id Audio source ID to unload
     * @return true if source was found and unloaded
     */
    bool unload_audio_source(AudioSourceID source_id);
    
    /**
     * @brief Get audio source information
     * 
     * @param source_id Audio source ID
     * @return Source information, or empty info if not found
     */
    AudioSourceInfo get_source_info(AudioSourceID source_id) const;
    
    /**
     * @brief Get all loaded audio sources
     */
    std::vector<AudioSourceInfo> get_loaded_sources() const;
    
    /**
     * @brief Check if audio source is loaded
     */
    bool is_source_loaded(AudioSourceID source_id) const;

    // Playback control
    
    /**
     * @brief Start playback
     * 
     * @return true on success, false if unable to start
     */
    bool play();
    
    /**
     * @brief Pause playback
     * 
     * @return true on success, false if not playing
     */
    bool pause();
    
    /**
     * @brief Stop playback and return to beginning
     * 
     * @return true on success
     */
    bool stop();
    
    /**
     * @brief Seek to specific position
     * 
     * @param position Target position in timeline
     * @return true on success, false if position invalid
     */
    bool seek(const TimePoint& position);
    
    /**
     * @brief Set master volume
     * 
     * @param volume Volume level (0.0 = silent, 1.0 = full)
     */
    void set_volume(float volume);
    
    /**
     * @brief Get current master volume
     */
    float get_volume() const;
    
    /**
     * @brief Set master mute state
     */
    void set_muted(bool muted);
    
    /**
     * @brief Get current mute state
     */
    bool is_muted() const;

    // State and information
    
    /**
     * @brief Get current playback state
     */
    PlaybackState get_playback_state() const;
    
    /**
     * @brief Get current playback position
     */
    TimePoint get_current_position() const;
    
    /**
     * @brief Get total duration of loaded content
     */
    TimeDuration get_duration() const;
    
    /**
     * @brief Get current engine state
     */
    AudioEngineState get_state() const;

    // Configuration and capabilities
    
    /**
     * @brief Get engine configuration
     */
    const AudioEngineConfig& get_config() const;
    
    /**
     * @brief Update engine configuration (requires restart)
     */
    bool set_config(const AudioEngineConfig& config);
    
    /**
     * @brief Get supported audio formats
     */
    std::vector<AudioCodec> get_supported_formats() const;
    
    /**
     * @brief Check if format is supported
     */
    bool is_format_supported(AudioCodec codec) const;

    // Callback management
    
    /**
     * @brief Register callback for engine notifications
     * 
     * @param callback Callback interface (weak reference)
     */
    void set_callback(AudioEngineCallback* callback);
    
    /**
     * @brief Remove callback
     */
    void clear_callback();

    // Integration interfaces
    
    /**
     * @brief Get mixing graph for advanced audio processing
     * 
     * @return Mixing graph instance, or nullptr if not available
     */
    MixingGraph* get_mixing_graph();
    
    /**
     * @brief Get audio clock for synchronization
     * 
     * @return Audio clock instance, or nullptr if not available
     */
    AudioClock* get_audio_clock();

    // Timeline integration
    
    /**
     * @brief Add audio source to timeline at specific position
     * 
     * @param source_id Audio source ID
     * @param start_time Start position in timeline
     * @param duration Duration to play (or full duration if zero)
     * @return true on success
     */
    bool add_source_to_timeline(AudioSourceID source_id, 
                                const TimePoint& start_time, 
                                const TimeDuration& duration = TimeDuration(0, 1));
    
    /**
     * @brief Remove audio source from timeline
     * 
     * @param source_id Audio source ID
     * @return true if source was in timeline and removed
     */
    bool remove_source_from_timeline(AudioSourceID source_id);
    
    /**
     * @brief Get audio sources active at specific time
     * 
     * @param time Timeline position
     * @return Vector of active source IDs
     */
    std::vector<AudioSourceID> get_active_sources_at_time(const TimePoint& time) const;

    // Error handling
    
    /**
     * @brief Get last error message
     */
    std::string get_last_error() const;
    
    /**
     * @brief Clear error state
     */
    void clear_error();

private:
    // Internal implementation
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    // State management
    mutable std::mutex state_mutex_;
    AudioEngineState current_state_;
    AudioEngineConfig config_;
    
    // Error handling
    mutable std::mutex error_mutex_;
    std::string last_error_;
    
    // Callback management
    std::mutex callback_mutex_;
    AudioEngineCallback* callback_;
    
    // Helper methods
    void set_state(AudioEngineState new_state);
    void set_error(const std::string& error);
    void notify_state_changed(AudioEngineState old_state, AudioEngineState new_state);
    void notify_position_changed(const TimePoint& position);
    void notify_error(const std::string& error);
    void notify_source_loaded(AudioSourceID source_id, const AudioSourceInfo& info);
    void notify_buffer_underrun();
    
    // Audio processing
    void audio_thread_loop();
    void process_audio_frame();
};

} // namespace ve::audio
