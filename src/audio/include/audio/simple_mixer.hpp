/**
 * @file simple_mixer.hpp
 * @brief Simple Audio Mixer Core for Basic Multi-Track Mixing
 *
 * Provides fundamental audio mixing capabilities:
 * - Multi-track audio summing with overflow protection
 * - Per-track gain control (-∞ to +12dB range)
 * - Stereo panning control (left-right field positioning)
 * - Master volume and mute controls
 * - Thread-safe operations for real-time audio processing
 */

#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include <cstdint>

#include "audio/audio_frame.hpp"
#include "core/time.hpp"

namespace ve::audio {

/**
 * @brief Mixer channel/track configuration
 */
struct MixerChannel {
    uint32_t id = 0;                    ///< Unique channel identifier
    std::string name;                   ///< Human-readable channel name
    float gain_db = 0.0f;               ///< Gain in decibels (-∞ to +12dB)
    float pan = 0.0f;                  ///< Stereo pan (-1.0 = full left, +1.0 = full right)
    bool muted = false;                 ///< Channel mute state
    bool solo = false;                  ///< Channel solo state
    uint64_t samples_processed = 0;     ///< Statistics: samples processed

    /**
     * @brief Check if channel is valid
     */
    bool is_valid() const {
        return id != 0 && !name.empty();
    }

    /**
     * @brief Get linear gain from dB
     */
    float get_linear_gain() const {
        if (gain_db <= -60.0f) return 0.0f; // Effectively silent
        return std::pow(10.0f, gain_db / 20.0f);
    }
};

/**
 * @brief Simple mixer configuration
 */
struct SimpleMixerConfig {
    uint32_t sample_rate = 48000;       ///< Target sample rate (Hz)
    uint16_t channel_count = 2;         ///< Output channel count (stereo)
    SampleFormat format = SampleFormat::Float32;  ///< Sample format
    uint32_t max_channels = 16;         ///< Maximum number of input channels
    bool enable_clipping_protection = true;  ///< Enable soft clipping
    float master_volume_db = 0.0f;      ///< Master volume in dB
    bool master_mute = false;           ///< Master mute state
};

/**
 * @brief Mixer processing statistics
 */
struct MixerStats {
    uint64_t total_samples_processed = 0;  ///< Total samples processed
    uint64_t clipping_events = 0;          ///< Number of clipping events
    float peak_level_left = 0.0f;          ///< Peak level for left channel
    float peak_level_right = 0.0f;         ///< Peak level for right channel
    float rms_level_left = 0.0f;           ///< RMS level for left channel
    float rms_level_right = 0.0f;          ///< RMS level for right channel
    uint32_t active_channels = 0;          ///< Number of active channels
    double cpu_usage_percent = 0.0f;       ///< CPU usage percentage
};

/**
 * @brief Audio mixer error codes
 */
enum class MixerError {
    Success,
    InvalidChannel,
    ChannelNotFound,
    TooManyChannels,
    InvalidConfiguration,
    BufferTooSmall,
    FormatMismatch,
    NotInitialized,
    Unknown
};

/**
 * @brief Simple Audio Mixer for Multi-Track Mixing
 *
 * Core mixing functionality providing:
 * - Multi-track audio summing with proper level management
 * - Per-track gain control and stereo panning
 * - Master volume and mute controls
 * - Solo/mute functionality for individual tracks
 * - Soft clipping protection to prevent distortion
 * - Real-time statistics and monitoring
 * - Thread-safe operations for concurrent access
 */
class SimpleMixer {
public:
    /**
     * @brief Create mixer instance
     *
     * @param config Mixer configuration
     * @return SimpleMixer instance or nullptr on failure
     */
    static std::unique_ptr<SimpleMixer> create(const SimpleMixerConfig& config = {});

    ~SimpleMixer();

    // Disable copy and move
    SimpleMixer(const SimpleMixer&) = delete;
    SimpleMixer& operator=(const SimpleMixer&) = delete;
    SimpleMixer(SimpleMixer&&) = delete;
    SimpleMixer& operator=(SimpleMixer&&) = delete;

    /**
     * @brief Initialize the mixer
     *
     * @return Success or error code
     */
    MixerError initialize();

    /**
     * @brief Shutdown the mixer
     */
    void shutdown();

    /**
     * @brief Check if mixer is initialized
     */
    bool is_initialized() const { return initialized_; }

    // Channel management

    /**
     * @brief Add a new mixer channel
     *
     * @param name Channel name
     * @param initial_gain_db Initial gain in dB
     * @param initial_pan Initial pan position (-1.0 to +1.0)
     * @return Channel ID or 0 on failure
     */
    uint32_t add_channel(const std::string& name,
                        float initial_gain_db = 0.0f,
                        float initial_pan = 0.0f);

    /**
     * @brief Remove a mixer channel
     *
     * @param channel_id Channel ID to remove
     * @return true if channel was found and removed
     */
    bool remove_channel(uint32_t channel_id);

    /**
     * @brief Get channel configuration
     *
     * @param channel_id Channel ID
     * @return Channel configuration or empty config if not found
     */
    MixerChannel get_channel(uint32_t channel_id) const;

