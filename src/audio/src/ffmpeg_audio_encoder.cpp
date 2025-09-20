/**
 * @file ffmpeg_audio_encoder.cpp
 * @brief Week 9 Audio Export Pipeline - FFmpeg Audio Encoder Implementation
 * 
 * Professional audio encoding implementation for export pipeline.
 */

#include "audio/ffmpeg_audio_encoder.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <chrono>

namespace ve::audio {

namespace {
    // Default configurations for different formats
    const std::unordered_map<AudioExportFormat, AudioEncoderConfig> DEFAULT_CONFIGS = {
        {AudioExportFormat::MP3, {48000, 2, 16, 320000, true, 5, true, 0, true, 0, 4096}},
        {AudioExportFormat::AAC, {48000, 2, 16, 256000, true, 5, false, 0, true, 0, 4096}},
        {AudioExportFormat::FLAC, {48000, 2, 24, 0, false, 5, false, 5, true, 0, 4096}},
        {AudioExportFormat::OGG, {48000, 2, 16, 320000, true, 5, false, 0, true, 0, 4096}}
    };

    // Primary codec mapping
    const std::unordered_map<AudioExportFormat, std::string> CODEC_NAMES = {
        {AudioExportFormat::MP3, "libmp3lame"},
        {AudioExportFormat::AAC, "aac"},
        {AudioExportFormat::FLAC, "flac"},
        {AudioExportFormat::OGG, "libvorbis"}
    };

    // Fallback codec chains for broader compatibility
    const std::unordered_map<AudioExportFormat, std::vector<std::string>> CODEC_FALLBACKS = {
        {AudioExportFormat::MP3, {"libmp3lame", "mp3", "mp3_mf"}},
        {AudioExportFormat::AAC, {"aac", "libfdk_aac", "libfaac", "aac_mf"}},
        {AudioExportFormat::FLAC, {"flac"}},
        {AudioExportFormat::OGG, {"libvorbis", "vorbis"}}
    };
} // anonymous namespace

#ifdef ENABLE_FFMPEG

FFmpegAudioEncoder::FFmpegAudioEncoder() = default;

FFmpegAudioEncoder::~FFmpegAudioEncoder() {
    cleanup();
}

std::unique_ptr<FFmpegAudioEncoder> FFmpegAudioEncoder::create(
    AudioExportFormat format,
    const AudioEncoderConfig& config) {
    
    auto encoder = std::unique_ptr<FFmpegAudioEncoder>(new FFmpegAudioEncoder());
    encoder->config_ = config;
    encoder->format_ = format;
    
    AudioEncoderError result = encoder->init_encoder(format);
    if (result != AudioEncoderError::Success) {
        ve::log::error("Failed to initialize audio encoder for format: " + 
                       encoder_utils::format_to_string(format));
        return nullptr;
    }
    
    return encoder;
}

std::unique_ptr<FFmpegAudioEncoder> FFmpegAudioEncoder::create(
    AudioExportFormat format,
    uint32_t sample_rate,
    uint16_t channels) {
    
    AudioEncoderConfig config;
    config.sample_rate = sample_rate;
    config.channel_count = channels;
    config.bit_depth = 16;  // Default bit depth
    
    return create(format, config);
}

std::unique_ptr<FFmpegAudioEncoder> FFmpegAudioEncoder::create_with_codec(
    const std::string& codec_name,
    const AudioEncoderConfig& config) {
    
    auto encoder = std::unique_ptr<FFmpegAudioEncoder>(new FFmpegAudioEncoder());
    encoder->config_ = config;
    
    // Find codec
    encoder->codec_ = avcodec_find_encoder_by_name(codec_name.c_str());
    if (!encoder->codec_) {
        ve::log::error("Codec not found: " + codec_name);
        return nullptr;
    }
    
    // Create codec context
    encoder->codec_ctx_ = avcodec_alloc_context3(encoder->codec_);
    if (!encoder->codec_ctx_) {
        ve::log::error("Failed to allocate codec context");
        return nullptr;
    }
    
    // Configure codec context
    encoder->codec_ctx_->sample_rate = config.sample_rate;
    encoder->codec_ctx_->ch_layout.nb_channels = config.channel_count;
    encoder->codec_ctx_->bit_rate = config.bitrate;
    
    // Set sample format
    if (encoder->codec_->sample_fmts) {
        encoder->codec_ctx_->sample_fmt = encoder->codec_->sample_fmts[0];
    } else {
        encoder->codec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    
    // Set channel layout
    if (config.channel_count == 1) {
        av_channel_layout_default(&encoder->codec_ctx_->ch_layout, 1);
    } else if (config.channel_count == 2) {
        av_channel_layout_default(&encoder->codec_ctx_->ch_layout, 2);
    } else {
        av_channel_layout_default(&encoder->codec_ctx_->ch_layout, config.channel_count);
    }
    
    encoder->set_codec_options();
    
    return encoder;
}

AudioEncoderError FFmpegAudioEncoder::initialize(const std::string& output_path,
                                                 const AudioMetadata& metadata) {
    
    if (encoder_initialized_) {
        ve::log::warn("Encoder already initialized");
        return AudioEncoderError::Success;
    }
    
    // Initialize output file
    AudioEncoderError result = init_output_file(output_path, metadata);
    if (result != AudioEncoderError::Success) {
        return result;
    }
    
    // Initialize resampler if needed
    result = init_resampler();
    if (result != AudioEncoderError::Success) {
        return result;
    }
    
    // Open codec
    int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        ve::log::error("Failed to open codec");
        return AudioEncoderError::EncoderInitFailed;
    }
    
    // Write file header
    ret = avformat_write_header(format_ctx_, nullptr);
    if (ret < 0) {
        ve::log::error("Failed to write format header");
        return AudioEncoderError::OutputError;
    }
    
    encoder_initialized_ = true;
    encoding_start_ = std::chrono::steady_clock::now();
    
    ve::log::info("Audio encoder initialized successfully");
    return AudioEncoderError::Success;
}

AudioEncoderError FFmpegAudioEncoder::encode_frame(const std::shared_ptr<AudioFrame>& frame) {
    if (!encoder_initialized_) {
        return AudioEncoderError::InvalidInput;
    }
    
    if (!frame || frame->sample_count() == 0) {
        return AudioEncoderError::InvalidInput;
    }
    
    return encode_audio_frame(frame);
}

AudioEncoderError FFmpegAudioEncoder::finalize() {
    if (!encoder_initialized_) {
        return AudioEncoderError::InvalidInput;
    }
    
    // Flush encoder
    int ret = avcodec_send_frame(codec_ctx_, nullptr);
    if (ret < 0) {
        ve::log::error("Failed to flush encoder");
        return AudioEncoderError::EncodeFailed;
    }
    
    // Receive remaining packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            ve::log::error("Error during encoding flush");
            return AudioEncoderError::EncodeFailed;
        }
        
