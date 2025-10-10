#pragma once

#include "audio/decoder.hpp"
#include "audio/ffmpeg_audio_decoder.hpp"
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

namespace ve::audio {

/**
 * @brief FFmpeg-based audio decoder that implements the AudioDecoder interface
 *
 * This class bridges the AudioDecoder interface with the FFmpegAudioDecoder,
 * providing file-based audio decoding capabilities for the audio engine.
 *
 * Supports WAV, MP3, AAC, FLAC and other FFmpeg-supported formats.
 */
class FFmpegDecoderBridge : public ThreadedAudioDecoder {
public:
    /**
     * @brief Create decoder for audio file
     *
     * @param file_path Path to audio file
     * @return Decoder instance or nullptr on failure
     */
    static std::unique_ptr<FFmpegDecoderBridge> create(const std::string& file_path);

    ~FFmpegDecoderBridge() override;

    // AudioDecoder interface implementation
    AudioError initialize(const std::vector<uint8_t>& stream_data) override;
    AudioStreamInfo get_stream_info() const override;
    std::shared_ptr<AudioFrame> decode_frame(
        const std::vector<uint8_t>& input_data,
        const TimePoint& timestamp,
        SampleFormat output_format = SampleFormat::Float32
    ) override;
    AudioError last_error() const override { return last_error_; }
    AudioError seek(const TimePoint& timestamp) override;
    void flush() override;
    AudioCodec get_codec_type() const override;
    const char* get_codec_name() const override;

    // Hardware acceleration support
    bool supports_hardware_acceleration() const override;

protected:
    // ThreadedAudioDecoder interface implementation
    std::shared_ptr<AudioFrame> process_decode_request(const DecodeRequest& request) override;

private:
    /**
     * @brief Private constructor - use create() method
     */
    FFmpegDecoderBridge();

    /**
     * @brief Load audio file and extract stream information
     */
    AudioError load_file(const std::string& file_path);

    /**
     * @brief Convert FFmpeg sample format to our SampleFormat
     */
    SampleFormat convert_ffmpeg_format(int ffmpeg_format) const;

    /**
     * @brief Convert our SampleFormat to FFmpeg format
     */
    int convert_to_ffmpeg_format(SampleFormat format) const;

    /**
     * @brief Calculate timestamp from sample position
     */
    TimePoint calculate_timestamp(uint64_t sample_position) const;

    /**
     * @brief Calculate sample position from timestamp
     */
    uint64_t calculate_sample_position(const TimePoint& timestamp) const;

    // Member variables
    std::string file_path_;
    AudioStreamInfo stream_info_;
    AudioError last_error_ = AudioError::None;
    bool initialized_ = false;

    // FFmpeg decoder instance
    std::unique_ptr<FFmpegAudioDecoder> ffmpeg_decoder_;

    // File data for initialization
    std::vector<uint8_t> file_data_;

    // Current playback position (in samples)
    uint64_t current_sample_position_ = 0;

    // Decoder configuration
    AudioDecoderConfig decoder_config_;
};

} // namespace ve::audio