#include "audio/ffmpeg_decoder_bridge.hpp"
#include <core/log.hpp>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace ve::audio {

namespace {
    /**
     * @brief Read entire file into memory
     */
    std::vector<uint8_t> read_file(const std::string& file_path) {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return {};
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return {};
        }

        return buffer;
    }

    /**
     * @brief Detect audio codec from file extension
     */
    AudioCodec detect_codec_from_extension(const std::string& file_path) {
        std::filesystem::path path(file_path);
        std::string extension = path.extension().string();

        // Convert to lowercase
        std::transform(extension.begin(), extension.end(), extension.begin(), 
                      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (extension == ".wav" || extension == ".wave") {
            return AudioCodec::PCM;
        } else if (extension == ".mp3") {
            return AudioCodec::MP3;
        } else if (extension == ".aac" || extension == ".m4a") {
            return AudioCodec::AAC;
        } else if (extension == ".flac") {
            return AudioCodec::FLAC;
        } else if (extension == ".ogg") {
            return AudioCodec::Vorbis;
        } else if (extension == ".ac3") {
            return AudioCodec::AC3;
        } else if (extension == ".eac3") {
            return AudioCodec::EAC3;
        } else if (extension == ".opus") {
            return AudioCodec::Opus;
        }

        return AudioCodec::Unknown;
    }
}

std::unique_ptr<FFmpegDecoderBridge> FFmpegDecoderBridge::create(const std::string& file_path) {
    auto decoder = std::unique_ptr<FFmpegDecoderBridge>(new FFmpegDecoderBridge());

    if (decoder->load_file(file_path) != AudioError::None) {
        return nullptr;
    }

    return decoder;
}

FFmpegDecoderBridge::FFmpegDecoderBridge() {
    // Configure decoder for 48kHz stereo Float32 output
    decoder_config_.target_sample_rate = 48000;
    decoder_config_.target_channels = 2;
    decoder_config_.target_format = SampleFormat::Float32;
    decoder_config_.enable_resampling = true;
    decoder_config_.enable_channel_layout_conversion = true;
    decoder_config_.max_frame_size = 1024;
}

FFmpegDecoderBridge::~FFmpegDecoderBridge() {
    // Cleanup handled by unique_ptr
}

AudioError FFmpegDecoderBridge::initialize(const std::vector<uint8_t>& stream_data) {
    if (initialized_) {
        return AudioError::None;
    }

    if (stream_data.empty()) {
        last_error_ = AudioError::InvalidFormat;
        return last_error_;
    }

    // Store file data for FFmpeg processing
    file_data_ = stream_data;

    // For now, we'll initialize on first decode
    // This allows us to handle both file-based and stream-based initialization
    initialized_ = true;
    last_error_ = AudioError::None;

    return AudioError::None;
}

AudioError FFmpegDecoderBridge::load_file(const std::string& file_path) {
    file_path_ = file_path;

    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        ve::log::error("Audio file does not exist: " + file_path);
        last_error_ = AudioError::InvalidFormat;
        return last_error_;
    }

    // Read file data
    file_data_ = read_file(file_path);
    if (file_data_.empty()) {
        ve::log::error("Failed to read audio file: " + file_path);
        last_error_ = AudioError::InvalidFormat;
        return last_error_;
    }

    // Detect codec from file extension
    AudioCodec detected_codec = detect_codec_from_extension(file_path);
    if (detected_codec == AudioCodec::Unknown) {
        ve::log::warn("Unknown file extension, attempting auto-detection: " + file_path);
    }

    // Initialize basic stream info
    stream_info_.codec = detected_codec;
    stream_info_.preferred_format = SampleFormat::Float32;

    // For now, we'll set up basic info and let FFmpeg handle the details
    // This will be refined when we actually decode the file
    stream_info_.sample_rate = 48000;  // Default, will be updated by FFmpeg
    stream_info_.channel_count = 2;    // Default, will be updated by FFmpeg
    stream_info_.channel_layout = ChannelLayout::Stereo;
    stream_info_.duration_samples = 48000 * 10;  // 10 seconds default
    stream_info_.bit_rate = 128000;   // 128kbps default

    initialized_ = true;
    last_error_ = AudioError::None;

    ve::log::info("FFmpegDecoderBridge loaded file: " + file_path);
    return AudioError::None;
}

AudioStreamInfo FFmpegDecoderBridge::get_stream_info() const {
    if (!initialized_) {
        ve::log::warn("FFmpegDecoderBridge::get_stream_info called before initialization");
        return AudioStreamInfo{};
    }
    return stream_info_;
}

