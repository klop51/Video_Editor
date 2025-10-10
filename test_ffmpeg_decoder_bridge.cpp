/**
 * @file test_ffmpeg_decoder_bridge.cpp
 * @brief Basic test for FFmpegDecoderBridge functionality
 */

#include "audio/ffmpeg_decoder_bridge.hpp"
#include <iostream>
#include <core/log.hpp>

int main() {
    ve::log::info("Testing FFmpegDecoderBridge...");

    // Test 1: Create decoder with test WAV file
    auto decoder = ve::audio::FFmpegDecoderBridge::create("test.wav");
    if (!decoder) {
        ve::log::error("Failed to create FFmpegDecoderBridge");
        return 1;
    }

    ve::log::info("✅ FFmpegDecoderBridge created successfully");

    // Test 2: Initialize decoder
    std::vector<uint8_t> dummy_data;
    auto error = decoder->initialize(dummy_data);
    if (error != ve::audio::AudioError::None) {
        ve::log::error("Failed to initialize decoder: " + std::to_string(static_cast<int>(error)));
        return 1;
    }

    ve::log::info("✅ FFmpegDecoderBridge initialized successfully");

    // Test 3: Get stream info
    auto stream_info = decoder->get_stream_info();
    ve::log::info("Stream info - Sample rate: " + std::to_string(stream_info.sample_rate) +
                  ", Channels: " + std::to_string(stream_info.channel_count) +
                  ", Codec: " + std::string(decoder->get_codec_name()));

    // Test 4: Get codec info
    ve::log::info("Codec type: " + std::to_string(static_cast<int>(decoder->get_codec_type())) +
                  ", Name: " + std::string(decoder->get_codec_name()));

    ve::log::info("✅ FFmpegDecoderBridge basic tests passed!");
    return 0;
}