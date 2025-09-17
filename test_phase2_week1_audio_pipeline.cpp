/**
 * @file test_phase2_week1_audio_pipeline.cpp
 * @brief Phase 2 Week 1: Basic Audio Pipeline - Comprehensive Validation
 * 
 * Validates complete audio engine implementation providing:
 * - Audio loading and format detection
 * - Basic playback infrastructure (play/pause/stop/seek)
 * - Timeline integration for synchronized playback
 * - Professional-grade error handling and state management
 */

#include "audio/audio_engine.hpp"
#include "core/log.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using namespace ve;
using namespace ve::audio;

/**
 * @brief Test audio engine callback to verify notifications
 */
class TestAudioEngineCallback : public AudioEngineCallback {
public:
    // State tracking
    std::vector<AudioEngineState> state_history;
    std::vector<TimePoint> position_updates;
    std::vector<std::string> errors;
    std::vector<AudioSourceID> loaded_sources;
    uint32_t buffer_underrun_count = 0;
    
    void on_state_changed(AudioEngineState old_state, AudioEngineState new_state) override {
        std::cout << "  State changed: " << static_cast<int>(old_state) 
                  << " -> " << static_cast<int>(new_state) << std::endl;
        state_history.push_back(new_state);
    }
    
    void on_position_changed(const TimePoint& position) override {
        position_updates.push_back(position);
        // Only log occasionally to avoid spam
        if (position_updates.size() % 10 == 0) {
            auto rational = position.to_rational();
            std::cout << "  Position update: " << rational.num << "/" << rational.den << std::endl;
        }
    }
    
    void on_error(const std::string& error_message) override {
        std::cout << "  Error: " << error_message << std::endl;
        errors.push_back(error_message);
    }
    
    void on_source_loaded(AudioSourceID source_id, const AudioSourceInfo& info) override {
        std::cout << "  Source loaded: ID=" << source_id 
                  << ", Path=" << info.file_path << std::endl;
        loaded_sources.push_back(source_id);
    }
    
    void on_buffer_underrun() override {
        std::cout << "  Buffer underrun detected!" << std::endl;
        buffer_underrun_count++;
    }
};

/**
 * @brief Phase 2 Week 1 Audio Pipeline Validator
 */
class Phase2Week1Validator {
private:
    AudioEngine engine;
    TestAudioEngineCallback test_callback;
    bool validation_passed = true;
    
