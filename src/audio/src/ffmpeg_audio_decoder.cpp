#include "audio/ffmpeg_audio_decoder.hpp"
#include <core/log.hpp>
#include <algorithm>
#include <chrono>
#include <cstring>

namespace ve::audio {

namespace {

/**
 * @brief Convert FFmpeg error code to string
 */
std::string ffmpeg_error_string(int error_code) {
#ifdef ENABLE_FFMPEG
    char error_buf[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_buf, AV_ERROR_MAX_STRING_SIZE, error_code);
    return std::string(error_buf);
#else
    return "FFmpeg disabled (error code: " + std::to_string(error_code) + ")";
#endif
}

} // anonymous namespace

#ifdef ENABLE_FFMPEG

FFmpegAudioDecoder::FFmpegAudioDecoder() = default;

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
    cleanup();
}

std::unique_ptr<FFmpegAudioDecoder> FFmpegAudioDecoder::create(
    const media_io::StreamInfo& stream_info,
    const AudioDecoderConfig& config) {
    
    if (stream_info.type != media_io::StreamInfo::Audio) {
        ve::log::error("Stream is not audio type");
        return nullptr;
    }
    
    auto decoder = std::unique_ptr<FFmpegAudioDecoder>(new FFmpegAudioDecoder());
    decoder->config_ = config;
    
    AudioDecoderError result = decoder->initialize(stream_info);
    if (result != AudioDecoderError::Success) {
        ve::log::error("Failed to initialize audio decoder");
        return nullptr;
    }
    
    return decoder;
}