        // Write packet
        packet_->stream_index = audio_stream_->index;
        av_packet_rescale_ts(packet_, codec_ctx_->time_base, audio_stream_->time_base);
        
        ret = av_interleaved_write_frame(format_ctx_, packet_);
        av_packet_unref(packet_);
        
        if (ret < 0) {
            ve::log::error("Failed to write packet");
            return AudioEncoderError::OutputError;
        }
    }
    
    // Write file trailer
    ret = av_write_trailer(format_ctx_);
    if (ret < 0) {
        ve::log::error("Failed to write format trailer");
        return AudioEncoderError::OutputError;
    }
    
    update_stats();
    ve::log::info("Audio encoding completed successfully");
    return AudioEncoderError::Success;
}

AudioEncoderError FFmpegAudioEncoder::init_encoder(AudioExportFormat format) {
    format_ = format;
    
    // Get codec fallback chain
    auto fallback_it = CODEC_FALLBACKS.find(format);
    if (fallback_it == CODEC_FALLBACKS.end()) {
        ve::log::error("Unsupported audio format");
        return AudioEncoderError::EncoderNotFound;
    }
    
    const auto& codec_candidates = fallback_it->second;
    std::string attempted_codecs;
    std::string selected_codec;
    
    // Try each codec in the fallback chain
    for (const auto& codec_name : codec_candidates) {
        if (!attempted_codecs.empty()) {
            attempted_codecs += ", ";
        }
        attempted_codecs += codec_name;
        
        codec_ = avcodec_find_encoder_by_name(codec_name.c_str());
        if (codec_) {
            selected_codec = codec_name;
            ve::log::info("Audio encoder: Selected codec '" + codec_name + "' for format " + 
                         encoder_utils::format_to_string(format));
            break;
        }
    }
    
    if (!codec_) {
        ve::log::error("No suitable codec found for " + encoder_utils::format_to_string(format) + 
                      ". Attempted: " + attempted_codecs);
        
        // Provide diagnostic information
        auto available_encoders = get_available_encoders();
        ve::log::info("Available audio encoders in FFmpeg:");
        for (size_t i = 0; i < std::min(available_encoders.size(), size_t(10)); ++i) {
            ve::log::info("  - " + available_encoders[i]);
        }
        if (available_encoders.size() > 10) {
            ve::log::info("  ... and " + std::to_string(available_encoders.size() - 10) + " more");
        }
        
        return AudioEncoderError::EncoderNotFound;
    }
    
    // Create codec context
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        ve::log::error("Failed to allocate codec context");
        return AudioEncoderError::OutOfMemory;
    }
    
    // Configure codec context
    codec_ctx_->sample_rate = config_.sample_rate;
    codec_ctx_->ch_layout.nb_channels = config_.channel_count;
    codec_ctx_->bit_rate = config_.bitrate;
    
    // Set sample format based on codec
    if (codec_->sample_fmts) {
        codec_ctx_->sample_fmt = codec_->sample_fmts[0];
    } else {
        codec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    
    // Set channel layout
    if (config_.channel_count == 1) {
        av_channel_layout_default(&codec_ctx_->ch_layout, 1);
    } else if (config_.channel_count == 2) {
        av_channel_layout_default(&codec_ctx_->ch_layout, 2);
    } else {
        av_channel_layout_default(&codec_ctx_->ch_layout, config_.channel_count);
    }
    
    set_codec_options();
    
    // Allocate frames and packet
    input_frame_ = av_frame_alloc();
    encoded_frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    
    if (!input_frame_ || !encoded_frame_ || !packet_) {
        ve::log::error("Failed to allocate frames or packet");
        return AudioEncoderError::OutOfMemory;
    }
    
    return AudioEncoderError::Success;
}

