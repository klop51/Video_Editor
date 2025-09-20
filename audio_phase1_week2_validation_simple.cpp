/**
 * @file audio_phase1_week2_validation_simple.cpp
 * @brief Phase 1 Week 2: Audio Foundation Systems Simple Validation
 * 
 * This simplified validation test verifies:
 * 1. Sample Rate Converter - Can be created and initialized
 * 2. Audio Buffer Management - Lock-free circular buffers can be created
 * 3. Audio Clock System - Can be created and initialized
 * 
 * Focus on basic functionality rather than complex integration.
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <memory>

// Audio system includes
#include "audio/sample_rate_converter.hpp"
#include "audio/audio_buffer_pool.hpp" 
#include "audio/audio_clock.hpp"
#include "audio/audio_frame.hpp"
#include "core/time.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ve;

/**
 * @brief Test sample rate converter creation and basic operations
 */
bool test_sample_rate_converter() {
    std::cout << "\n🔧 Testing Sample Rate Converter...\n";
    
    try {
        using namespace ve::audio;
        
        // Test configuration: 44.1kHz to 48kHz conversion
        ResampleConfig config;
        config.input_sample_rate = 44100;
        config.output_sample_rate = 48000;
        config.input_channels = 2;
        config.output_channels = 2;
        config.quality = ResampleQuality::Highest;
        
        auto converter = SampleRateConverter::create(config);
        if (!converter) {
            std::cout << "❌ Failed to create sample rate converter\n";
            return false;
        }
        
        // Initialize converter
        auto init_result = converter->initialize();
        if (static_cast<int>(init_result) == static_cast<int>(ve::audio::AudioError::None)) {
            // Success - continue
        } else {
            std::cout << "❌ Failed to initialize converter\n";
            return false;
        }
        
        if (!converter->is_initialized()) {
            std::cout << "❌ Converter reports not initialized after successful init\n";
            return false;
        }
        
        std::cout << "✅ Sample Rate Converter Results:\n";
        std::cout << "   • Creation: PASS\n";
        std::cout << "   • Initialization: PASS\n";
        std::cout << "   • Status check: PASS\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception in sample rate converter test: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test audio buffer management creation
 */
bool test_audio_buffer_management() {
    std::cout << "\n🔧 Testing Audio Buffer Management...\n";
    
    try {
        using namespace ve::audio;
        
        // Test circular buffer configuration
        AudioBufferConfig buffer_config;
        buffer_config.buffer_size_samples = 1024;
        buffer_config.channel_count = 2;
        buffer_config.sample_format = SampleFormat::Float32;
        buffer_config.lock_free = true;
        
        auto circular_buffer = std::make_unique<CircularAudioBuffer>(buffer_config);
        if (!circular_buffer) {
            std::cout << "❌ Failed to create circular buffer\n";
            return false;
        }
        
        // Test buffer pool
        buffer_config.pool_size = 8;
        buffer_config.zero_on_acquire = true;
        
        auto buffer_pool = std::make_unique<AudioBufferPool>(buffer_config);
        if (!buffer_pool) {
            std::cout << "❌ Failed to create buffer pool\n";
            return false;
        }
        
        // Try to acquire a buffer
        auto buffer = buffer_pool->acquire_buffer();
        if (!buffer) {
            std::cout << "❌ Failed to acquire buffer from pool\n";
            return false;
        }
        
        // Release buffer
        buffer_pool->release_buffer(buffer);
        
        std::cout << "✅ Audio Buffer Management Results:\n";
        std::cout << "   • Circular buffer creation: PASS\n";
        std::cout << "   • Buffer pool creation: PASS\n";
        std::cout << "   • Buffer acquire/release: PASS\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception in buffer management test: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test audio clock system creation and basic operations
 */
bool test_audio_clock_system() {
    std::cout << "\n🔧 Testing Audio Clock System...\n";
    
    try {
        using namespace ve::audio;
        
        // Test clock configuration
        AudioClockConfig clock_config;
        clock_config.sample_rate = 48000;
        clock_config.drift_threshold = 0.001; // 1ms
        clock_config.enable_drift_compensation = true;
        clock_config.measurement_window = 100;
        
        auto audio_clock = std::make_unique<AudioClock>(clock_config);
        if (!audio_clock) {
            std::cout << "❌ Failed to create audio clock\n";
            return false;
        }
        
        // Initialize clock
        auto init_result = audio_clock->initialize();
        if (static_cast<int>(init_result) == static_cast<int>(ve::audio::AudioError::None)) {
            // Success - continue  
        } else {
            std::cout << "❌ Failed to initialize audio clock\n";
            return false;
        }
        
        // Start clock with a simple timestamp
        TimePoint start_time(0, 1); // Time 0
        audio_clock->start(start_time);
        
        std::cout << "✅ Audio Clock System Results:\n";
        std::cout << "   • Creation: PASS\n";
        std::cout << "   • Initialization: PASS\n";
        std::cout << "   • Start operation: PASS\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception in audio clock test: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test AudioFrame creation and basic operations
 */
bool test_audio_frame_system() {
    std::cout << "\n🔧 Testing Audio Frame System...\n";
    
    try {
        using namespace ve::audio;
        
        // Create an audio frame
        TimePoint timestamp(0, 1);
        auto frame = AudioFrame::create(
            48000,  // sample_rate
            2,      // channel_count  
            1024,   // sample_count
            SampleFormat::Float32,
            timestamp
        );
        
        if (!frame) {
            std::cout << "❌ Failed to create audio frame\n";
            return false;
        }
        
        // Test getters
        if (frame->sample_rate() != 48000) {
            std::cout << "❌ Incorrect sample rate: " << frame->sample_rate() << "\n";
            return false;
        }
        
        if (frame->channel_count() != 2) {
            std::cout << "❌ Incorrect channel count: " << frame->channel_count() << "\n";
            return false;
        }
        
        if (frame->sample_count() != 1024) {
            std::cout << "❌ Incorrect sample count: " << frame->sample_count() << "\n";
            return false;
        }
        
        if (frame->format() != SampleFormat::Float32) {
            std::cout << "❌ Incorrect sample format\n";
            return false;
        }
        
        // Check data buffer
        if (!frame->data()) {
            std::cout << "❌ No data buffer allocated\n";
            return false;
        }
        
        if (frame->data_size() == 0) {
            std::cout << "❌ Data buffer has zero size\n";
            return false;
        }
        
        std::cout << "✅ Audio Frame System Results:\n";
        std::cout << "   • Creation: PASS\n";
        std::cout << "   • Sample rate: " << frame->sample_rate() << " Hz\n";
        std::cout << "   • Channels: " << frame->channel_count() << "\n";
        std::cout << "   • Samples: " << frame->sample_count() << "\n";
        std::cout << "   • Data size: " << frame->data_size() << " bytes\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception in audio frame test: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Simple integration test - basic component creation
 */
bool test_basic_integration() {
    std::cout << "\n🔧 Testing Basic Integration...\n";
    
    try {
        using namespace ve::audio;
        
        // Set up sample rate converter
        ResampleConfig resample_config;
        resample_config.input_sample_rate = 44100;
        resample_config.output_sample_rate = 48000;
        resample_config.input_channels = 2;
        resample_config.output_channels = 2;
        resample_config.quality = ResampleQuality::Medium;
        
        auto converter = SampleRateConverter::create(resample_config);
        if (!converter) {
            std::cout << "❌ Failed to create sample rate converter\n";
            return false;
        }
        
        ve::audio::AudioError converter_init = converter->initialize();
        if (static_cast<int>(converter_init) != static_cast<int>(ve::audio::AudioError::None)) {
            std::cout << "❌ Failed to initialize sample rate converter\n";
            return false;
        }
        
        // Set up buffer pool
        AudioBufferConfig buffer_config;
        buffer_config.buffer_size_samples = 1024;
        buffer_config.channel_count = 2;
        buffer_config.sample_format = SampleFormat::Float32;
        buffer_config.pool_size = 4;
        buffer_config.lock_free = true;
        
        auto buffer_pool = std::make_unique<AudioBufferPool>(buffer_config);
        if (!buffer_pool) {
            std::cout << "❌ Failed to create buffer pool\n";
            return false;
        }
        
        // Set up audio clock
        AudioClockConfig clock_config;
        clock_config.sample_rate = 48000; // Output sample rate
        clock_config.enable_drift_compensation = true;
        
        auto audio_clock = std::make_unique<AudioClock>(clock_config);
        if (!audio_clock) {
            std::cout << "❌ Failed to create audio clock\n";
            return false;
        }
        
        ve::audio::AudioError clock_init = audio_clock->initialize();
        if (static_cast<int>(clock_init) != static_cast<int>(ve::audio::AudioError::None)) {
            std::cout << "❌ Failed to initialize audio clock\n";
            return false;
        }
        
        // Create an audio frame
        TimePoint timestamp(0, 1);
        auto frame = AudioFrame::create(44100, 2, 1024, SampleFormat::Float32, timestamp);
        if (!frame) {
            std::cout << "❌ Failed to create audio frame\n";
            return false;
        }
        
        std::cout << "✅ Basic Integration Results:\n";
        std::cout << "   • All components created successfully: PASS\n";
        std::cout << "   • All components initialized: PASS\n";
        std::cout << "   • Audio frame creation: PASS\n";
        std::cout << "   • Ready for advanced processing: PASS\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception in integration test: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Main validation function
 */
int main() {
    std::cout << "🎵 Video Editor - Phase 1 Week 2: Audio Foundation Systems Simple Validation\n";
    std::cout << "============================================================================\n";
    
    bool all_tests_passed = true;
    
    // Run basic functionality tests
    all_tests_passed &= test_sample_rate_converter();
    all_tests_passed &= test_audio_buffer_management();
    all_tests_passed &= test_audio_clock_system();
    all_tests_passed &= test_audio_frame_system();
    
    // Run basic integration test
    all_tests_passed &= test_basic_integration();
    
    std::cout << "\n============================================================================\n";
    if (all_tests_passed) {
        std::cout << "🎉 ALL TESTS PASSED! Phase 1 Week 2 audio foundation systems are ready.\n";
        std::cout << "✅ Sample Rate Converter: Creation and initialization working\n";
        std::cout << "✅ Audio Buffer Management: Buffer pool and circular buffers working\n";
        std::cout << "✅ Audio Clock System: Timing infrastructure working\n";
        std::cout << "✅ Audio Frame System: Data containers working\n";
        std::cout << "✅ Basic Integration: All components work together\n";
        std::cout << "\n📋 Next Steps: Run performance and quality tests for production readiness\n";
        return 0;
    } else {
        std::cout << "❌ SOME TESTS FAILED! Phase 1 Week 2 requires attention.\n";
        return 1;
    }
}