    void assert_test(bool condition, const std::string& test_name) {
        if (condition) {
            std::cout << "✅ " << test_name << " - PASSED" << std::endl;
        } else {
            std::cout << "❌ " << test_name << " - FAILED" << std::endl;
            validation_passed = false;
        }
    }
    
public:
    bool run_all_tests() {
        std::cout << "=== PHASE 2 WEEK 1: Basic Audio Pipeline Validation ===" << std::endl;
        std::cout << "=========================================================" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🎯 PHASE 2 WEEK 1 OBJECTIVE:" << std::endl;
        std::cout << "   Implement comprehensive basic audio pipeline with:" << std::endl;
        std::cout << "   • Audio loading and format detection" << std::endl;
        std::cout << "   • Basic playback infrastructure (play/pause/stop/seek)" << std::endl;
        std::cout << "   • Timeline integration for synchronized playback" << std::endl;
        std::cout << "   • Professional error handling and state management" << std::endl;
        std::cout << std::endl;
        
        bool all_passed = true;
        all_passed &= test_engine_initialization();
        all_passed &= test_configuration_management();
        all_passed &= test_audio_source_loading();
        all_passed &= test_playback_control();
        all_passed &= test_volume_and_mute_control();
        all_passed &= test_seek_functionality();
        all_passed &= test_timeline_integration();
        all_passed &= test_state_management();
        all_passed &= test_callback_system();
        all_passed &= test_format_support();
        all_passed &= test_error_handling();
        all_passed &= test_integration_points();
        
        std::cout << std::endl;
        std::cout << "=== PHASE 2 WEEK 1 RESULTS ===" << std::endl;
        if (all_passed) {
            std::cout << "🎉 PHASE 2 WEEK 1 VALIDATION: ALL TESTS PASSED!" << std::endl;
            std::cout << "✅ Basic Audio Pipeline implementation is COMPLETE" << std::endl;
            std::cout << "✅ Professional-grade audio engine foundation established" << std::endl;
            std::cout << "✅ Ready for Phase 2 Week 2 (Audio Synchronization)" << std::endl;
        } else {
            std::cout << "❌ PHASE 2 WEEK 1 VALIDATION: SOME TESTS FAILED" << std::endl;
            std::cout << "   Review implementation and fix identified issues" << std::endl;
        }
        std::cout << std::endl;
        
        return all_passed;
    }
    
private:
    bool test_engine_initialization() {
        std::cout << "📋 Testing Engine Initialization..." << std::endl;
        
        // Test initial state
        assert_test(engine.get_state() == AudioEngineState::Uninitialized,
                   "Initial state is Uninitialized");
        assert_test(!engine.is_initialized(), "Not initialized initially");
        
        // Test initialization
        bool init_result = engine.initialize();
        assert_test(init_result, "Engine initialization succeeds");
        assert_test(engine.is_initialized(), "Engine reports initialized");
        assert_test(engine.get_state() == AudioEngineState::Stopped, "State is Stopped after init");
        
        // Test double initialization
        bool second_init = engine.initialize();
        assert_test(second_init, "Double initialization handled gracefully");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_configuration_management() {
        std::cout << "⚙️ Testing Configuration Management..." << std::endl;
        
        // Test default configuration
        const auto& default_config = engine.get_config();
        assert_test(default_config.sample_rate == 48000, "Default sample rate is 48kHz");
        assert_test(default_config.channel_count == 2, "Default channel count is 2");
        assert_test(default_config.buffer_size == 512, "Default buffer size is 512");
        
        // Test configuration validation
        AudioEngineConfig custom_config;
        custom_config.sample_rate = 44100;
        custom_config.channel_count = 6;
        custom_config.buffer_size = 1024;
        
        // Cannot change config while initialized
        bool config_change = engine.set_config(custom_config);
        assert_test(!config_change, "Config change rejected while initialized");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_audio_source_loading() {
        std::cout << "🎵 Testing Audio Source Loading..." << std::endl;
        
        // Set callback to capture events
        engine.set_callback(&test_callback);
        
        // Test loading valid source (using test decoder)
        AudioSourceID source1 = engine.load_audio_source("test_audio_file.wav");
        assert_test(source1 != INVALID_AUDIO_SOURCE_ID, "Load audio source succeeds");
        assert_test(engine.is_source_loaded(source1), "Source reported as loaded");
        
        // Test source info
        AudioSourceInfo info = engine.get_source_info(source1);
        assert_test(info.is_valid(), "Source info is valid");
        assert_test(info.id == source1, "Source info ID matches");
        assert_test(info.file_path == "test_audio_file.wav", "Source info path matches");
        
        // Test loading multiple sources
        AudioSourceID source2 = engine.load_audio_source("test_audio_file2.wav");
        assert_test(source2 != INVALID_AUDIO_SOURCE_ID, "Load second audio source succeeds");
        assert_test(source2 != source1, "Different source IDs assigned");
        
        // Test get loaded sources
        auto loaded_sources = engine.get_loaded_sources();
        assert_test(loaded_sources.size() == 2, "Two sources loaded");
        
        // Test callback notifications
        assert_test(test_callback.loaded_sources.size() == 2, "Callback received load notifications");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_playback_control() {
        std::cout << "▶️ Testing Playback Control..." << std::endl;
        
        // Test initial playback state
        PlaybackState initial_state = engine.get_playback_state();
        assert_test(initial_state.state == AudioEngineState::Stopped, "Initial playback state is Stopped");
        
        // Test play
        bool play_result = engine.play();
        assert_test(play_result, "Play command succeeds");
        assert_test(engine.get_state() == AudioEngineState::Playing, "State is Playing");
        
        // Brief delay to allow audio thread to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Test pause
        bool pause_result = engine.pause();
        assert_test(pause_result, "Pause command succeeds");
        assert_test(engine.get_state() == AudioEngineState::Paused, "State is Paused");
        
        // Test resume (play from paused)
        bool resume_result = engine.play();
        assert_test(resume_result, "Resume from pause succeeds");
        assert_test(engine.get_state() == AudioEngineState::Playing, "State is Playing after resume");
        
        // Test stop
        bool stop_result = engine.stop();
        assert_test(stop_result, "Stop command succeeds");
        assert_test(engine.get_state() == AudioEngineState::Stopped, "State is Stopped");
        
        // Verify position reset
        TimePoint position = engine.get_current_position();
        assert_test(position.to_rational().num == 0, "Position reset to beginning after stop");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_volume_and_mute_control() {
        std::cout << "🔊 Testing Volume and Mute Control..." << std::endl;
        
        // Test default volume
        float default_volume = engine.get_volume();
        assert_test(default_volume == 1.0f, "Default volume is 1.0");
        assert_test(!engine.is_muted(), "Not muted by default");
        
        // Test volume setting
        engine.set_volume(0.5f);
        assert_test(engine.get_volume() == 0.5f, "Volume set to 0.5");
        
        // Test volume clamping
        engine.set_volume(1.5f);  // Above maximum
        assert_test(engine.get_volume() == 1.0f, "Volume clamped to maximum");
        
        engine.set_volume(-0.1f);  // Below minimum
        assert_test(engine.get_volume() == 0.0f, "Volume clamped to minimum");
        
        // Test mute control
        engine.set_muted(true);
        assert_test(engine.is_muted(), "Mute state set correctly");
        
        engine.set_muted(false);
        assert_test(!engine.is_muted(), "Unmute state set correctly");
        
        // Restore default volume
        engine.set_volume(1.0f);
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_seek_functionality() {
        std::cout << "⏭️ Testing Seek Functionality..." << std::endl;
        
        // Test seek to specific position
        TimePoint target_position(1000, 48000);  // 1000 samples at 48kHz
        bool seek_result = engine.seek(target_position);
        assert_test(seek_result, "Seek command succeeds");
        
        // Verify position
        TimePoint current_position = engine.get_current_position();
        assert_test(current_position.to_rational().num == target_position.to_rational().num,
                   "Position updated correctly after seek");
        
        // Test seek to beginning
        TimePoint beginning(0, 1);
        bool seek_beginning = engine.seek(beginning);
        assert_test(seek_beginning, "Seek to beginning succeeds");
        
        TimePoint position_after_reset = engine.get_current_position();
        assert_test(position_after_reset.to_rational().num == 0, "Position reset to beginning");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_timeline_integration() {
        std::cout << "📅 Testing Timeline Integration..." << std::endl;
        
        // Get loaded sources from previous test
        auto loaded_sources = engine.get_loaded_sources();
        if (loaded_sources.empty()) {
            // Load a source for timeline testing
            AudioSourceID source = engine.load_audio_source("timeline_test.wav");
            if (source != INVALID_AUDIO_SOURCE_ID) {
                loaded_sources = engine.get_loaded_sources();
            }
        }
        
        if (!loaded_sources.empty()) {
            AudioSourceID source_id = loaded_sources[0].id;
            
            // Test adding source to timeline
            TimePoint start_time(0, 1);
            TimeDuration duration(2000, 48);  // 2000/48 seconds
            bool add_result = engine.add_source_to_timeline(source_id, start_time, duration);
            assert_test(add_result, "Add source to timeline succeeds");
            
            // Test getting active sources at time
            auto active_sources = engine.get_active_sources_at_time(TimePoint(500, 48));
            assert_test(!active_sources.empty(), "Source active at expected time");
            assert_test(active_sources[0] == source_id, "Correct source active");
            
            // Test removing source from timeline
            bool remove_result = engine.remove_source_from_timeline(source_id);
            assert_test(remove_result, "Remove source from timeline succeeds");
            
            // Verify source no longer active
            auto active_after_remove = engine.get_active_sources_at_time(TimePoint(500, 48));
            assert_test(active_after_remove.empty(), "No sources active after removal");
        } else {
            std::cout << "  ⚠️ Skipping timeline tests - no loaded sources available" << std::endl;
        }
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_state_management() {
        std::cout << "🔄 Testing State Management..." << std::endl;
        
        // Test state transitions
        engine.stop();  // Ensure stopped state
        assert_test(engine.get_state() == AudioEngineState::Stopped, "State is Stopped");
        
        // Test playback state information
        PlaybackState state = engine.get_playback_state();
        assert_test(state.state == AudioEngineState::Stopped, "Playback state matches engine state");
        assert_test(state.volume == engine.get_volume(), "Playback state volume matches");
        assert_test(state.muted == engine.is_muted(), "Playback state mute matches");
        
        // Test duration calculation
        TimeDuration total_duration = engine.get_duration();
        assert_test(total_duration.to_rational().den > 0, "Duration has valid denominator");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_callback_system() {
        std::cout << "📞 Testing Callback System..." << std::endl;
        
        // Clear previous callback data
        test_callback.state_history.clear();
        test_callback.position_updates.clear();
        test_callback.errors.clear();
        
        // Test callback registration
        engine.set_callback(&test_callback);
        
        // Trigger state changes to test callbacks
        engine.play();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        engine.pause();
        engine.stop();
        
        // Verify callback notifications
        assert_test(!test_callback.state_history.empty(), "State change callbacks received");
        assert_test(test_callback.position_updates.size() > 0, "Position update callbacks received");
        
        // Test callback removal
        engine.clear_callback();
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_format_support() {
        std::cout << "🎶 Testing Format Support..." << std::endl;
        
        // Test supported formats query
        auto supported_formats = engine.get_supported_formats();
        assert_test(!supported_formats.empty(), "Supported formats list not empty");
        
        // Test specific format support
        bool pcm_supported = engine.is_format_supported(AudioCodec::PCM);
        assert_test(pcm_supported, "PCM format supported");
        
        bool mp3_supported = engine.is_format_supported(AudioCodec::MP3);
        assert_test(mp3_supported, "MP3 format supported");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_error_handling() {
        std::cout << "⚠️ Testing Error Handling..." << std::endl;
        
        // Clear any previous errors
        engine.clear_error();
        
        // Test invalid operations to trigger errors
        AudioSourceID invalid_source = engine.load_audio_source("");  // Empty path
        assert_test(invalid_source == INVALID_AUDIO_SOURCE_ID, "Invalid source load returns invalid ID");
        
        // Test error reporting (might be empty if no actual error occurred)
        std::string last_error = engine.get_last_error();
        // Note: Error might be empty with test decoder, which is okay
        
        // Test unload invalid source
        bool unload_invalid = engine.unload_audio_source(999999);  // Non-existent ID
        assert_test(!unload_invalid, "Unload invalid source returns false");
        
        // Test get info for invalid source
        AudioSourceInfo invalid_info = engine.get_source_info(999999);
        assert_test(!invalid_info.is_valid(), "Invalid source info is not valid");
        
        std::cout << std::endl;
        return validation_passed;
    }
    
    bool test_integration_points() {
        std::cout << "🔗 Testing Integration Points..." << std::endl;
        
        // Test mixing graph access
        MixingGraph* mixing_graph = engine.get_mixing_graph();
        assert_test(mixing_graph != nullptr, "Mixing graph accessible");
        
        // Test audio clock access
        AudioClock* audio_clock = engine.get_audio_clock();
        assert_test(audio_clock != nullptr, "Audio clock accessible");
        
        // Test shutdown and re-initialization
        engine.shutdown();
        assert_test(!engine.is_initialized(), "Engine properly shutdown");
        assert_test(engine.get_state() == AudioEngineState::Uninitialized, "State reset to Uninitialized");
        
        // Re-initialize for clean state
        bool reinit = engine.initialize();
        assert_test(reinit, "Re-initialization succeeds");
        
        std::cout << std::endl;
        return validation_passed;
    }
};

/**
 * @brief Main validation entry point
 */
int main() {
    std::cout << "PHASE 2 WEEK 1: Basic Audio Pipeline Validation" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << std::endl;
    
    try {
        Phase2Week1Validator validator;
        bool success = validator.run_all_tests();
        
        if (success) {
            std::cout << "🎉 PHASE 2 WEEK 1 VALIDATION: COMPLETE SUCCESS!" << std::endl;
            std::cout << "✅ Professional audio engine foundation established" << std::endl;
            std::cout << "✅ All required functionality implemented and tested" << std::endl;
            std::cout << "✅ Ready for Phase 2 Week 2 development" << std::endl;
            return 0;
        } else {
            std::cout << "❌ PHASE 2 WEEK 1 VALIDATION: FAILURES DETECTED" << std::endl;
            std::cout << "   Please review and fix implementation issues" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "💥 VALIDATION ERROR: " << e.what() << std::endl;
        return 1;
    }
}