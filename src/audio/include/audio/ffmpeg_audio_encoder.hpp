/**
 * @file ffmpeg_audio_encoder.hpp
 * @brief Week 9 Audio Export Pipeline - FFmpeg Audio Encoder
 * 
 * Professional audio encoding using FFmpeg for high-quality export:
 * - AAC encoding (libfdk-aac or ffmpeg native)
 * - MP3 encoding (LAME or ffmpeg native)
 * - FLAC lossless encoding
 * - Professional quality settings and metadata support
 * - Multi-threaded encoding for performance
 */

#pragma once

#include "audio/audio_frame.hpp"
#include "core/time.hpp"
#include "core/log.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
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
 * @brief Audio encoder configuration
 */
struct AudioEncoderConfig {
    // Output format
    uint32_t sample_rate = 48000;
    uint16_t channel_count = 2;
    uint32_t bit_depth = 16;
    
    // Encoding settings
    uint32_t bitrate = 320000;          // Bitrate in bps (320kbps default)
    bool vbr_mode = true;               // Variable bitrate
    uint32_t quality = 5;               // Quality level (0-10, codec dependent)
    
    // Advanced settings
    bool joint_stereo = true;           // Joint stereo for MP3
    uint32_t compression_level = 5;     // FLAC compression (0-8)
    bool enable_metadata = true;        // Include metadata in output
    
    // Performance
    uint32_t thread_count = 0;          // 0 = auto-detect
    uint32_t buffer_size = 4096;        // Encoding buffer size
};

/**
 * @brief Audio encoder error codes
 */
enum class AudioEncoderError {
    Success,
    InvalidInput,
    EncoderNotFound,
    EncoderInitFailed,
    EncodeFailed,
    OutputError,
    InvalidConfig,
    OutOfMemory,
    UnknownError
};

/**
 * @brief Audio export format types
 */
enum class AudioExportFormat {
    MP3,        // MPEG-1 Audio Layer III
    AAC,        // Advanced Audio Codec
    FLAC,       // Free Lossless Audio Codec
    OGG         // Ogg Vorbis
};

/**
 * @brief Audio encoder statistics
 */
struct AudioEncoderStats {
    uint64_t frames_encoded = 0;
    uint64_t bytes_written = 0;
    uint32_t average_bitrate = 0;
    double encoding_speed = 0.0;        // Real-time factor
    std::chrono::duration<double> total_time{0};
};

/**
 * @brief Audio metadata for export
 */
struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string comment;
    uint32_t year = 0;
    uint32_t track_number = 0;
    uint32_t total_tracks = 0;
};

/**
 * @brief FFmpeg-based audio encoder for professional formats
 * 
 * Provides high-quality audio encoding for export pipeline:
 * - Multi-format support (AAC, MP3, FLAC, OGG)
 * - Professional quality settings
 * - Metadata embedding
 * - Performance optimization
 * - Thread-safe operation
 */
class FFmpegAudioEncoder {
public:
    /**
     * @brief Create audio encoder for specific format
     * 
     * @param format Target audio format
     * @param config Encoder configuration
     * @return Encoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioEncoder> create(
        AudioExportFormat format,
        const AudioEncoderConfig& config = {}
    );

    /**
     * @brief Create audio encoder with simplified parameters
     * 
     * @param format Target audio format
     * @param sample_rate Target sample rate
     * @param channels Number of channels
     * @return Encoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioEncoder> create(
        AudioExportFormat format,
        uint32_t sample_rate,
        uint16_t channels
    );

    /**
     * @brief Create encoder with codec name
     * 
     * @param codec_name FFmpeg codec name (e.g., "libfdk_aac", "libmp3lame")
     * @param config Encoder configuration
     * @return Encoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioEncoder> create_with_codec(
        const std::string& codec_name,
        const AudioEncoderConfig& config = {}
    );

    ~FFmpegAudioEncoder();

    // Disable copy constructor and assignment
    FFmpegAudioEncoder(const FFmpegAudioEncoder&) = delete;
    FFmpegAudioEncoder& operator=(const FFmpegAudioEncoder&) = delete;

    /**
     * @brief Initialize encoder for output file
     * 
     * @param output_path Output file path
     * @param metadata Audio metadata (optional)
     * @return Success or error code
     */
    AudioEncoderError initialize(const std::string& output_path,
                                const AudioMetadata& metadata = {});

    /**
     * @brief Encode audio frame
     * 
     * @param frame Input audio frame
     * @return Success or error code
     */
    AudioEncoderError encode_frame(const std::shared_ptr<AudioFrame>& frame);

