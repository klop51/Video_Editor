#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "audio_frame.hpp"
#include <core/time.hpp>

namespace ve::audio {

/**
 * @brief Audio codec types supported by the decoder
 */
enum class AudioCodec {
    AAC,        ///< Advanced Audio Coding
    MP3,        ///< MPEG Layer 3
    PCM,        ///< Pulse Code Modulation (uncompressed)
    FLAC,       ///< Free Lossless Audio Codec
    Vorbis,     ///< Ogg Vorbis
    Opus,       ///< Opus codec
    AC3,        ///< Dolby Digital AC-3
    EAC3,       ///< Enhanced AC-3 (Dolby Digital Plus)
    Unknown     ///< Unknown or unsupported codec
};

/**
 * @brief Audio decoder error types
 */
enum class AudioError {
    None,                   ///< No error
    InvalidFormat,          ///< Unsupported or invalid audio format
    DecodeFailed,           ///< Failed to decode audio data
    EndOfStream,            ///< Reached end of audio stream
    InvalidTimestamp,       ///< Invalid or discontinuous timestamp
    InsufficientData,       ///< Not enough data to decode
    HardwareError,          ///< Hardware decoder error
    MemoryError,            ///< Memory allocation error
    ConfigurationError,     ///< Invalid decoder configuration
    NetworkError,           ///< Network-related error (for streaming)
    Interrupted,            ///< Decode operation was interrupted
    Unknown                 ///< Unknown error
};

/**
 * @brief Audio stream information
 */
struct AudioStreamInfo {
    AudioCodec codec = AudioCodec::Unknown;
    uint32_t sample_rate = 0;           ///< Sample rate in Hz
    uint16_t channel_count = 0;         ///< Number of audio channels
    uint64_t duration_samples = 0;      ///< Total duration in samples
    uint32_t bit_rate = 0;             ///< Bit rate in bits per second
    uint16_t bits_per_sample = 0;      ///< Bits per sample (for PCM)
    SampleFormat preferred_format = SampleFormat::Float32;
    ChannelLayout channel_layout = ChannelLayout::Unknown;
    
    /**
     * @brief Get duration as rational time
     */
    TimeDuration duration() const {
        if (sample_rate == 0) return TimeDuration(0, 1);
        return TimeDuration(static_cast<int64_t>(duration_samples), static_cast<int32_t>(sample_rate));
    }

    /**
     * @brief Check if stream info is valid
     */
    bool is_valid() const {
        return codec != AudioCodec::Unknown && 
               sample_rate > 0 && 
               channel_count > 0;
    }
};

/**
 * @brief Decode request for asynchronous decoding
 */
struct DecodeRequest {
    TimePoint timestamp;               ///< Target timestamp to decode
    uint32_t frame_count;              ///< Number of frames to decode
    SampleFormat output_format;        ///< Desired output format
    std::function<void(std::shared_ptr<AudioFrame>, AudioError)> callback;
    
    uint64_t request_id = 0;           ///< Unique request identifier
    std::atomic<bool> cancelled{false}; ///< Cancellation flag
    
    DecodeRequest(const TimePoint& ts, uint32_t frames, SampleFormat format)
        : timestamp(ts), frame_count(frames), output_format(format) {}
};

/**
 * @brief Abstract base class for audio decoders
 * 
 * Provides a codec-agnostic interface for decoding audio streams.
 * Supports both synchronous and asynchronous decoding operations.
 * Thread-safe for concurrent decode requests.
 */
class AudioDecoder {
public:
    virtual ~AudioDecoder() = default;

    /**
     * @brief Initialize the decoder with stream data
     * 
     * @param stream_data Raw stream data (headers, format info, etc.)
     * @return AudioError::None on success, error code otherwise
     */
    virtual AudioError initialize(const std::vector<uint8_t>& stream_data) = 0;

    /**
     * @brief Get information about the audio stream
     * 
     * @return Stream information, or error set in info.error_code if failed
     */
    virtual AudioStreamInfo get_stream_info() const = 0;

    /**
     * @brief Decode audio data synchronously
     * 
     * @param input_data Encoded audio data
     * @param timestamp Target timestamp for decoded frame
     * @param output_format Desired output sample format
     * @return Decoded audio frame, or nullptr if error (check last_error())
     */
    virtual std::shared_ptr<AudioFrame> decode_frame(
        const std::vector<uint8_t>& input_data,
        const TimePoint& timestamp,
        SampleFormat output_format = SampleFormat::Float32
    ) = 0;

    /**
     * @brief Get the last error that occurred
     * 
     * @return Last error code
     */
    virtual AudioError last_error() const = 0;

    /**
     * @brief Seek to specific timestamp in the stream
     * 
     * @param timestamp Target timestamp
     * @return AudioError::None on success, error code otherwise
     */
    virtual AudioError seek(const TimePoint& timestamp) = 0;

    /**
     * @brief Flush decoder buffers
     * 
     * Clears any internal buffers and resets decoder state.
     */
    virtual void flush() = 0;