std::unique_ptr<FFmpegAudioDecoder> FFmpegAudioDecoder::create_from_params(
    int codec_id,
    const std::vector<uint8_t>& codec_params,
    const AudioDecoderConfig& config) {
    
    // Create a temporary stream info from codec parameters
    media_io::StreamInfo stream_info;
    stream_info.type = media_io::StreamInfo::Audio;
    
    // Find codec name
    const AVCodec* codec = avcodec_find_decoder(static_cast<AVCodecID>(codec_id));
    if (!codec) {
        ve::log::error("Codec not found for ID: " + std::to_string(codec_id));
        return nullptr;
    }
    
    stream_info.codec_name = codec->name;
    
    auto decoder = std::unique_ptr<FFmpegAudioDecoder>(new FFmpegAudioDecoder());
    decoder->config_ = config;
    
    AudioDecoderError result = decoder->init_decoder(stream_info);
    if (result != AudioDecoderError::Success) {
        return nullptr;
    }
    
    // Set codec parameters if provided
    if (!codec_params.empty() && decoder->codec_ctx_) {
        decoder->codec_ctx_->extradata_size = static_cast<int>(codec_params.size());
        decoder->codec_ctx_->extradata = static_cast<uint8_t*>(av_malloc(codec_params.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        std::memcpy(decoder->codec_ctx_->extradata, codec_params.data(), codec_params.size());
        std::memset(decoder->codec_ctx_->extradata + codec_params.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
    }
    
    return decoder;
}

AudioDecoderError FFmpegAudioDecoder::initialize(const media_io::StreamInfo& stream_info) {
    stream_info_ = stream_info;
    
    AudioDecoderError result = init_decoder(stream_info);
    if (result != AudioDecoderError::Success) {
        return result;
    }
    
    if (config_.enable_resampling) {
        result = init_resampler();
        if (result != AudioDecoderError::Success) {
            return result;
        }
    }
    
    decoder_initialized_ = true;
    ve::log::info("Audio decoder initialized for codec: " + stream_info.codec_name);
    
    return AudioDecoderError::Success;
}

AudioDecoderError FFmpegAudioDecoder::init_decoder(const media_io::StreamInfo& stream_info) {
    // Find decoder
    codec_ = avcodec_find_decoder_by_name(stream_info.codec_name.c_str());
    if (!codec_) {
        ve::log::error("Decoder not found for codec: " + stream_info.codec_name);
        return AudioDecoderError::DecoderNotFound;
    }
    
    // Allocate codec context
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        ve::log::error("Failed to allocate codec context");
        return AudioDecoderError::OutOfMemory;
    }
    
    // Set basic parameters from stream info
    codec_ctx_->sample_rate = stream_info.sample_rate;
    
    // Use channel layout instead of deprecated channels field
    if (stream_info.channels == 1) {
        av_channel_layout_default(&codec_ctx_->ch_layout, 1);
    } else if (stream_info.channels == 2) {
        av_channel_layout_default(&codec_ctx_->ch_layout, 2);
    } else {
        av_channel_layout_default(&codec_ctx_->ch_layout, stream_info.channels);
    }
    
    // Set sample format if known
    if (!stream_info.sample_format.empty()) {
        AVSampleFormat fmt = av_get_sample_fmt(stream_info.sample_format.c_str());
        if (fmt != AV_SAMPLE_FMT_NONE) {
            codec_ctx_->sample_fmt = fmt;
        }
    }
    
    // Open codec
    int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        ve::log::error("Failed to open codec: " + ffmpeg_error_string(ret));
        return AudioDecoderError::DecoderInitFailed;
    }
    
    // Allocate frame for input
    input_frame_ = av_frame_alloc();
    if (!input_frame_) {
        ve::log::error("Failed to allocate input frame");
        return AudioDecoderError::OutOfMemory;
    }
    
    // Set time base for timestamp calculations
    time_base_ = {1, static_cast<int>(stream_info.sample_rate)};
    
    ve::log::info("Decoder initialized: " + 
                  std::to_string(codec_ctx_->sample_rate) + "Hz, " +
                  std::to_string(codec_ctx_->ch_layout.nb_channels) + " channels, " +
                  av_get_sample_fmt_name(codec_ctx_->sample_fmt));
    
    return AudioDecoderError::Success;
}

AudioDecoderError FFmpegAudioDecoder::init_resampler() {
    if (!codec_ctx_) {
        return AudioDecoderError::InvalidInput;
    }
    
    // Check if resampling is actually needed
    bool needs_resample = (
        static_cast<uint32_t>(codec_ctx_->sample_rate) != config_.target_sample_rate ||
        static_cast<uint16_t>(codec_ctx_->ch_layout.nb_channels) != config_.target_channels ||
        convert_ffmpeg_format(codec_ctx_->sample_fmt) != config_.target_format
    );
    
    if (!needs_resample) {
        ve::log::info("No resampling needed");
        return AudioDecoderError::Success;
    }
    
    // Create resampler context
    swr_ctx_ = swr_alloc();
    if (!swr_ctx_) {
        ve::log::error("Failed to allocate resampler context");
        return AudioDecoderError::OutOfMemory;
    }
    
    // Configure input channel layout
    av_opt_set_chlayout(swr_ctx_, "in_chlayout", &codec_ctx_->ch_layout, 0);
    av_opt_set_int(swr_ctx_, "in_sample_rate", codec_ctx_->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", codec_ctx_->sample_fmt, 0);
    
    // Configure output channel layout
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, config_.target_channels);
    av_opt_set_chlayout(swr_ctx_, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(swr_ctx_, "out_sample_rate", config_.target_sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", convert_to_ffmpeg_format(config_.target_format), 0);
    
    // Set quality
    av_opt_set_int(swr_ctx_, "filter_size", config_.resample_quality * 16, 0);
    av_opt_set_int(swr_ctx_, "linear_interp", 1, 0);
    
    // Initialize resampler
    int ret = swr_init(swr_ctx_);
    if (ret < 0) {
        ve::log::error("Failed to initialize resampler: " + ffmpeg_error_string(ret));
        return AudioDecoderError::ResamplerInitFailed;
    }
    
    // Allocate output frame
    output_frame_ = av_frame_alloc();
    if (!output_frame_) {
        ve::log::error("Failed to allocate output frame");
        return AudioDecoderError::OutOfMemory;
    }
    
    output_frame_->format = convert_to_ffmpeg_format(config_.target_format);
    av_channel_layout_default(&output_frame_->ch_layout, config_.target_channels);
    output_frame_->sample_rate = config_.target_sample_rate;
    output_frame_->nb_samples = config_.max_frame_size;
    
    ret = av_frame_get_buffer(output_frame_, 0);
    if (ret < 0) {
        ve::log::error("Failed to allocate output frame buffer: " + ffmpeg_error_string(ret));
        return AudioDecoderError::OutOfMemory;
    }
    
    ve::log::info("Resampler initialized: " +
                  std::to_string(codec_ctx_->sample_rate) + "Hz→" + std::to_string(config_.target_sample_rate) + "Hz, " +
                  std::to_string(codec_ctx_->ch_layout.nb_channels) + "→" + std::to_string(config_.target_channels) + " channels");
    
    return AudioDecoderError::Success;
}

AudioDecoderError FFmpegAudioDecoder::decode_packet(
    const media_io::Packet& packet,
    std::shared_ptr<AudioFrame>& frame) {
    
    if (!decoder_initialized_) {
        return AudioDecoderError::InvalidInput;
    }
    
    frame = nullptr;
    
    auto decode_start = std::chrono::high_resolution_clock::now();
    
    // Send packet to decoder
    AVPacket av_packet;
    av_init_packet(&av_packet);
    av_packet.data = const_cast<uint8_t*>(packet.data.data());
    av_packet.size = static_cast<int>(packet.data.size());
    av_packet.pts = packet.pts.to_rational().num * time_base_.den / packet.pts.to_rational().den;
    av_packet.dts = packet.dts.to_rational().num * time_base_.den / packet.dts.to_rational().den;
    
    int ret = avcodec_send_packet(codec_ctx_, &av_packet);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            return AudioDecoderError::InsufficientData;
        } else if (ret == AVERROR_EOF) {
            return AudioDecoderError::EndOfStream;
        }
        
        stats_.decode_errors++;
        ve::log::error("Failed to send packet to decoder: " + ffmpeg_error_string(ret));
        return AudioDecoderError::DecodeFailed;
    }
    
    // Receive frame from decoder
    ret = avcodec_receive_frame(codec_ctx_, input_frame_);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            return AudioDecoderError::InsufficientData;
        } else if (ret == AVERROR_EOF) {
            return AudioDecoderError::EndOfStream;
        }
        
        stats_.decode_errors++;
        ve::log::error("Failed to receive frame from decoder: " + ffmpeg_error_string(ret));
        return AudioDecoderError::DecodeFailed;
    }
    
    auto decode_end = std::chrono::high_resolution_clock::now();
    auto decode_duration = std::chrono::duration_cast<std::chrono::microseconds>(decode_end - decode_start);
    
    // Update statistics
    stats_.packets_decoded++;
    stats_.bytes_processed += packet.data.size();
    stats_.samples_decoded += input_frame_->nb_samples;
    stats_.avg_decode_time_us = (stats_.avg_decode_time_us * (stats_.packets_decoded - 1) + decode_duration.count()) / stats_.packets_decoded;
    
    // Resample if needed
    if (swr_ctx_) {
        return resample_frame(frame);
    } else {
        // Direct conversion without resampling
        TimePoint timestamp = calculate_frame_timestamp(input_frame_->pts);
        
        frame = AudioFrame::create_from_data(
            input_frame_->sample_rate,
            static_cast<uint16_t>(input_frame_->ch_layout.nb_channels),
            input_frame_->nb_samples,
            convert_ffmpeg_format(static_cast<AVSampleFormat>(input_frame_->format)),
            timestamp,
            input_frame_->data[0],
            input_frame_->linesize[0]
        );
        
        if (!frame) {
            return AudioDecoderError::OutOfMemory;
        }
        
        stats_.frames_produced++;
        return AudioDecoderError::Success;
    }
}

