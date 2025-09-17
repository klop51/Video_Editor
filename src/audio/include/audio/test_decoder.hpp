#pragma once

#include "decoder.hpp"

namespace ve::audio {

/**
 * @brief Simple test decoder for Audio Engine Phase 1 Week 1 validation
 * 
 * This decoder generates test audio frames to validate the audio infrastructure
 * without requiring actual codec implementations.
 */
class TestAudioDecoder : public ThreadedAudioDecoder {
public:
    TestAudioDecoder() = default;
    ~TestAudioDecoder() override = default;

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

protected:
    // ThreadedAudioDecoder interface implementation
    std::shared_ptr<AudioFrame> process_decode_request(const DecodeRequest& request) override;

private:
    AudioStreamInfo stream_info_;
    AudioError last_error_ = AudioError::None;
    bool initialized_ = false;
    
    // Test data generation
    void generate_test_audio_data(float* samples, uint32_t sample_count, float frequency = 440.0f);
};

} // namespace ve::audio