AudioEncoderError FFmpegAudioEncoder::init_resampler() {
    // Initialize resampler if format conversion is needed
    swr_ctx_ = swr_alloc();
    if (!swr_ctx_) {
        ve::log::error("Failed to allocate resampler context");
        return AudioEncoderError::OutOfMemory;
    }
    
    // Set resampler options
    av_opt_set_chlayout(swr_ctx_, "in_chlayout", &codec_ctx_->ch_layout, 0);
    av_opt_set_int(swr_ctx_, "in_sample_rate", config_.sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    
    av_opt_set_chlayout(swr_ctx_, "out_chlayout", &codec_ctx_->ch_layout, 0);
    av_opt_set_int(swr_ctx_, "out_sample_rate", codec_ctx_->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", codec_ctx_->sample_fmt, 0);
    
    int ret = swr_init(swr_ctx_);
    if (ret < 0) {
        ve::log::error("Failed to initialize resampler");
        return AudioEncoderError::EncoderInitFailed;
    }
    
    return AudioEncoderError::Success;
}

AudioEncoderError FFmpegAudioEncoder::init_output_file(const std::string& output_path,
                                                       const AudioMetadata& metadata) {
    // Create output format context
    int ret = avformat_alloc_output_context2(&format_ctx_, nullptr, nullptr, output_path.c_str());
    if (ret < 0) {
        ve::log::error("Failed to allocate output format context");
        return AudioEncoderError::OutputError;
    }
    
    // Create audio stream
    audio_stream_ = avformat_new_stream(format_ctx_, nullptr);
    if (!audio_stream_) {
        ve::log::error("Failed to create audio stream");
        return AudioEncoderError::OutputError;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(audio_stream_->codecpar, codec_ctx_);
    if (ret < 0) {
        ve::log::error("Failed to copy codec parameters");
        return AudioEncoderError::OutputError;
    }
    
    // Set metadata if provided
    if (config_.enable_metadata) {
        if (!metadata.title.empty()) {
            av_dict_set(&format_ctx_->metadata, "title", metadata.title.c_str(), 0);
        }
        if (!metadata.artist.empty()) {
            av_dict_set(&format_ctx_->metadata, "artist", metadata.artist.c_str(), 0);
        }
        if (!metadata.album.empty()) {
            av_dict_set(&format_ctx_->metadata, "album", metadata.album.c_str(), 0);
        }
        if (!metadata.genre.empty()) {
            av_dict_set(&format_ctx_->metadata, "genre", metadata.genre.c_str(), 0);
        }
        if (!metadata.comment.empty()) {
            av_dict_set(&format_ctx_->metadata, "comment", metadata.comment.c_str(), 0);
        }
        if (metadata.year > 0) {
            av_dict_set(&format_ctx_->metadata, "date", std::to_string(metadata.year).c_str(), 0);
        }
        if (metadata.track_number > 0) {
            av_dict_set(&format_ctx_->metadata, "track", std::to_string(metadata.track_number).c_str(), 0);
        }
    }
    
    // Open output file
    if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&format_ctx_->pb, output_path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            ve::log::error("Failed to open output file: " + output_path);
            return AudioEncoderError::OutputError;
        }
    }
    
    return AudioEncoderError::Success;
}

AudioEncoderError FFmpegAudioEncoder::encode_audio_frame(const std::shared_ptr<AudioFrame>& frame) {
    // Convert audio frame to AVFrame
    input_frame_->nb_samples = frame->sample_count();
    input_frame_->format = AV_SAMPLE_FMT_FLT;
    input_frame_->ch_layout = codec_ctx_->ch_layout;
    input_frame_->sample_rate = config_.sample_rate;
    
    // Allocate frame data
    int ret = av_frame_get_buffer(input_frame_, 0);
    if (ret < 0) {
        ve::log::error("Failed to allocate frame buffer");
        return AudioEncoderError::OutOfMemory;
    }
    
    // Copy audio data
    const float* input_data = reinterpret_cast<const float*>(frame->data());
    float* output_data = reinterpret_cast<float*>(input_frame_->data[0]);
    std::memcpy(output_data, input_data, frame->data_size());
    
    // Resample if needed
    if (swr_ctx_) {
        encoded_frame_->nb_samples = input_frame_->nb_samples;
        encoded_frame_->format = codec_ctx_->sample_fmt;
        encoded_frame_->ch_layout = codec_ctx_->ch_layout;
        
        ret = av_frame_get_buffer(encoded_frame_, 0);
        if (ret < 0) {
            ve::log::error("Failed to allocate encoded frame buffer");
            return AudioEncoderError::OutOfMemory;
        }
        
        ret = swr_convert(swr_ctx_, encoded_frame_->data, encoded_frame_->nb_samples,
                         (const uint8_t**)input_frame_->data, input_frame_->nb_samples);
        if (ret < 0) {
            ve::log::error("Failed to resample audio");
            return AudioEncoderError::EncodeFailed;
        }
        
        encoded_frame_->pts = next_pts_;
        next_pts_ += encoded_frame_->nb_samples;
    } else {
        input_frame_->pts = next_pts_;
        next_pts_ += input_frame_->nb_samples;
        encoded_frame_ = input_frame_;
    }
    
    // Send frame to encoder
    ret = avcodec_send_frame(codec_ctx_, encoded_frame_);
    if (ret < 0) {
        ve::log::error("Failed to send frame to encoder");
        return AudioEncoderError::EncodeFailed;
    }
    
    // Receive encoded packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            ve::log::error("Error during encoding");
            return AudioEncoderError::EncodeFailed;
        }
        
        // Write packet
        packet_->stream_index = audio_stream_->index;
        av_packet_rescale_ts(packet_, codec_ctx_->time_base, audio_stream_->time_base);
        
        ret = av_interleaved_write_frame(format_ctx_, packet_);
        av_packet_unref(packet_);
        
        if (ret < 0) {
            ve::log::error("Failed to write packet");
            return AudioEncoderError::OutputError;
        }
        
        stats_.frames_encoded++;
        stats_.bytes_written += packet_->size;
    }
    
    update_stats();
    return AudioEncoderError::Success;
}