AudioDecoderError FFmpegAudioDecoder::resample_frame(std::shared_ptr<AudioFrame>& output_frame) {
    if (!swr_ctx_ || !input_frame_ || !output_frame_) {
        return AudioDecoderError::InvalidInput;
    }
    
    auto resample_start = std::chrono::high_resolution_clock::now();
    
    // Calculate output sample count
    int64_t output_samples = swr_get_out_samples(swr_ctx_, input_frame_->nb_samples);
    if (output_samples <= 0) {
        stats_.resample_errors++;
        return AudioDecoderError::DecodeFailed;
    }
    
    // Ensure output frame is large enough
    if (output_samples > output_frame_->nb_samples) {
        output_frame_->nb_samples = static_cast<int>(output_samples);
        av_frame_unref(output_frame_);
        int ret = av_frame_get_buffer(output_frame_, 0);
        if (ret < 0) {
            stats_.resample_errors++;
            ve::log::error("Failed to reallocate output frame buffer: " + ffmpeg_error_string(ret));
            return AudioDecoderError::OutOfMemory;
        }
    }
    
    // Perform resampling
    int converted_samples = swr_convert(swr_ctx_,
                                       output_frame_->data, static_cast<int>(output_samples),
                                       const_cast<const uint8_t**>(input_frame_->data), input_frame_->nb_samples);
    
    if (converted_samples < 0) {
        stats_.resample_errors++;
        ve::log::error("Resampling failed: " + ffmpeg_error_string(converted_samples));
        return AudioDecoderError::DecodeFailed;
    }
    
    auto resample_end = std::chrono::high_resolution_clock::now();
    auto resample_duration = std::chrono::duration_cast<std::chrono::microseconds>(resample_end - resample_start);
    
    // Update statistics
    stats_.avg_resample_time_us = (stats_.avg_resample_time_us * (stats_.frames_produced) + resample_duration.count()) / (stats_.frames_produced + 1);
    
    // Calculate timestamp for output frame
    TimePoint timestamp = calculate_frame_timestamp(input_frame_->pts);
    
    // Create AudioFrame from resampled data
    size_t bytes_per_sample = AudioFrame::bytes_per_sample(config_.target_format);
    size_t total_bytes = converted_samples * config_.target_channels * bytes_per_sample;
    
    output_frame = AudioFrame::create_from_data(
        config_.target_sample_rate,
        config_.target_channels,
        converted_samples,
        config_.target_format,
        timestamp,
        output_frame_->data[0],
        total_bytes
    );
    
    if (!output_frame) {
        return AudioDecoderError::OutOfMemory;
    }
    
    stats_.frames_produced++;
    return AudioDecoderError::Success;
}