    /**
     * @brief Update channel configuration
     *
     * @param channel Channel configuration to update
     * @return Success or error code
     */
    MixerError update_channel(const MixerChannel& channel);

    /**
     * @brief Get all mixer channels
     */
    std::vector<MixerChannel> get_all_channels() const;

    /**
     * @brief Get number of active channels
     */
    uint32_t get_channel_count() const;

    // Audio processing

    /**
     * @brief Process audio for a single channel
     *
     * @param channel_id Target channel ID
     * @param input Input audio frame
     * @return Success or error code
     */
    MixerError process_channel(uint32_t channel_id, std::shared_ptr<AudioFrame> input);

    /**
     * @brief Mix all active channels to output buffer
     *
     * @param output Output audio frame (will be filled with mixed result)
     * @param clear_accumulator Clear the internal accumulator after mixing
     * @return Success or error code
     */
    MixerError mix_to_output(std::shared_ptr<AudioFrame>& output, bool clear_accumulator = true);

    /**
     * @brief Mix all active channels and return new output frame
     *
     * @param frame_count Number of frames to generate
     * @param timestamp Output frame timestamp
     * @return Mixed audio frame or nullptr on error
     */
    std::shared_ptr<AudioFrame> mix_channels(uint32_t frame_count,
                                            const TimePoint& timestamp = TimePoint(0, 1));

    /**
     * @brief Clear the internal mixing accumulator
     */
    void clear_accumulator();

    // Master controls

    /**
     * @brief Set master volume
     *
     * @param volume_db Master volume in dB (-∞ to +12dB)
     * @return Success or error code
     */
    MixerError set_master_volume(float volume_db);

    /**
     * @brief Get master volume
     */
    float get_master_volume() const { return config_.master_volume_db; }

    /**
     * @brief Set master mute state
     */
    MixerError set_master_mute(bool muted);

    /**
     * @brief Get master mute state
     */
    bool is_master_muted() const { return config_.master_mute; }

    /**
     * @brief Get linear master gain
     */
    float get_master_linear_gain() const;

    // Solo/Mute controls

    /**
     * @brief Set channel solo state
     *
     * @param channel_id Channel ID
     * @param solo Solo state
     * @return Success or error code
     */
    MixerError set_channel_solo(uint32_t channel_id, bool solo);

    /**
     * @brief Set channel mute state
     *
     * @param channel_id Channel ID
     * @param muted Mute state
     * @return Success or error code
     */
    MixerError set_channel_mute(uint32_t channel_id, bool muted);

    /**
     * @brief Set channel gain
     *
     * @param channel_id Channel ID
     * @param gain_db Gain in dB
     * @return Success or error code
     */
    MixerError set_channel_gain(uint32_t channel_id, float gain_db);

    /**
     * @brief Set channel pan
     *
     * @param channel_id Channel ID
     * @param pan Pan position (-1.0 to +1.0)
     * @return Success or error code
     */
    MixerError set_channel_pan(uint32_t channel_id, float pan);

    // Statistics and monitoring

    /**
     * @brief Get mixer statistics
     */
    MixerStats get_stats() const;

    /**
     * @brief Reset statistics
     */
    void reset_stats();

    /**
     * @brief Get mixer configuration
     */
    const SimpleMixerConfig& get_config() const { return config_; }

    // Error handling

    /**
     * @brief Get last error message
     */
    std::string get_last_error() const;

    /**
     * @brief Clear error state
     */
    void clear_error();

    // Utility functions

    /**
     * @brief Convert dB to linear gain
     */
    static float db_to_linear(float db);

    /**
     * @brief Convert linear gain to dB
     */
    static float linear_to_db(float linear);

    /**
     * @brief Apply soft clipping to prevent distortion
     */
    static float soft_clip(float sample, float threshold = 0.9f);

private:
    /**
     * @brief Private constructor
     */
    SimpleMixer(const SimpleMixerConfig& config);

    /**
     * @brief Find channel by ID
     */
    MixerChannel* find_channel(uint32_t channel_id);
    const MixerChannel* find_channel(uint32_t channel_id) const;

    /**
     * @brief Check if any channel is soloed
     */
    bool has_solo_channels() const;

    /**
     * @brief Apply panning to stereo channels
     */
    void apply_panning(float left_input, float right_input, float pan,
                      float& left_output, float& right_output);

    /**
     * @brief Update statistics
     */
    void update_stats(const std::vector<float>& mixed_buffer);

    /**
     * @brief Set error with message
     */
    void set_error(MixerError error, const std::string& message);

    // Configuration
    SimpleMixerConfig config_;
    bool initialized_ = false;

    // Channel management
    std::vector<MixerChannel> channels_;
    mutable std::mutex channels_mutex_;
    std::atomic<uint32_t> next_channel_id_{1};

    // Mixing accumulator (internal buffer for summing)
    std::vector<float> accumulator_;
    mutable std::mutex accumulator_mutex_;

    // Statistics
    mutable std::mutex stats_mutex_;
    MixerStats stats_;

    // Error handling
    mutable std::mutex error_mutex_;
    MixerError last_error_ = MixerError::Success;
    std::string last_error_message_;
};

} // namespace ve::audio