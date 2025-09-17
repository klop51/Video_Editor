#include "audio/test_decoder.hpp"
#include <core/log.hpp>
#include <cmath>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ve::audio {

AudioError TestAudioDecoder::initialize(const std::vector<uint8_t>& /* stream_data */) {
    // Initialize test decoder with default stereo 48kHz stream
    stream_info_.sample_rate = 48000;
    stream_info_.channel_count = 2;
    stream_info_.channel_layout = ChannelLayout::Stereo;
    stream_info_.preferred_format = SampleFormat::Float32;
    stream_info_.duration_samples = 48000 * 10; // 10 seconds of test audio
    stream_info_.codec = AudioCodec::PCM;
    stream_info_.bit_rate = 48000 * 2 * 32; // 48kHz * 2 channels * 32 bits
    
    last_error_ = AudioError::None;
    initialized_ = true;
    
    ve::log::info("TestAudioDecoder initialized: 48kHz stereo, 10 seconds duration");
    return AudioError::None;
}

AudioStreamInfo TestAudioDecoder::get_stream_info() const {
    if (!initialized_) {
        ve::log::warn("TestAudioDecoder::get_stream_info called before initialization");
        AudioStreamInfo error_info = {};
        return error_info;
    }
    return stream_info_;
}

std::shared_ptr<AudioFrame> TestAudioDecoder::decode_frame(
    const std::vector<uint8_t>& /* input_data */,
    const TimePoint& timestamp,
    SampleFormat output_format) {
    
    if (!initialized_) {
        last_error_ = AudioError::ConfigurationError;
        return nullptr;
    }
    
    // Generate 1024 samples (about 21ms at 48kHz)
    const uint32_t samples_per_frame = 1024;
    auto frame = AudioFrame::create(
        stream_info_.sample_rate,
        stream_info_.channel_count,
        samples_per_frame,
        output_format,
        timestamp
    );
    
    if (!frame) {
        last_error_ = AudioError::MemoryError;
        return nullptr;
    }
    
    // Generate test tone (440Hz sine wave)
    if (output_format == SampleFormat::Float32) {
        float* samples = static_cast<float*>(frame->data());
        generate_test_audio_data(samples, samples_per_frame * stream_info_.channel_count);
    }
    
    last_error_ = AudioError::None;
    return frame;
}

AudioError TestAudioDecoder::seek(const TimePoint& timestamp) {
    if (!initialized_) {
        return AudioError::ConfigurationError;
    }
    
    // For test decoder, seeking always succeeds
    std::ostringstream oss;
    oss << "TestAudioDecoder seek to timestamp: " << timestamp.to_rational().num << "/" << timestamp.to_rational().den;
    ve::log::info(oss.str());
    return AudioError::None;
}

void TestAudioDecoder::flush() {
    ve::log::info("TestAudioDecoder flush called");
    // Nothing to flush for test decoder
}

AudioCodec TestAudioDecoder::get_codec_type() const {
    return AudioCodec::PCM;
}

const char* TestAudioDecoder::get_codec_name() const {
    return "Test Audio Decoder (PCM)";
}

std::shared_ptr<AudioFrame> TestAudioDecoder::process_decode_request(const DecodeRequest& request) {
    if (!initialized_) {
        last_error_ = AudioError::ConfigurationError;
        return nullptr;
    }
    
    // Generate test audio frame for the requested timestamp
    const uint32_t samples_per_frame = request.frame_count > 0 ? request.frame_count : 1024;
    
    auto frame = AudioFrame::create(
        stream_info_.sample_rate,
        stream_info_.channel_count,
        samples_per_frame,
        request.output_format,
        request.timestamp
    );
    
    if (!frame) {
        last_error_ = AudioError::MemoryError;
        return nullptr;
    }
    
    // Generate test audio data
    if (request.output_format == SampleFormat::Float32) {
        float* samples = static_cast<float*>(frame->data());
        generate_test_audio_data(samples, samples_per_frame * stream_info_.channel_count);
    }
    
    last_error_ = AudioError::None;
    return frame;
}

void TestAudioDecoder::generate_test_audio_data(float* samples, uint32_t sample_count, float frequency) {
    static float phase = 0.0f;
    const float phase_increment = 2.0f * static_cast<float>(M_PI) * frequency / stream_info_.sample_rate;
    
    for (uint32_t i = 0; i < sample_count; i += stream_info_.channel_count) {
        float sample_value = 0.1f * std::sin(phase); // Low amplitude sine wave
        
        // Fill all channels with the same sample
        for (uint32_t ch = 0; ch < stream_info_.channel_count; ++ch) {
            samples[i + ch] = sample_value;
        }
        
        phase += phase_increment;
        if (phase >= 2.0f * static_cast<float>(M_PI)) {
            phase -= 2.0f * static_cast<float>(M_PI);
        }
    }
}

} // namespace ve::audio