AudioDecoderError FFmpegAudioDecoder::flush(std::vector<std::shared_ptr<AudioFrame>>& frames) {
    if (!decoder_initialized_) {
        return AudioDecoderError::InvalidInput;
    }
    
    frames.clear();
    
    // Send null packet to flush decoder
    int ret = avcodec_send_packet(codec_ctx_, nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        ve::log::error("Failed to flush decoder: " + ffmpeg_error_string(ret));
        return AudioDecoderError::DecodeFailed;
    }
    
    // Receive remaining frames
    while (true) {
        ret = avcodec_receive_frame(codec_ctx_, input_frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            ve::log::error("Failed to receive flush frame: " + ffmpeg_error_string(ret));
            return AudioDecoderError::DecodeFailed;
        }
        
        std::shared_ptr<AudioFrame> frame;
        if (swr_ctx_) {
            AudioDecoderError result = resample_frame(frame);
            if (result != AudioDecoderError::Success) {
                return result;
            }
        } else {
            TimePoint timestamp = calculate_frame_timestamp(input_frame_->pts);
            
            frame = AudioFrame::create_from_data(
                input_frame_->sample_rate,
                static_cast<uint16_t>(input_frame_->ch_layout.nb_channels),
                input_frame_->nb_samples,
                convert_ffmpeg_format(static_cast<AVSampleFormat>(input_frame_->format)),
                timestamp,
                input_frame_->data[0],
                input_frame_->linesize[0]
            );
        }
        
        if (frame) {
            frames.push_back(frame);
        }
    }
    
    // Flush resampler if present
    if (swr_ctx_) {
        while (true) {
            int converted_samples = swr_convert(swr_ctx_,
                                               output_frame_->data, output_frame_->nb_samples,
                                               nullptr, 0);
            if (converted_samples <= 0) {
                break;
            }
            
            TimePoint timestamp = calculate_frame_timestamp(next_pts_);
            next_pts_ += converted_samples;
            
            size_t bytes_per_sample = AudioFrame::bytes_per_sample(config_.target_format);
            size_t total_bytes = converted_samples * config_.target_channels * bytes_per_sample;
            
            auto frame = AudioFrame::create_from_data(
                config_.target_sample_rate,
                config_.target_channels,
                converted_samples,
                config_.target_format,
                timestamp,
                output_frame_->data[0],
                total_bytes
            );
            
            if (frame) {
                frames.push_back(frame);
            }
        }
    }
    
    ve::log::info("Decoder flushed, " + std::to_string(frames.size()) + " remaining frames");
    return AudioDecoderError::Success;
}