void FFmpegAudioEncoder::set_codec_options() {
    if (!codec_ctx_) return;
    
    // Set format-specific options
    switch (format_) {
        case AudioExportFormat::MP3:
            if (config_.vbr_mode) {
                av_opt_set(codec_ctx_->priv_data, "b", "0", 0);
                av_opt_set_int(codec_ctx_->priv_data, "q:a", config_.quality, 0);
            }
            if (config_.joint_stereo && config_.channel_count == 2) {
                av_opt_set(codec_ctx_->priv_data, "joint_stereo", "1", 0);
            }
            break;
            
        case AudioExportFormat::AAC:
            if (config_.vbr_mode) {
                av_opt_set_int(codec_ctx_->priv_data, "vbr", config_.quality, 0);
            }
            break;
            
        case AudioExportFormat::FLAC:
            av_opt_set_int(codec_ctx_->priv_data, "compression_level", config_.compression_level, 0);
            break;
            
        case AudioExportFormat::OGG:
            if (config_.vbr_mode) {
                av_opt_set_double(codec_ctx_->priv_data, "b", config_.quality, 0);
            }
            break;
    }
    
    // Set thread count
    if (config_.thread_count > 0) {
        codec_ctx_->thread_count = config_.thread_count;
    }
}

void FFmpegAudioEncoder::update_stats() {
    auto now = std::chrono::steady_clock::now();
    stats_.total_time = now - encoding_start_;
    
    if (stats_.frames_encoded > 0) {
        double seconds = stats_.total_time.count();
        double frames_per_second = stats_.frames_encoded / seconds;
        double expected_fps = static_cast<double>(config_.sample_rate) / config_.buffer_size;
        stats_.encoding_speed = frames_per_second / expected_fps;
        
        if (stats_.bytes_written > 0 && seconds > 0) {
            stats_.average_bitrate = static_cast<uint32_t>((stats_.bytes_written * 8) / seconds);
        }
    }
}

