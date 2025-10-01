/**
 * @file itu_downmix_validation.cpp
 * @brief Test ITU downmix matrix implementation for proper voice/background balance
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

// Audio components
#include "audio/audio_pipeline.hpp"
#include "audio/audio_frame.hpp"
#include "core/log.hpp"
#include "core/time.hpp"

using namespace ve::audio;

int main() {
    std::cout << "=== ITU Downmix Matrix Validation Test ===" << std::endl;
    
    // Create pipeline with stereo output
    AudioPipelineConfig config;
    config.sample_rate = 48000;
    config.channel_count = 2;  // Stereo output
    config.format = SampleFormat::Float32;
    config.enable_output = true;
    config.buffer_size = 1024;
    
    auto pipeline = AudioPipeline::create(config);
    if (!pipeline) {
        std::cerr << "Failed to create audio pipeline" << std::endl;
        return 1;
    }
    
    if (!pipeline->initialize()) {
        std::cerr << "Failed to initialize audio pipeline: " << pipeline->get_last_error() << std::endl;
        return 1;
    }
    
    std::cout << "✓ Audio pipeline initialized successfully" << std::endl;
    
    // Create a test 5.1 surround audio frame with different channel content
    const uint32_t frame_samples = 480;  // 10ms at 48kHz
    const uint16_t input_channels = 6;    // 5.1 surround
    
    auto test_frame = AudioFrame::create(
        48000,      // sample rate
        input_channels,  // 5.1 channels
        frame_samples,   // 10ms worth
        SampleFormat::Float32,
        ve::TimePoint()
    );
    
    if (!test_frame) {
        std::cerr << "Failed to create test audio frame" << std::endl;
        return 1;
    }
    
    // Fill test frame with different content per channel
    // L=0.8, R=0.8, C=1.0 (voice), LFE=0.3, SL=0.4, SR=0.4 (ambience)
    float* samples = static_cast<float*>(test_frame->data());
    for (uint32_t sample = 0; sample < frame_samples; ++sample) {
        samples[sample * 6 + 0] = 0.8f;  // L
        samples[sample * 6 + 1] = 0.8f;  // R
        samples[sample * 6 + 2] = 1.0f;  // C (dialog/voice)
        samples[sample * 6 + 3] = 0.3f;  // LFE
        samples[sample * 6 + 4] = 0.4f;  // SL (surround/ambience)
        samples[sample * 6 + 5] = 0.4f;  // SR (surround/ambience)
    }
    
    std::cout << "✓ Created 5.1 test frame with:" << std::endl;
    std::cout << "  - L/R: 0.8 (front stereo)" << std::endl;
    std::cout << "  - C: 1.0 (dialog/voice - should stay clear)" << std::endl;
    std::cout << "  - LFE: 0.3 (bass)" << std::endl;
    std::cout << "  - SL/SR: 0.4 (ambience - should be balanced, not weak)" << std::endl;
    
    // Start audio output
    if (!pipeline->start_output()) {
        std::cerr << "Failed to start audio output: " << pipeline->get_last_error() << std::endl;
        return 1;
    }
    
    std::cout << "✓ Audio output started" << std::endl;
    
    // Process several frames to test downmix
    std::cout << "\n=== Processing test frames with ITU downmix ===" << std::endl;
    
    for (int i = 0; i < 100; ++i) {  // ~1 second of audio
        if (!pipeline->process_audio_frame(test_frame)) {
            std::cerr << "Failed to process audio frame " << i << std::endl;
            break;
        }
        
        if (i % 20 == 0) {
            std::cout << "Processed frame " << i << "/100" << std::endl;
        }
        
        // Sleep to simulate real-time playback
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Get pipeline statistics
    auto stats = pipeline->get_stats();
    std::cout << "\n=== Pipeline Statistics ===" << std::endl;
    std::cout << "Frames processed: " << stats.total_frames_processed << std::endl;
    std::cout << "Samples processed: " << stats.total_samples_processed << std::endl;
    std::cout << "Buffer underruns: " << stats.buffer_underruns << std::endl;
    std::cout << "Buffer overruns: " << stats.buffer_overruns << std::endl;
    
    // Keep playing for a moment to hear the result
    std::cout << "\n✓ Playing downmixed audio... (listen for clear voice + balanced background)" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Stop pipeline
    pipeline->stop_output();
    pipeline->shutdown();
    
    std::cout << "\n=== ITU Downmix Matrix Validation Complete ===" << std::endl;
    std::cout << "Expected results:" << std::endl;
    std::cout << "- Voice (center channel) should be clear and prominent" << std::endl;
    std::cout << "- Background (surround channels) should be present, not weak/distorted" << std::endl;
    std::cout << "- Overall balance should sound natural, not 'pumping'" << std::endl;
    
    return 0;
}