void FFmpegAudioDecoder::reset() {
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
    }
    
    if (swr_ctx_) {
        // Reset resampler state
        swr_close(swr_ctx_);
        swr_init(swr_ctx_);
    }
    
    next_pts_ = 0;
    ve::log::debug("Decoder reset");
}

TimePoint FFmpegAudioDecoder::calculate_frame_timestamp(int64_t pts, int64_t sample_offset) {
    if (!config_.use_precise_timestamps || pts == AV_NOPTS_VALUE) {
        // Use sequential timestamp calculation
        TimePoint timestamp(next_pts_ + sample_offset, config_.target_sample_rate);
        next_pts_ += (swr_ctx_ ? output_frame_->nb_samples : input_frame_->nb_samples);
        return timestamp;
    }
    
    // Convert PTS to target sample rate timebase
    int64_t adjusted_pts = pts;
    if (codec_ctx_->sample_rate != static_cast<int>(config_.target_sample_rate)) {
        adjusted_pts = pts * config_.target_sample_rate / codec_ctx_->sample_rate;
    }
    
    return TimePoint(adjusted_pts + sample_offset, config_.target_sample_rate);
}

SampleFormat FFmpegAudioDecoder::convert_ffmpeg_format(AVSampleFormat av_format) {
    switch (av_format) {
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return SampleFormat::Int16;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return SampleFormat::Int32;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return SampleFormat::Float32;
        default:
            return SampleFormat::Unknown;
    }
}

AVSampleFormat FFmpegAudioDecoder::convert_to_ffmpeg_format(SampleFormat format) {
    switch (format) {
        case SampleFormat::Int16:
            return AV_SAMPLE_FMT_S16;
        case SampleFormat::Int32:
            return AV_SAMPLE_FMT_S32;
        case SampleFormat::Float32:
            return AV_SAMPLE_FMT_FLT;
        default:
            return AV_SAMPLE_FMT_NONE;
    }
}

void FFmpegAudioDecoder::cleanup() {
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
    }
    
    if (output_frame_) {
        av_frame_free(&output_frame_);
    }
    
    if (input_frame_) {
        av_frame_free(&input_frame_);
    }
    
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
    
    decoder_initialized_ = false;
}

std::vector<std::string> FFmpegAudioDecoder::get_supported_codecs() {
    std::vector<std::string> codecs;
    
    const AVCodec* codec = nullptr;
    void* opaque = nullptr;
    
    while ((codec = av_codec_iterate(&opaque))) {
        if (av_codec_is_decoder(codec) && codec->type == AVMEDIA_TYPE_AUDIO) {
            codecs.push_back(codec->name);
        }
    }
    
    return codecs;
}

bool FFmpegAudioDecoder::is_codec_supported(const std::string& codec_name) {
    const AVCodec* codec = avcodec_find_decoder_by_name(codec_name.c_str());
    return codec != nullptr && codec->type == AVMEDIA_TYPE_AUDIO;
}

#else // !ENABLE_FFMPEG

// Stub implementations when FFmpeg is disabled

FFmpegAudioDecoder::FFmpegAudioDecoder() = default;
FFmpegAudioDecoder::~FFmpegAudioDecoder() = default;

std::unique_ptr<FFmpegAudioDecoder> FFmpegAudioDecoder::create(
    const media_io::StreamInfo&,
    const AudioDecoderConfig&) {
    ve::log::error("FFmpeg support not enabled");
    return nullptr;
}

std::unique_ptr<FFmpegAudioDecoder> FFmpegAudioDecoder::create_from_params(
    int, const std::vector<uint8_t>&, const AudioDecoderConfig&) {
    ve::log::error("FFmpeg support not enabled");
    return nullptr;
}

AudioDecoderError FFmpegAudioDecoder::initialize(const media_io::StreamInfo&) {
    return AudioDecoderError::DecoderNotFound;
}

AudioDecoderError FFmpegAudioDecoder::decode_packet(
    const media_io::Packet&, std::shared_ptr<AudioFrame>&) {
    return AudioDecoderError::DecoderNotFound;
}