void FFmpegAudioEncoder::cleanup() {
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
    }
    
    if (input_frame_) {
        av_frame_free(&input_frame_);
    }
    
    if (encoded_frame_ && encoded_frame_ != input_frame_) {
        av_frame_free(&encoded_frame_);
    }
    
    if (packet_) {
        av_packet_free(&packet_);
    }
    
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
    
    if (format_ctx_) {
        if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&format_ctx_->pb);
        }
        avformat_free_context(format_ctx_);
    }
    
    encoder_initialized_ = false;
}

std::string FFmpegAudioEncoder::get_version_info() {
#ifdef ENABLE_FFMPEG
    return std::string(av_version_info());
#else
    return "FFmpeg not available";
#endif
}

std::vector<std::string> FFmpegAudioEncoder::get_available_encoders() {
    std::vector<std::string> encoders;
    
#ifdef ENABLE_FFMPEG
    const AVCodec* codec = nullptr;
    void* iter = nullptr;
    
    while ((codec = av_codec_iterate(&iter))) {
        if (av_codec_is_encoder(codec) && codec->type == AVMEDIA_TYPE_AUDIO) {
            encoders.push_back(codec->name);
        }
    }
#endif
    
    return encoders;
}

std::string FFmpegAudioEncoder::get_codec_diagnostics() {
    std::string diagnostics;
    
#ifdef ENABLE_FFMPEG
    diagnostics += "=== FFmpeg Audio Codec Diagnostics ===\n";
    diagnostics += "FFmpeg Version: " + get_version_info() + "\n\n";
    
    // Check format support with fallback details
    diagnostics += "Format Support Analysis:\n";
    for (const auto& [format, fallback_codecs] : CODEC_FALLBACKS) {
        std::string format_name = encoder_utils::format_to_string(format);
        diagnostics += "  " + format_name + ":\n";
        
        bool format_supported = false;
        for (const auto& codec_name : fallback_codecs) {
            const AVCodec* codec = avcodec_find_encoder_by_name(codec_name.c_str());
            bool available = (codec != nullptr);
            
            diagnostics += "    - " + codec_name + ": " + 
                          (available ? "AVAILABLE" : "NOT FOUND");
            if (available && !format_supported) {
                diagnostics += " (SELECTED)";
                format_supported = true;
            }
            diagnostics += "\n";
        }
        
        diagnostics += "    Status: " + std::string(format_supported ? "SUPPORTED" : "UNSUPPORTED") + "\n\n";
    }
    
    // List all available audio encoders
    auto available_encoders = get_available_encoders();
    diagnostics += "All Available Audio Encoders (" + std::to_string(available_encoders.size()) + " total):\n";
    
    // Group by common categories
    std::vector<std::string> mp3_encoders, aac_encoders, vorbis_encoders, flac_encoders, other_encoders;
    
    for (const auto& encoder : available_encoders) {
        if (encoder.find("mp3") != std::string::npos) {
            mp3_encoders.push_back(encoder);
        } else if (encoder.find("aac") != std::string::npos) {
            aac_encoders.push_back(encoder);
        } else if (encoder.find("vorbis") != std::string::npos || encoder.find("ogg") != std::string::npos) {
            vorbis_encoders.push_back(encoder);
        } else if (encoder.find("flac") != std::string::npos) {
            flac_encoders.push_back(encoder);
        } else {
            other_encoders.push_back(encoder);
        }
    }
    
    auto add_encoder_group = [&](const std::string& category, const std::vector<std::string>& encoders) {
        if (!encoders.empty()) {
            diagnostics += "  " + category + ": ";
            for (size_t i = 0; i < encoders.size(); ++i) {
                if (i > 0) diagnostics += ", ";
                diagnostics += encoders[i];
            }
            diagnostics += "\n";
        }
    };
    
    add_encoder_group("MP3", mp3_encoders);
    add_encoder_group("AAC", aac_encoders);
    add_encoder_group("Vorbis/OGG", vorbis_encoders);
    add_encoder_group("FLAC", flac_encoders);
    add_encoder_group("Other", other_encoders);
    
#else
    diagnostics = "FFmpeg support not compiled in - codec diagnostics unavailable";
#endif
    
    return diagnostics;
}

