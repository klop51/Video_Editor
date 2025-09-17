#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <core/time.hpp>

namespace ve::audio {

/**
 * @brief Audio sample format enumeration
 * 
 * Defines the different sample formats supported by the audio engine.
 * All formats are native endian.
 */
enum class SampleFormat {
    Int16,      ///< 16-bit signed integer (-32768 to 32767)
    Int32,      ///< 32-bit signed integer (-2147483648 to 2147483647)
    Float32,    ///< 32-bit IEEE floating point (-1.0 to 1.0)
    Unknown     ///< Unknown or unsupported format
};

/**
 * @brief Audio channel layout enumeration
 * 
 * Defines common audio channel layouts for professional audio work.
 */
enum class ChannelLayout {
    Mono,           ///< 1 channel: M
    Stereo,         ///< 2 channels: L, R
    Stereo21,       ///< 3 channels: L, R, LFE
    Surround51,     ///< 6 channels: L, R, C, LFE, SL, SR
    Surround71,     ///< 8 channels: L, R, C, LFE, SL, SR, BL, BR
    Unknown         ///< Unknown or custom layout
};

/**
 * @brief Audio frame data container with reference counting
 * 
 * Represents a block of audio samples with associated metadata.
 * Uses reference counting for efficient memory management in 
 * multi-threaded audio processing pipelines.
 * 
 * Thread-safe for read operations when properly managed.
 */
class AudioFrame : public std::enable_shared_from_this<AudioFrame> {
public:
    /**
     * @brief Create an audio frame with specified parameters
     * 
     * @param sample_rate Sample rate in Hz (e.g., 48000)
     * @param channel_count Number of audio channels
     * @param sample_count Number of samples per channel
     * @param format Sample format
     * @param timestamp Timeline position in rational time
     */
    static std::shared_ptr<AudioFrame> create(
        uint32_t sample_rate,
        uint16_t channel_count,
        uint32_t sample_count,
        SampleFormat format,
        const TimePoint& timestamp
    );

    /**
     * @brief Create an audio frame from raw data
     * 
     * @param sample_rate Sample rate in Hz
     * @param channel_count Number of audio channels 
     * @param sample_count Number of samples per channel
     * @param format Sample format
     * @param timestamp Timeline position
     * @param data Raw audio data (will be copied)
     * @param data_size Size of data in bytes
     */
    static std::shared_ptr<AudioFrame> create_from_data(
        uint32_t sample_rate,
        uint16_t channel_count,
        uint32_t sample_count,
        SampleFormat format,
        const TimePoint& timestamp,
        const void* data,
        size_t data_size
    );

    // Disable copy constructor and assignment
    AudioFrame(const AudioFrame&) = delete;
    AudioFrame& operator=(const AudioFrame&) = delete;

    // Getters
    uint32_t sample_rate() const { return sample_rate_; }
    uint16_t channel_count() const { return channel_count_; }
    uint32_t sample_count() const { return sample_count_; }
    SampleFormat format() const { return format_; }
    const TimePoint& timestamp() const { return timestamp_; }
    
    /**
     * @brief Get duration of this audio frame
     */
    TimeDuration duration() const {
        return TimeDuration(static_cast<int64_t>(sample_count_), static_cast<int32_t>(sample_rate_));
    }

    /**
     * @brief Get raw audio data pointer
     * 
     * @return Pointer to audio data, format depends on sample_format()
     */
    void* data() { return data_.data(); }
    const void* data() const { return data_.data(); }

    /**
     * @brief Get audio data size in bytes
     */
    size_t data_size() const { return data_.size(); }

    /**
     * @brief Get size of a single sample in bytes for the current format
     */
    static size_t bytes_per_sample(SampleFormat format);

    /**
     * @brief Get channel layout based on channel count
     * 
     * @param channel_count Number of channels
     * @return Best guess channel layout for the given count
     */
    static ChannelLayout deduce_channel_layout(uint16_t channel_count);

    /**
     * @brief Get human-readable format string
     */
    static const char* format_string(SampleFormat format);

    /**
     * @brief Check if the audio frame contains valid data
     */
    bool is_valid() const {
        return sample_rate_ > 0 && 
               channel_count_ > 0 && 
               sample_count_ > 0 && 
               format_ != SampleFormat::Unknown &&
               !data_.empty();
    }

    /**
     * @brief Create a copy of this audio frame with potentially different format
     * 
     * @param new_format Target sample format (if different from current)
     * @return New AudioFrame instance with copied/converted data
     */
    std::shared_ptr<AudioFrame> clone(SampleFormat new_format = SampleFormat::Unknown) const;

    /**
     * @brief Get interleaved sample at specific position
     * 
     * @param channel Channel index (0-based)
     * @param sample Sample index (0-based)
     * @return Sample value as float32 (-1.0 to 1.0 range)
     */
    float get_sample_as_float(uint16_t channel, uint32_t sample) const;

    /**
     * @brief Set interleaved sample at specific position
     * 
     * @param channel Channel index (0-based) 
     * @param sample Sample index (0-based)
     * @param value Sample value as float32 (-1.0 to 1.0 range)
     */
    void set_sample_from_float(uint16_t channel, uint32_t sample, float value);

private:
    // Private constructor - use create() methods
    AudioFrame(uint32_t sample_rate, uint16_t channel_count, uint32_t sample_count,
               SampleFormat format, const TimePoint& timestamp);

    uint32_t sample_rate_;      ///< Sample rate in Hz
    uint16_t channel_count_;    ///< Number of audio channels
    uint32_t sample_count_;     ///< Number of samples per channel
    SampleFormat format_;       ///< Sample format
    TimePoint timestamp_;       ///< Timeline position
    
    std::vector<uint8_t> data_; ///< Raw audio data (interleaved)
};

/**
 * @brief Audio frame processing utilities
 */
namespace frame_utils {

    /**
     * @brief Mix two audio frames together
     * 
     * @param frame1 First audio frame
     * @param frame2 Second audio frame  
     * @param gain1 Gain for first frame (0.0 to 1.0)
     * @param gain2 Gain for second frame (0.0 to 1.0)
     * @return Mixed audio frame, or nullptr on error
     */
    std::shared_ptr<AudioFrame> mix_frames(
        const std::shared_ptr<AudioFrame>& frame1,
        const std::shared_ptr<AudioFrame>& frame2,
        float gain1 = 1.0f,
        float gain2 = 1.0f
    );

    /**
     * @brief Apply gain to an audio frame
     * 
     * @param frame Input audio frame
     * @param gain_db Gain in decibels
     * @return New audio frame with gain applied
     */
    std::shared_ptr<AudioFrame> apply_gain(
        const std::shared_ptr<AudioFrame>& frame,
        float gain_db
    );

    /**
     * @brief Convert sample format of an audio frame
     * 
     * @param frame Input audio frame
     * @param target_format Target sample format
     * @return New audio frame with converted format
     */
    std::shared_ptr<AudioFrame> convert_format(
        const std::shared_ptr<AudioFrame>& frame,
        SampleFormat target_format
    );

    /**
     * @brief Extract specific channels from an audio frame
     * 
     * @param frame Input audio frame
     * @param channels Vector of channel indices to extract
     * @return New audio frame with only specified channels
     */
    std::shared_ptr<AudioFrame> extract_channels(
        const std::shared_ptr<AudioFrame>& frame,
        const std::vector<uint16_t>& channels
    );

} // namespace frame_utils

} // namespace ve::audio