AudioDecoderError FFmpegAudioDecoder::flush(std::vector<std::shared_ptr<AudioFrame>>&) {
    return AudioDecoderError::DecoderNotFound;
}

void FFmpegAudioDecoder::reset() {}

std::vector<std::string> FFmpegAudioDecoder::get_supported_codecs() {
    return {};
}

bool FFmpegAudioDecoder::is_codec_supported(const std::string&) {
    return false;
}

void FFmpegAudioDecoder::cleanup() {}

#endif // ENABLE_FFMPEG

// Factory implementations
std::unique_ptr<FFmpegAudioDecoder> AudioDecoderFactory::create_for_file(
    const std::string& file_path,
    int audio_stream_index,
    const AudioDecoderConfig& config) {
    
    auto demuxer = media_io::Demuxer::create(file_path);
    if (!demuxer) {
        ve::log::error("Failed to create demuxer for: " + file_path);
        return nullptr;
    }
    
    auto streams = demuxer->get_streams();
    
    // Find audio stream
    media_io::StreamInfo audio_stream;
    bool found = false;
    
    if (audio_stream_index >= 0) {
        // Use specified stream
        for (const auto& stream : streams) {
            if (stream.index == audio_stream_index && stream.type == media_io::StreamInfo::Audio) {
                audio_stream = stream;
                found = true;
                break;
            }
        }
    } else {
        // Find first audio stream
        for (const auto& stream : streams) {
            if (stream.type == media_io::StreamInfo::Audio) {
                audio_stream = stream;
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        ve::log::error("No audio stream found in: " + file_path);
        return nullptr;
    }
    
    return FFmpegAudioDecoder::create(audio_stream, config);
}

std::unique_ptr<FFmpegAudioDecoder> AudioDecoderFactory::create_for_codec(
    const std::string& codec_name,
    uint32_t sample_rate,
    uint16_t channels,
    const AudioDecoderConfig& config) {
    
    media_io::StreamInfo stream_info;
    stream_info.type = media_io::StreamInfo::Audio;
    stream_info.codec_name = codec_name;
    stream_info.sample_rate = static_cast<int>(sample_rate);
    stream_info.channels = channels;
    
    return FFmpegAudioDecoder::create(stream_info, config);
}

// Utility functions
namespace decoder_utils {

const char* error_string(AudioDecoderError error) {
    switch (error) {
        case AudioDecoderError::Success: return "Success";
        case AudioDecoderError::InvalidInput: return "Invalid input";
        case AudioDecoderError::DecoderNotFound: return "Decoder not found";
        case AudioDecoderError::DecoderInitFailed: return "Decoder initialization failed";
        case AudioDecoderError::InvalidCodecParameters: return "Invalid codec parameters";
        case AudioDecoderError::ResamplerInitFailed: return "Resampler initialization failed";
        case AudioDecoderError::DecodeFailed: return "Decode failed";
        case AudioDecoderError::EndOfStream: return "End of stream";
        case AudioDecoderError::InsufficientData: return "Insufficient data";
        case AudioDecoderError::InvalidTimestamp: return "Invalid timestamp";
        case AudioDecoderError::OutOfMemory: return "Out of memory";
        default: return "Unknown error";
    }
}

size_t estimate_buffer_size(const media_io::StreamInfo& stream_info, double target_duration) {
    if (stream_info.type != media_io::StreamInfo::Audio) {
        return 0;
    }
    
    // Estimate based on bit rate or fall back to sample rate calculation
    if (stream_info.bit_rate > 0) {
        return static_cast<size_t>(stream_info.bit_rate * target_duration / 8.0);
    }
    
    // Fallback: assume 32-bit float samples
    size_t samples_per_second = stream_info.sample_rate * stream_info.channels;
    return static_cast<size_t>(samples_per_second * target_duration * 4);
}

bool needs_resampling(const media_io::StreamInfo& input_stream, const AudioDecoderConfig& target_config) {
    return (
        static_cast<uint32_t>(input_stream.sample_rate) != target_config.target_sample_rate ||
        static_cast<uint16_t>(input_stream.channels) != target_config.target_channels
    );
}

} // namespace decoder_utils

} // namespace ve::audio