    /**
     * @brief Flush encoder and finalize output
     * 
     * @return Success or error code
     */
    AudioEncoderError finalize();

    /**
     * @brief Check if encoder is initialized
     */
    bool is_initialized() const { return encoder_initialized_; }

    /**
     * @brief Get encoder configuration
     */
    const AudioEncoderConfig& get_config() const { return config_; }

    /**
     * @brief Get encoding statistics
     */
    const AudioEncoderStats& get_stats() const { return stats_; }

    /**
     * @brief Get supported export formats
     * 
     * @return Vector of supported format names
     */
    static std::vector<std::string> get_supported_formats();

    /**
     * @brief Check if format is supported
     * 
     * @param format Format to check
     * @return True if format is supported
     */
    static bool is_format_supported(AudioExportFormat format);

    /**
     * @brief Get codec name for format
     * 
     * @param format Audio format
     * @return Codec name or empty string if unsupported
     */
    static std::string get_codec_name(AudioExportFormat format);

    /**
     * @brief Get FFmpeg version information
     * 
     * @return FFmpeg version string
     */
    static std::string get_version_info();

    /**
     * @brief Get list of available audio encoders
     * 
     * @return Vector of available encoder names
     */
    static std::vector<std::string> get_available_encoders();

    /**
     * @brief Get comprehensive codec availability diagnostics
     * 
     * @return Detailed codec support information for debugging
     */
    static std::string get_codec_diagnostics();

private:
    // Private constructor - use create() methods
    FFmpegAudioEncoder();

    // Initialize FFmpeg components
    AudioEncoderError init_encoder(AudioExportFormat format);
    AudioEncoderError init_resampler();
    AudioEncoderError init_output_file(const std::string& output_path,
                                      const AudioMetadata& metadata);

    // Encoding pipeline
    AudioEncoderError encode_audio_frame(const std::shared_ptr<AudioFrame>& frame);
    AudioEncoderError resample_and_encode(const std::shared_ptr<AudioFrame>& frame);
    AudioEncoderError write_encoded_packet();

    // Utility methods
    void update_stats();
    void set_codec_options();
    void cleanup();

    // Configuration
    AudioEncoderConfig config_;
    AudioExportFormat format_;
    bool encoder_initialized_ = false;

#ifdef ENABLE_FFMPEG
    // FFmpeg components
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVFormatContext* format_ctx_ = nullptr;
    AVStream* audio_stream_ = nullptr;
    AVFrame* input_frame_ = nullptr;
    AVFrame* encoded_frame_ = nullptr;
    AVPacket* packet_ = nullptr;
    
    // Resampling
    SwrContext* swr_ctx_ = nullptr;
    
    // Buffer management
    std::vector<uint8_t> resample_buffer_;
    int64_t next_pts_ = 0;
#endif

    // Statistics and monitoring
    AudioEncoderStats stats_;
    std::chrono::steady_clock::time_point encoding_start_;
    std::chrono::steady_clock::time_point last_stats_update_;
};

/**
 * @brief Audio encoder factory for easy creation
 */
class AudioEncoderFactory {
public:
    /**
     * @brief Create encoder for file export
     * 
     * @param format Export format
     * @param output_path Output file path
     * @param config Encoder configuration
     * @param metadata Audio metadata
     * @return Encoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegAudioEncoder> create_for_export(
        AudioExportFormat format,
        const std::string& output_path,
        const AudioEncoderConfig& config = {},
        const AudioMetadata& metadata = {}
    );

    /**
     * @brief Get default configuration for format
     * 
     * @param format Target format
     * @return Default configuration
     */
    static AudioEncoderConfig get_default_config(AudioExportFormat format);

    /**
     * @brief Get professional quality config
     * 
     * @param format Target format
     * @param preset Quality preset ("broadcast", "web", "archive")
     * @return Professional configuration
     */
    static AudioEncoderConfig get_quality_config(AudioExportFormat format,
                                                 const std::string& preset);
};

/**
 * @brief Utility functions for audio encoding
 */
namespace encoder_utils {
    /**
     * @brief Convert format enum to string
     */
    std::string format_to_string(AudioExportFormat format);

    /**
     * @brief Parse format from string
     */
    AudioExportFormat string_to_format(const std::string& format_str);

    /**
     * @brief Get file extension for format
     */
    std::string get_file_extension(AudioExportFormat format);

    /**
     * @brief Calculate estimated bitrate
     */
    uint32_t estimate_bitrate(const AudioEncoderConfig& config, bool compressed = true);

} // namespace encoder_utils

} // namespace ve::audio