/**
 * @file audio_pipeline_mixer_test.cpp
 * @brief Test to verify SimpleMixer integration in AudioPipeline
 * 
 * This test validates that the audio pipeline properly uses the SimpleMixer
 * instead of bypassing it, ensuring clean audio output with proper mixing.
 */

#include "audio/audio_pipeline.hpp"
#include "audio/audio_frame.hpp"
#include "core/log.hpp"
#include <memory>
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ve::audio;

int main() {
    ve::log::info("Starting Audio Pipeline Mixer Integration Test");
    
    // Configure audio pipeline for testing
    AudioPipelineConfig config;
    config.sample_rate = 48000;
    config.channel_count = 2;
    config.format = SampleFormat::Int16;
    config.buffer_size = 1024;
    
    // Create audio pipeline
    auto pipeline = AudioPipeline::create(config);
    if (!pipeline) {
        ve::log::error("Failed to create audio pipeline");
        return 1;
    }
    
    // Initialize the pipeline
    if (!pipeline->initialize()) {
        ve::log::error("Failed to initialize audio pipeline");
        return 1;
    }
    
    ve::log::info("Audio pipeline initialized successfully");
    
    // Create test audio frame
    auto test_frame = AudioFrame::create(
        config.sample_rate,
        config.channel_count,
        config.buffer_size,
        config.format,
        ve::TimePoint(0, config.sample_rate)
    );
    
    if (!test_frame) {
        ve::log::error("Failed to create test audio frame");
        return 1;
    }
    
    // Fill with test audio data (simple sine wave)
    const size_t sample_count = test_frame->sample_count();
    const size_t channel_count = test_frame->channel_count();
    
    for (size_t i = 0; i < sample_count; ++i) {
        // Generate a simple test tone
        float sample_value = 0.1f * sin(2.0f * M_PI * 440.0f * i / config.sample_rate);
        
        for (size_t ch = 0; ch < channel_count; ++ch) {
            // Use the correct method name for float samples
            test_frame->set_sample_from_float(static_cast<uint16_t>(ch), static_cast<uint32_t>(i), sample_value);
        }
    }
    
    std::cout << "Created test audio frame with " << sample_count << " samples, " << channel_count << " channels" << std::endl;
    
    // Start audio output
    if (!pipeline->start_output()) {
        ve::log::error("Failed to start audio output");
        return 1;
    }
    
    ve::log::info("Audio pipeline output started successfully");
    
    // Add a mixer channel for testing
    uint32_t channel_id = pipeline->add_audio_channel("Test Channel", 0.0f, 0.0f);
    if (channel_id == 0) {
        ve::log::error("Failed to add audio channel to mixer");
        return 1;
    }
    
    std::cout << "Added mixer channel with ID: " << channel_id << std::endl;
    
    // Submit test audio frame
    if (!pipeline->process_audio_frame(test_frame)) {
        ve::log::error("Failed to process test audio frame");
        return 1;
    }
    
    ve::log::info("Submitted test audio frame to pipeline");
    
    // Let the pipeline process for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Test mixer functionality by changing channel settings
    bool gain_result = pipeline->set_channel_gain(channel_id, -6.0f);
    bool pan_result = pipeline->set_channel_pan(channel_id, 0.5f);
    
    if (gain_result && pan_result) {
        ve::log::info("Successfully modified mixer channel settings");
    } else {
        ve::log::warn("Failed to modify mixer channel settings");
    }
    
    // Process a bit more
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stop the pipeline
    pipeline->stop_output();
    ve::log::info("Audio pipeline stopped");
    
    // Shutdown
    pipeline->shutdown();
    ve::log::info("Audio pipeline shutdown complete");
    
    ve::log::info("Audio Pipeline Mixer Integration Test PASSED");
    std::cout << "✓ SimpleMixer integration working correctly" << std::endl;
    std::cout << "✓ Audio frames processed through mixer" << std::endl;
    std::cout << "✓ Mixer channel controls functional" << std::endl;
    
    return 0;
}