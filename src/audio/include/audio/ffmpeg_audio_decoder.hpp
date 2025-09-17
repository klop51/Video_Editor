#pragma once

#include "audio/audio_frame.hpp"
#include "media_io/demuxer.hpp"
#include <memory>
#include <string>
#include <vector>
#include <chrono>

#ifdef ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}
#endif

namespace ve::audio {

/**
 * @brief Audio decoder configuration
 */
struct AudioDecoderConfig {
    uint32_t target_sample_rate = 48000;    ///< Target output sample rate
    uint16_t target_channels = 2;           ///< Target number of channels  
    SampleFormat target_format = SampleFormat::Float32; ///< Target sample format
    
    // Advanced settings
    bool enable_resampling = true;          ///< Enable automatic resampling
    bool enable_channel_layout_conversion = true; ///< Enable channel layout conversion
    uint32_t max_frame_size = 4096;        ///< Maximum samples per output frame
    
    // Quality settings
    int resample_quality = 10;              ///< SWR quality (0-10, higher is better)
    bool use_precise_timestamps = true;     ///< Use precise timestamp calculation
};

/**
 * @brief Audio decoder error codes
 */
enum class AudioDecoderError {
    Success,
    InvalidInput,
    DecoderNotFound,
    DecoderInitFailed,
    InvalidCodecParameters,
    ResamplerInitFailed,
    DecodeFailed,
    EndOfStream,
    InsufficientData,
    InvalidTimestamp,
    OutOfMemory
};

/**
 * @brief FFmpeg-based audio decoder for professional audio codecs
 * 
 * Supports AAC, MP3, FLAC, PCM and other formats commonly used in video production.
 * Features automatic resampling to target format and sample rate for consistent
 * audio pipeline integration.
 * 
 * Key features:
 * - Multi-format support (AAC, MP3, FLAC, PCM, etc.)
 * - Automatic resampling to 48kHz stereo
 * - High-quality sample rate conversion using libswresample
 * - Precise timestamp preservation for A/V sync
 * - Buffer management integration with AudioFrame
 * - Professional codec support for broadcast workflows
 */
class FFmpegAudioDecoder {
public:
    /**
     * @brief Create audio decoder for specific stream
     * 
     * @param stream_info Audio stream information from demuxer
     * @param config Decoder configuration
     * @return Decoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioDecoder> create(
        const media_io::StreamInfo& stream_info,
        const AudioDecoderConfig& config = {}
    );

    /**
     * @brief Create audio decoder from codec parameters
     * 
     * @param codec_id FFmpeg codec ID
     * @param codec_params Raw codec parameters data
     * @param config Decoder configuration
     * @return Decoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioDecoder> create_from_params(
        int codec_id,
        const std::vector<uint8_t>& codec_params,
        const AudioDecoderConfig& config = {}
    );

    ~FFmpegAudioDecoder();

    // Disable copy constructor and assignment
    FFmpegAudioDecoder(const FFmpegAudioDecoder&) = delete;
    FFmpegAudioDecoder& operator=(const FFmpegAudioDecoder&) = delete;

    /**
     * @brief Initialize decoder with stream information
     * 
     * @param stream_info Audio stream metadata
     * @return Success or error code
     */
    AudioDecoderError initialize(const media_io::StreamInfo& stream_info);

    /**
     * @brief Decode audio packet into AudioFrame
     * 
     * @param packet Input audio packet from demuxer
     * @param frame Output audio frame (may be nullptr if no frame ready)
     * @return Success, error code, or EndOfStream
     */
    AudioDecoderError decode_packet(
        const media_io::Packet& packet,
        std::shared_ptr<AudioFrame>& frame
    );

    /**
     * @brief Flush decoder and get remaining frames
     * 
     * @param frames Vector to receive remaining decoded frames
     * @return Success or error code
     */
    AudioDecoderError flush(std::vector<std::shared_ptr<AudioFrame>>& frames);

    /**
     * @brief Reset decoder state for seeking
     */
    void reset();

    /**
     * @brief Check if decoder is initialized and ready
     */
    bool is_initialized() const { return decoder_initialized_; }

    /**
     * @brief Get input stream information
     */
    const media_io::StreamInfo& get_stream_info() const { return stream_info_; }

    /**
     * @brief Get decoder configuration
     */
    const AudioDecoderConfig& get_config() const { return config_; }

    /**
     * @brief Get supported codec list
     * 
     * @return Vector of supported codec names
     */
    static std::vector<std::string> get_supported_codecs();