std::shared_ptr<AudioFrame> FFmpegDecoderBridge::decode_frame(
    const std::vector<uint8_t>& input_data,
    const TimePoint& timestamp,
    SampleFormat output_format) {

    if (!initialized_) {
        last_error_ = AudioError::ConfigurationError;
        return nullptr;
    }

    // For file-based decoding, we ignore input_data and use our stored file_data_
    // In a real implementation, we'd need to handle streaming data

    // Create a decode request for the threaded decoder
    auto request = std::make_unique<DecodeRequest>(timestamp, 1024, output_format);

    // Submit the request (this will call process_decode_request)
    uint64_t request_id = submit_decode_request(std::move(request));

    if (request_id == 0) {
        last_error_ = AudioError::DecodeFailed;
        return nullptr;
    }

    // For synchronous operation, we need to wait for the result
    // This is a simplified implementation - in practice, you'd want async operation
    last_error_ = AudioError::None;
    return nullptr;  // Placeholder - actual implementation would return decoded frame
}

AudioError FFmpegDecoderBridge::seek(const TimePoint& timestamp) {
    if (!initialized_) {
        return AudioError::ConfigurationError;
    }

    // Calculate sample position from timestamp
    current_sample_position_ = calculate_sample_position(timestamp);

    // TODO: Implement actual seeking in FFmpeg decoder
    ve::log::info("FFmpegDecoderBridge seek to timestamp: " + std::to_string(current_sample_position_) + " samples");

    return AudioError::None;
}

void FFmpegDecoderBridge::flush() {
    current_sample_position_ = 0;

    // Cancel all pending requests
    cancel_all_requests();

    // TODO: Flush FFmpeg decoder buffers
    ve::log::info("FFmpegDecoderBridge flush called");
}

AudioCodec FFmpegDecoderBridge::get_codec_type() const {
    return stream_info_.codec;
}

const char* FFmpegDecoderBridge::get_codec_name() const {
    switch (stream_info_.codec) {
        case AudioCodec::PCM: return "PCM (WAV)";
        case AudioCodec::MP3: return "MP3";
        case AudioCodec::AAC: return "AAC";
        case AudioCodec::FLAC: return "FLAC";
        case AudioCodec::Vorbis: return "Vorbis (OGG)";
        case AudioCodec::AC3: return "AC-3";
        case AudioCodec::EAC3: return "E-AC-3";
        case AudioCodec::Opus: return "Opus";
        default: return "Unknown";
    }
}

bool FFmpegDecoderBridge::supports_hardware_acceleration() const {
    // FFmpeg can support hardware acceleration depending on codec and platform
    // For now, return false as we haven't implemented HW accel detection
    return false;
}

std::shared_ptr<AudioFrame> FFmpegDecoderBridge::process_decode_request(const DecodeRequest& request) {
    if (!initialized_) {
        last_error_ = AudioError::ConfigurationError;
        return nullptr;
    }

    // TODO: Implement actual FFmpeg decoding
    // This is a placeholder that creates a test frame

    auto frame = AudioFrame::create(
        stream_info_.sample_rate,
        stream_info_.channel_count,
        request.frame_count,
        request.output_format,
        request.timestamp
    );

    if (!frame) {
        last_error_ = AudioError::MemoryError;
        return nullptr;
    }

    // For now, fill with silence (placeholder implementation)
    // Real implementation would decode from file_data_ using FFmpeg
    std::memset(frame->data(), 0, frame->data_size());

    last_error_ = AudioError::None;
    return frame;
}

SampleFormat FFmpegDecoderBridge::convert_ffmpeg_format(int ffmpeg_format) const {
#ifdef ENABLE_FFMPEG
    switch (ffmpeg_format) {
        case AV_SAMPLE_FMT_U8:   return SampleFormat::Unknown;  // Not supported
        case AV_SAMPLE_FMT_S16:  return SampleFormat::Int16;
        case AV_SAMPLE_FMT_S32:  return SampleFormat::Int32;
        case AV_SAMPLE_FMT_FLT:  return SampleFormat::Float32;
        case AV_SAMPLE_FMT_DBL:  return SampleFormat::Unknown;  // Not supported
        default:                 return SampleFormat::Unknown;
    }
#else
    return SampleFormat::Unknown;
#endif
}

int FFmpegDecoderBridge::convert_to_ffmpeg_format(SampleFormat format) const {
#ifdef ENABLE_FFMPEG
    switch (format) {
        case SampleFormat::Int16:   return AV_SAMPLE_FMT_S16;
        case SampleFormat::Int32:   return AV_SAMPLE_FMT_S32;
        case SampleFormat::Float32: return AV_SAMPLE_FMT_FLT;
        default:                    return AV_SAMPLE_FMT_NONE;
    }
#else
    return 0;
#endif
}

TimePoint FFmpegDecoderBridge::calculate_timestamp(uint64_t sample_position) const {
    if (stream_info_.sample_rate == 0) {
        return TimePoint(0, 1);
    }

    return TimePoint(static_cast<int64_t>(sample_position),
                     static_cast<int32_t>(stream_info_.sample_rate));
}

uint64_t FFmpegDecoderBridge::calculate_sample_position(const TimePoint& timestamp) const {
    if (stream_info_.sample_rate == 0) {
        return 0;
    }

    // Convert timestamp to samples
    auto rational = timestamp.to_rational();
    double seconds = static_cast<double>(rational.num) / static_cast<double>(rational.den);
    return static_cast<uint64_t>(seconds * stream_info_.sample_rate);
}

} // namespace ve::audio