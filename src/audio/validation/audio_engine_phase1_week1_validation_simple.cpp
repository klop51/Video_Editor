/**
 * @file audio_engine_phase1_week1_validation_simple.cpp
 * @brief Simplified Audio Engine Phase 1 Week 1 validation test
 * 
 * Tests only the implemented core audio infrastructure:
 * - AudioFrame creation and manipulation
 * - TestAudioDecoder basic functionality
 * - Sample format handling
 * 
 * Success criteria:
 * ✅ AudioFrame operations work correctly
 * ✅ TestAudioDecoder can generate test audio
 * ✅ Core infrastructure is stable
 */

#include <audio/audio_frame.hpp>
#include <audio/test_decoder.hpp>
#include <core/log.hpp>
#include <core/time.hpp>

#include <iostream>
#include <memory>
#include <sstream>

using namespace ve;
using namespace ve::audio;

bool test_audio_frame_creation() {
    ve::log::info("Testing AudioFrame creation...");
    
    try {
        // Test basic AudioFrame creation
        TimePoint timestamp(TimeRational(1, 1)); // 1 second timestamp
        auto frame = AudioFrame::create(
            48000,                      // sample rate
            2,                          // channels (stereo)
            1024,                       // samples
            SampleFormat::Float32,      // format
            timestamp                   // timestamp
        );
        
        if (!frame) {
            ve::log::error("Failed to create AudioFrame");
            return false;
        }
        
        // Validate frame properties
        if (frame->sample_count() != 1024) {
            std::ostringstream oss;
            oss << "Wrong sample count: " << frame->sample_count();
            ve::log::error(oss.str());
            return false;
        }
        
        if (frame->channel_count() != 2) {
            std::ostringstream oss;
            oss << "Wrong channel count: " << frame->channel_count();
            ve::log::error(oss.str());
            return false;
        }
        
        if (frame->sample_rate() != 48000) {
            std::ostringstream oss;
            oss << "Wrong sample rate: " << frame->sample_rate();
            ve::log::error(oss.str());
            return false;
        }
        
        if (frame->format() != SampleFormat::Float32) {
            ve::log::error("Wrong sample format");
            return false;
        }
        
        // Test data access
        void* data = frame->data();
        if (!data) {
            ve::log::error("AudioFrame data pointer is null");
            return false;
        }
        
        size_t data_size = frame->data_size();
        size_t expected_size = 1024 * 2 * sizeof(float); // samples * channels * float32
        if (data_size != expected_size) {
            std::ostringstream oss;
            oss << "Wrong data size: " << data_size << " (expected " << expected_size << ")";
            ve::log::error(oss.str());
            return false;
        }
        
        ve::log::info("✅ AudioFrame creation tests passed");
        return true;
        
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Exception in AudioFrame test: " << e.what();
        ve::log::error(oss.str());
        return false;
    }
}

bool test_audio_decoder_basic() {
    ve::log::info("Testing TestAudioDecoder...");
    
    try {
        // Create test decoder
        auto decoder = std::make_unique<TestAudioDecoder>();
        
        // Test initialization
        std::vector<uint8_t> dummy_data{0x00, 0x01, 0x02, 0x03};
        auto init_result = decoder->initialize(dummy_data);
        if (init_result != AudioError::None) {
            ve::log::error("TestAudioDecoder initialization failed");
            return false;
        }
        
        // Test stream info
        auto stream_info = decoder->get_stream_info();
        if (stream_info.sample_rate != 48000) {
            std::ostringstream oss;
            oss << "Wrong sample rate: " << stream_info.sample_rate;
            ve::log::error(oss.str());
            return false;
        }
        
        if (stream_info.channel_count != 2) {
            std::ostringstream oss;
            oss << "Wrong channel count: " << stream_info.channel_count;
            ve::log::error(oss.str());
            return false;
        }
        
        // Test frame decoding
        TimePoint timestamp(TimeRational(1, 1)); // 1 second
        auto frame = decoder->decode_frame(dummy_data, timestamp, SampleFormat::Float32);
        if (!frame) {
            ve::log::error("Failed to decode test frame");
            return false;
        }
        
        // Validate decoded frame
        if (frame->sample_rate() != 48000 || frame->channel_count() != 2) {
            ve::log::error("Decoded frame has wrong properties");
            return false;
        }
        
        ve::log::info("✅ TestAudioDecoder tests passed");
        return true;
        
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Exception in decoder test: " << e.what();
        ve::log::error(oss.str());
        return false;
    }
}

bool test_time_integration() {
    ve::log::info("Testing time system integration...");
    
    try {
        // Test TimePoint creation and usage
        TimePoint start_time(TimeRational(0, 1));
        TimePoint one_second(TimeRational(1, 1));
        TimePoint half_second(TimeRational(1, 2));
        
        // Create frames with different timestamps
        auto frame1 = AudioFrame::create(48000, 2, 1024, SampleFormat::Float32, start_time);
        auto frame2 = AudioFrame::create(48000, 2, 1024, SampleFormat::Float32, one_second);
        auto frame3 = AudioFrame::create(48000, 2, 1024, SampleFormat::Float32, half_second);
        
        if (!frame1 || !frame2 || !frame3) {
            ve::log::error("Failed to create frames with timestamps");
            return false;
        }
        
        // Validate timestamps
        if (frame1->timestamp().to_rational().num != 0 || frame1->timestamp().to_rational().den != 1) {
            ve::log::error("Wrong timestamp for frame1");
            return false;
        }
        
        if (frame2->timestamp().to_rational().num != 1 || frame2->timestamp().to_rational().den != 1) {
            ve::log::error("Wrong timestamp for frame2");
            return false;
        }
        
        ve::log::info("✅ Time integration tests passed");
        return true;
        
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Exception in time integration test: " << e.what();
        ve::log::error(oss.str());
        return false;
    }
}

int main() {
    std::cout << "=== Audio Engine Phase 1 Week 1 Validation Test ===" << std::endl;
    std::cout << "Testing core audio infrastructure implementation" << std::endl;
    std::cout << std::endl;
    
    bool all_passed = true;
    
    try {
        // Run validation tests
        all_passed &= test_audio_frame_creation();
        all_passed &= test_audio_decoder_basic();
        all_passed &= test_time_integration();
        
        std::cout << std::endl;
        if (all_passed) {
            std::cout << "🎉 All Audio Engine Phase 1 Week 1 tests PASSED!" << std::endl;
            std::cout << "Core audio infrastructure is ready for Phase 1 Week 2" << std::endl;
            ve::log::info("Audio Engine Phase 1 Week 1 validation completed successfully");
        } else {
            std::cout << "❌ Some tests FAILED. Check the logs above." << std::endl;
            ve::log::error("Audio Engine Phase 1 Week 1 validation failed");
        }
        
    } catch (const std::exception& e) {
        std::cout << "💥 Fatal error during validation: " << e.what() << std::endl;
        std::ostringstream oss;
        oss << "Fatal error in validation: " << e.what();
        ve::log::critical(oss.str());
        all_passed = false;
    }
    
    return all_passed ? 0 : 1;
}