    /**
     * @brief Check if codec is supported
     * 
     * @param codec_name Codec name (e.g., "aac", "mp3", "flac")
     * @return True if codec is supported
     */
    static bool is_codec_supported(const std::string& codec_name);

    /**
     * @brief Get decoder statistics
     */
    struct Statistics {
        uint64_t packets_decoded = 0;       ///< Total packets processed
        uint64_t frames_produced = 0;       ///< Total frames produced
        uint64_t samples_decoded = 0;       ///< Total samples decoded
        uint64_t bytes_processed = 0;       ///< Total bytes processed
        
        // Performance metrics
        double avg_decode_time_us = 0.0;    ///< Average decode time per packet
        double avg_resample_time_us = 0.0;  ///< Average resample time per frame
        
        // Quality metrics
        uint32_t decode_errors = 0;         ///< Number of decode errors
        uint32_t resample_errors = 0;       ///< Number of resample errors
    };
    
    const Statistics& get_statistics() const { return stats_; }
    void reset_statistics() { stats_ = {}; }

private:
    // Private constructor - use create() methods
    FFmpegAudioDecoder();

    // Initialize FFmpeg components
    AudioDecoderError init_decoder(const media_io::StreamInfo& stream_info);
    AudioDecoderError init_resampler();

    // Decoding pipeline
    AudioDecoderError decode_frame(const media_io::Packet& packet);
    AudioDecoderError resample_frame(std::shared_ptr<AudioFrame>& output_frame);
    
    // Utility methods
    TimePoint calculate_frame_timestamp(int64_t pts, int64_t sample_offset = 0);
    SampleFormat convert_ffmpeg_format(AVSampleFormat av_format);
    AVSampleFormat convert_to_ffmpeg_format(SampleFormat format);
    void cleanup();

    // Configuration
    AudioDecoderConfig config_;
    media_io::StreamInfo stream_info_;
    bool decoder_initialized_ = false;

#ifdef ENABLE_FFMPEG
    // FFmpeg components
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVFrame* input_frame_ = nullptr;
    
    // Resampling
    SwrContext* swr_ctx_ = nullptr;
    AVFrame* output_frame_ = nullptr;
    
    // Buffer management
    std::vector<uint8_t> resample_buffer_;
    int64_t next_pts_ = 0;
    AVRational time_base_ = {1, 48000};
#endif

    // Statistics
    Statistics stats_;
    
    // Performance tracking
    std::chrono::high_resolution_clock::time_point last_decode_start_;
    std::chrono::high_resolution_clock::time_point last_resample_start_;
};

/**
 * @brief Audio decoder factory for easy creation
 */
class AudioDecoderFactory {
public:
    /**
     * @brief Create best decoder for media file
     * 
     * @param file_path Path to media file
     * @param audio_stream_index Audio stream index (-1 for first audio stream)
     * @param config Decoder configuration
     * @return Decoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioDecoder> create_for_file(
        const std::string& file_path,
        int audio_stream_index = -1,
        const AudioDecoderConfig& config = {}
    );

    /**
     * @brief Create decoder for specific codec
     * 
     * @param codec_name Codec name (e.g., "aac", "mp3")
     * @param sample_rate Input sample rate
     * @param channels Input channel count
     * @param config Decoder configuration  
     * @return Decoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioDecoder> create_for_codec(
        const std::string& codec_name,
        uint32_t sample_rate,
        uint16_t channels,
        const AudioDecoderConfig& config = {}
    );
};

/**
 * @brief Utility functions for audio decoding
 */
namespace decoder_utils {

    /**
     * @brief Get readable error string
     * 
     * @param error Audio decoder error code
     * @return Human-readable error description
     */
    const char* error_string(AudioDecoderError error);

    /**
     * @brief Estimate decoder buffer requirements
     * 
     * @param stream_info Audio stream information
     * @param target_duration Target buffer duration in seconds
     * @return Estimated buffer size in bytes
     */
    size_t estimate_buffer_size(
        const media_io::StreamInfo& stream_info,
        double target_duration = 1.0
    );

    /**
     * @brief Check if resampling is needed
     * 
     * @param input_stream Stream information
     * @param target_config Target configuration
     * @return True if resampling is required
     */
    bool needs_resampling(
        const media_io::StreamInfo& input_stream,
        const AudioDecoderConfig& target_config
    );

} // namespace decoder_utils

} // namespace ve::audio