std::vector<std::string> FFmpegAudioEncoder::get_supported_formats() {
    std::vector<std::string> formats;
    
    // Check each format using the enhanced fallback system
    for (const auto& [format, fallback_codecs] : CODEC_FALLBACKS) {
        bool format_supported = false;
        std::string selected_codec;
        
        // Find the first available codec for this format
        for (const auto& codec_name : fallback_codecs) {
            const AVCodec* codec = avcodec_find_encoder_by_name(codec_name.c_str());
            if (codec) {
                format_supported = true;
                selected_codec = codec_name;
                break;
            }
        }
        
        if (format_supported) {
            std::string format_str = encoder_utils::format_to_string(format) + 
                                   " (using " + selected_codec + ")";
            formats.push_back(format_str);
        }
    }
    
    return formats;
}

bool FFmpegAudioEncoder::is_format_supported(AudioExportFormat format) {
    auto fallback_it = CODEC_FALLBACKS.find(format);
    if (fallback_it == CODEC_FALLBACKS.end()) {
        return false;
    }
    
    // Check if any codec in the fallback chain is available
    for (const auto& codec_name : fallback_it->second) {
        const AVCodec* codec = avcodec_find_encoder_by_name(codec_name.c_str());
        if (codec) {
            return true;
        }
    }
    
    return false;
}

std::string FFmpegAudioEncoder::get_codec_name(AudioExportFormat format) {
    auto fallback_it = CODEC_FALLBACKS.find(format);
    if (fallback_it == CODEC_FALLBACKS.end()) {
        return "";
    }
    
    // Return the first available codec name
    for (const auto& codec_name : fallback_it->second) {
        const AVCodec* codec = avcodec_find_encoder_by_name(codec_name.c_str());
        if (codec) {
            return codec_name;
        }
    }
    
    return "";
}

#else // !ENABLE_FFMPEG

// Stub implementations when FFmpeg is disabled

FFmpegAudioEncoder::FFmpegAudioEncoder() = default;
FFmpegAudioEncoder::~FFmpegAudioEncoder() = default;

std::unique_ptr<FFmpegAudioEncoder> FFmpegAudioEncoder::create(
    AudioExportFormat, const AudioEncoderConfig&) {
    ve::log::error("FFmpeg support not enabled");
    return nullptr;
}