    /**
     * @brief Check if decoder supports hardware acceleration
     */
    virtual bool supports_hardware_acceleration() const { return false; }

    /**
     * @brief Get codec type handled by this decoder
     */
    virtual AudioCodec get_codec_type() const = 0;

    /**
     * @brief Get human-readable codec name
     */
    virtual const char* get_codec_name() const = 0;

    // Asynchronous decoding interface
    
    /**
     * @brief Submit decode request for asynchronous processing
     * 
     * @param request Decode request with callback
     * @return Request ID for tracking, or 0 on error
     */
    virtual uint64_t submit_decode_request(std::unique_ptr<DecodeRequest> request) = 0;

    /**
     * @brief Cancel pending decode request
     * 
     * @param request_id Request ID to cancel
     * @return true if request was found and cancelled
     */
    virtual bool cancel_decode_request(uint64_t request_id) = 0;

    /**
     * @brief Cancel all pending decode requests
     */
    virtual void cancel_all_requests() = 0;

    /**
     * @brief Get number of pending decode requests
     */
    virtual size_t get_pending_request_count() const = 0;

    /**
     * @brief Check if decoder is currently busy
     */
    virtual bool is_busy() const = 0;
};

/**
 * @brief Base implementation for threaded audio decoders
 * 
 * Provides common functionality for asynchronous decode request handling.
 * Concrete decoders should inherit from this class and implement the
 * pure virtual decode methods.
 */
class ThreadedAudioDecoder : public AudioDecoder {
public:
    ThreadedAudioDecoder();
    virtual ~ThreadedAudioDecoder();

    // Asynchronous interface implementation
    uint64_t submit_decode_request(std::unique_ptr<DecodeRequest> request) override;
    bool cancel_decode_request(uint64_t request_id) override;
    void cancel_all_requests() override;
    size_t get_pending_request_count() const override;
    bool is_busy() const override;

protected:
    /**
     * @brief Start the worker thread
     */
    void start_worker_thread();

    /**
     * @brief Stop the worker thread
     */
    void stop_worker_thread();

    /**
     * @brief Process a single decode request (implemented by derived classes)
     * 
     * @param request The decode request to process
     * @return Decoded audio frame, or nullptr if error
     */
    virtual std::shared_ptr<AudioFrame> process_decode_request(
        const DecodeRequest& request
    ) = 0;

private:
    void worker_thread_loop();

    std::thread worker_thread_;
    std::atomic<bool> stop_worker_{false};
    
    mutable std::mutex request_queue_mutex_;
    std::queue<std::unique_ptr<DecodeRequest>> request_queue_;
    std::condition_variable request_available_;
    
    std::atomic<uint64_t> next_request_id_{1};
    std::atomic<size_t> pending_count_{0};
    std::atomic<bool> worker_busy_{false};
};

/**
 * @brief Audio decoder factory
 * 
 * Creates appropriate decoder instances based on codec type or stream data.
 */
class AudioDecoderFactory {
public:
    /**
     * @brief Create decoder for specific codec
     * 
     * @param codec Target audio codec
     * @return Decoder instance or nullptr if codec not supported
     */
    static std::unique_ptr<AudioDecoder> create_decoder(AudioCodec codec);

    /**
     * @brief Create decoder by detecting codec from stream data
     * 
     * @param stream_data Raw stream data for codec detection
     * @return Decoder instance or nullptr if codec not detected/supported
     */
    static std::unique_ptr<AudioDecoder> create_decoder_from_data(
        const std::vector<uint8_t>& stream_data
    );

    /**
     * @brief Detect codec type from stream data
     * 
     * @param stream_data Raw stream data
     * @return Detected codec type or AudioCodec::Unknown
     */
    static AudioCodec detect_codec(const std::vector<uint8_t>& stream_data);

    /**
     * @brief Get list of supported codecs
     */
    static std::vector<AudioCodec> get_supported_codecs();

    /**
     * @brief Check if a codec is supported
     */
    static bool is_codec_supported(AudioCodec codec);

    /**
     * @brief Get human-readable codec name
     */
    static const char* get_codec_name(AudioCodec codec);
};

/**
 * @brief Audio decoder utilities
 */
namespace decoder_utils {

    /**
     * @brief Convert AudioError to human-readable string
     */
    const char* error_to_string(AudioError error);

    /**
     * @brief Check if error is recoverable
     */
    bool is_recoverable_error(AudioError error);

    /**
     * @brief Estimate decode complexity for a codec
     * 
     * @param codec Audio codec
     * @return Relative complexity (1.0 = baseline)
     */
    float get_decode_complexity(AudioCodec codec);

    /**
     * @brief Get recommended buffer size for codec
     * 
     * @param codec Audio codec
     * @param sample_rate Sample rate in Hz
     * @return Recommended buffer size in samples
     */
    uint32_t get_recommended_buffer_size(AudioCodec codec, uint32_t sample_rate);

} // namespace decoder_utils

} // namespace ve::audio