std::unique_ptr<FFmpegAudioEncoder> FFmpegAudioEncoder::create_with_codec(
    const std::string&, const AudioEncoderConfig&) {
    ve::log::error("FFmpeg support not enabled");
    return nullptr;
}

AudioEncoderError FFmpegAudioEncoder::initialize(const std::string&, const AudioMetadata&) {
    return AudioEncoderError::EncoderNotFound;
}

AudioEncoderError FFmpegAudioEncoder::encode_frame(const std::shared_ptr<AudioFrame>&) {
    return AudioEncoderError::EncoderNotFound;
}

AudioEncoderError FFmpegAudioEncoder::finalize() {
    return AudioEncoderError::EncoderNotFound;
}

std::vector<std::string> FFmpegAudioEncoder::get_supported_formats() {
    return {};
}

bool FFmpegAudioEncoder::is_format_supported(AudioExportFormat) {
    return false;
}

std::string FFmpegAudioEncoder::get_codec_name(AudioExportFormat) {
    return "";
}

#endif // ENABLE_FFMPEG

// Factory implementations

std::unique_ptr<FFmpegAudioEncoder> AudioEncoderFactory::create_for_export(
    AudioExportFormat format,
    const std::string& output_path,
    const AudioEncoderConfig& config,
    const AudioMetadata& metadata) {
    
    auto encoder = FFmpegAudioEncoder::create(format, config);
    if (!encoder) {
        return nullptr;
    }
    
    AudioEncoderError result = encoder->initialize(output_path, metadata);
    if (result != AudioEncoderError::Success) {
        ve::log::error("Failed to initialize encoder for export");
        return nullptr;
    }
    
    return encoder;
}

AudioEncoderConfig AudioEncoderFactory::get_default_config(AudioExportFormat format) {
    auto config_it = DEFAULT_CONFIGS.find(format);
    return (config_it != DEFAULT_CONFIGS.end()) ? config_it->second : AudioEncoderConfig{};
}

AudioEncoderConfig AudioEncoderFactory::get_quality_config(AudioExportFormat format,
                                                           const std::string& preset) {
    AudioEncoderConfig config = get_default_config(format);
    
    if (preset == "broadcast") {
        config.sample_rate = 48000;
        config.bit_depth = 24;
        config.bitrate = (format == AudioExportFormat::FLAC) ? 0 : 320000;
        config.quality = 9;
    } else if (preset == "web") {
        config.sample_rate = 44100;
        config.bit_depth = 16;
        config.bitrate = (format == AudioExportFormat::FLAC) ? 0 : 192000;
        config.quality = 5;
    } else if (preset == "archive") {
        config.sample_rate = 96000;
        config.bit_depth = 32;
        config.bitrate = (format == AudioExportFormat::FLAC) ? 0 : 480000;
        config.quality = 10;
    }
    
    return config;
}

// Utility functions

namespace encoder_utils {
    std::string format_to_string(AudioExportFormat format) {
        switch (format) {
            case AudioExportFormat::MP3: return "MP3";
            case AudioExportFormat::AAC: return "AAC";
            case AudioExportFormat::FLAC: return "FLAC";
            case AudioExportFormat::OGG: return "OGG";
            default: return "Unknown";
        }
    }

    AudioExportFormat string_to_format(const std::string& format_str) {
        if (format_str == "MP3") return AudioExportFormat::MP3;
        if (format_str == "AAC") return AudioExportFormat::AAC;
        if (format_str == "FLAC") return AudioExportFormat::FLAC;
        if (format_str == "OGG") return AudioExportFormat::OGG;
        return AudioExportFormat::MP3; // Default
    }

    std::string get_file_extension(AudioExportFormat format) {
        switch (format) {
            case AudioExportFormat::MP3: return ".mp3";
            case AudioExportFormat::AAC: return ".m4a";
            case AudioExportFormat::FLAC: return ".flac";
            case AudioExportFormat::OGG: return ".ogg";
            default: return ".audio";
        }
    }

    uint32_t estimate_bitrate(const AudioEncoderConfig& config, bool compressed) {
        if (!compressed) {
            // Uncompressed PCM
            return config.sample_rate * config.channel_count * config.bit_depth;
        }
        
        return config.bitrate;
    }
} // namespace encoder_utils

} // namespace ve::audio