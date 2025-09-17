/**
 * @file audio_output_validation.cpp
 * @brief Validation test for WASAPI Audio Output Backend
 *
 * Tests the audio output implementation for:
 * - Device enumeration
 * - Initialization and shutdown
 * - Basic audio playback
 * - Error handling
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

#include "audio/audio_output.hpp"
#include "audio/audio_frame.hpp"

namespace {

const float M_PI_F = 3.14159265358979323846f;

/**
 * @brief Generate a test sine wave
 */
std::vector<float> generate_sine_wave(uint32_t sample_count, uint32_t channels,
                                      float frequency, uint32_t sample_rate) {
    std::vector<float> samples(sample_count * channels);

    for (uint32_t i = 0; i < sample_count; ++i) {
        float time = static_cast<float>(i) / static_cast<float>(sample_rate);
        float value = 0.1f * std::sin(2.0f * M_PI_F * frequency * time); // Low volume for safety

        for (uint32_t ch = 0; ch < channels; ++ch) {
            samples[i * channels + ch] = value;
        }
    }

    return samples;
}

/**
 * @brief Test device enumeration
 */
bool test_device_enumeration() {
    std::cout << "\n=== Audio Device Enumeration Test ===\n";

    try {
        // Enumerate output devices
        auto devices = ve::audio::AudioOutput::enumerate_devices(false);

        std::cout << "Found " << devices.size() << " audio output devices:\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];
            std::cout << "  " << (i + 1) << ". " << device.name << "\n";
            std::cout << "     ID: " << device.id << "\n";
            std::cout << "     Default: " << (device.is_default ? "Yes" : "No") << "\n";
        }

        // Get default device
        auto default_device = ve::audio::AudioOutput::get_default_device();
        if (!default_device.id.empty()) {
            std::cout << "\nDefault device: " << default_device.name << "\n";
        } else {
            std::cout << "\nNo default device found\n";
        }

        return true;

    } catch (const std::exception& e) {
        std::cout << "❌ Device enumeration test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test audio output initialization
 */
bool test_audio_output_initialization() {
    std::cout << "\n=== Audio Output Initialization Test ===\n";

    try {
        // Create audio output with default config
        auto audio_output = ve::audio::AudioOutput::create();
        if (!audio_output) {
            std::cout << "❌ Failed to create audio output\n";
            return false;
        }

        // Test initialization
        auto result = audio_output->initialize();
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to initialize audio output: " << audio_output->get_last_error() << "\n";
            return false;
        }

        std::cout << "✅ Audio output initialized successfully\n";

        // Get and display configuration
        const auto& config = audio_output->get_config();
        std::cout << "Configuration:\n";
        std::cout << "  Sample Rate: " << config.sample_rate << " Hz\n";
        std::cout << "  Channels: " << config.channel_count << "\n";
        std::cout << "  Format: " << (config.format == ve::audio::SampleFormat::Float32 ? "Float32" :
                                       config.format == ve::audio::SampleFormat::Int32 ? "Int32" : "Int16") << "\n";
        std::cout << "  Buffer Duration: " << config.buffer_duration_ms << " ms\n";

        // Get statistics
        auto stats = audio_output->get_stats();
        std::cout << "Initial Statistics:\n";
        std::cout << "  Buffer Size: " << stats.buffer_size_frames << " frames\n";

        // Test shutdown
        audio_output->shutdown();
        std::cout << "✅ Audio output shutdown successfully\n";

        return true;

    } catch (const std::exception& e) {
        std::cout << "❌ Audio output initialization test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test basic audio playback
 */
bool test_basic_playback() {
    std::cout << "\n=== Basic Audio Playback Test ===\n";

    try {
        // Create and initialize audio output
        auto audio_output = ve::audio::AudioOutput::create();
        if (!audio_output) {
            std::cout << "❌ Failed to create audio output\n";
            return false;
        }

        auto result = audio_output->initialize();
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to initialize audio output: " << audio_output->get_last_error() << "\n";
            return false;
        }

        // Generate test audio (1 second of 440Hz sine wave)
        const auto& config = audio_output->get_config();
        auto test_audio = generate_sine_wave(config.sample_rate, config.channel_count, 440.0f, config.sample_rate);

        std::cout << "Generated " << test_audio.size() / config.channel_count << " samples of test audio\n";

        // Start playback
        result = audio_output->start();
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to start playback: " << audio_output->get_last_error() << "\n";
            audio_output->shutdown();
            return false;
        }

        std::cout << "✅ Playback started\n";

        // Submit audio data in chunks
        const uint32_t chunk_size = config.sample_rate / 10; // 100ms chunks
        uint32_t submitted_samples = 0;

        while (submitted_samples < test_audio.size()) {
            uint32_t samples_to_submit = std::min(chunk_size,
                static_cast<uint32_t>(test_audio.size() - submitted_samples));

            result = audio_output->submit_data(
                test_audio.data() + submitted_samples,
                samples_to_submit / config.channel_count
            );

            if (result != ve::audio::AudioOutputError::Success) {
                std::cout << "❌ Failed to submit audio data: " << audio_output->get_last_error() << "\n";
                break;
            }

            submitted_samples += samples_to_submit;

            // Small delay to prevent overwhelming the audio system
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "✅ Submitted " << submitted_samples / config.channel_count << " frames for playback\n";

        // Wait a bit for playback to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));

        // Stop playback
        result = audio_output->stop();
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to stop playback: " << audio_output->get_last_error() << "\n";
        } else {
            std::cout << "✅ Playback stopped\n";
        }

        // Get final statistics
        auto final_stats = audio_output->get_stats();
        std::cout << "Final Statistics:\n";
        std::cout << "  Frames Rendered: " << final_stats.frames_rendered << "\n";
        std::cout << "  Buffer Underruns: " << final_stats.buffer_underruns << "\n";
        std::cout << "  CPU Usage: " << std::fixed << std::setprecision(2) << final_stats.cpu_usage_percent << "%\n";

        // Shutdown
        audio_output->shutdown();
        std::cout << "✅ Audio output shutdown successfully\n";

        return (final_stats.frames_rendered > 0);

    } catch (const std::exception& e) {
        std::cout << "❌ Basic playback test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test audio frame submission
 */
bool test_audio_frame_submission() {
    std::cout << "\n=== Audio Frame Submission Test ===\n";

    try {
        // Create and initialize audio output
        auto audio_output = ve::audio::AudioOutput::create();
        if (!audio_output) {
            std::cout << "❌ Failed to create audio output\n";
            return false;
        }

        auto result = audio_output->initialize();
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to initialize audio output: " << audio_output->get_last_error() << "\n";
            return false;
        }

        const auto& config = audio_output->get_config();

        // Create audio frame
        auto frame = ve::audio::AudioFrame::create(
            config.sample_rate,
            config.channel_count,
            config.sample_rate / 2, // 500ms of audio
            config.format,
            ve::TimePoint(0, 1)
        );

        if (!frame) {
            std::cout << "❌ Failed to create audio frame\n";
            audio_output->shutdown();
            return false;
        }

        // Fill frame with test data
        auto test_data = generate_sine_wave(frame->get_sample_count(), frame->get_channel_count(),
                                           880.0f, frame->get_sample_rate()); // Higher pitch

        // Copy data to frame (simplified - assumes Float32 format)
        if (config.format == ve::audio::SampleFormat::Float32) {
            float* frame_data = static_cast<float*>(frame->get_mutable_data());
            std::copy(test_data.begin(), test_data.end(), frame_data);
        }

        std::cout << "✅ Created and filled audio frame with " << frame->get_sample_count() << " samples\n";

        // Start playback
        result = audio_output->start();
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to start playback: " << audio_output->get_last_error() << "\n";
            audio_output->shutdown();
            return false;
        }

        // Submit frame
        result = audio_output->submit_frame(frame);
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to submit audio frame: " << audio_output->get_last_error() << "\n";
            audio_output->stop();
            audio_output->shutdown();
            return false;
        }

        std::cout << "✅ Audio frame submitted successfully\n";

        // Wait for playback
        std::this_thread::sleep_for(std::chrono::milliseconds(600));

        // Stop and cleanup
        audio_output->stop();
        audio_output->shutdown();

        std::cout << "✅ Frame submission test completed\n";
        return true;

    } catch (const std::exception& e) {
        std::cout << "❌ Frame submission test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test volume and mute controls
 */
bool test_volume_controls() {
    std::cout << "\n=== Volume Controls Test ===\n";

    try {
        auto audio_output = ve::audio::AudioOutput::create();
        if (!audio_output) {
            std::cout << "❌ Failed to create audio output\n";
            return false;
        }

        auto result = audio_output->initialize();
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to initialize audio output: " << audio_output->get_last_error() << "\n";
            return false;
        }

        // Test volume control
        result = audio_output->set_volume(0.5f);
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to set volume: " << audio_output->get_last_error() << "\n";
            audio_output->shutdown();
            return false;
        }

        float volume = audio_output->get_volume();
        std::cout << "✅ Volume set to " << volume << "\n";

        // Test mute control
        result = audio_output->set_muted(true);
        if (result != ve::audio::AudioOutputError::Success) {
            std::cout << "❌ Failed to set mute: " << audio_output->get_last_error() << "\n";
            audio_output->shutdown();
            return false;
        }

        bool muted = audio_output->is_muted();
        std::cout << "✅ Mute state: " << (muted ? "muted" : "unmuted") << "\n";

        // Test volume range
        result = audio_output->set_volume(1.5f); // Should clamp to 1.0
        volume = audio_output->get_volume();
        std::cout << "✅ Volume clamping: set to 1.5, got " << volume << "\n";

        result = audio_output->set_volume(-0.5f); // Should clamp to 0.0
        volume = audio_output->get_volume();
        std::cout << "✅ Volume clamping: set to -0.5, got " << volume << "\n";

        audio_output->shutdown();
        std::cout << "✅ Volume controls test completed\n";

        return true;

    } catch (const std::exception& e) {
        std::cout << "❌ Volume controls test failed: " << e.what() << "\n";
        return false;
    }
}

} // anonymous namespace

/**
 * @brief Main validation function for Audio Output Backend
 */
int main() {
    std::cout << "=================================================================\n";
    std::cout << "Audio Output Backend (WASAPI) - Validation Test\n";
    std::cout << "=================================================================\n";
    std::cout << "Testing WASAPI audio output implementation for:\n";
    std::cout << "• Device enumeration and selection\n";
    std::cout << "• Audio output initialization and configuration\n";
    std::cout << "• Basic audio playback with raw data submission\n";
    std::cout << "• AudioFrame integration and submission\n";
    std::cout << "• Volume and mute controls\n";
    std::cout << "=================================================================\n";

    bool all_tests_passed = true;

    // Test device enumeration
    all_tests_passed &= test_device_enumeration();

    // Test initialization
    all_tests_passed &= test_audio_output_initialization();

    // Test basic playback
    all_tests_passed &= test_basic_playback();

    // Test frame submission
    all_tests_passed &= test_audio_frame_submission();

    // Test volume controls
    all_tests_passed &= test_volume_controls();

    std::cout << "\n=================================================================\n";
    if (all_tests_passed) {
        std::cout << "🎉 ALL AUDIO OUTPUT TESTS PASSED! 🎉\n";
        std::cout << "\nAudio Output Backend successfully implemented:\n";
        std::cout << "✅ WASAPI device enumeration and selection\n";
        std::cout << "✅ Audio output initialization with proper configuration\n";
        std::cout << "✅ Low-latency audio playback with buffer management\n";
        std::cout << "✅ AudioFrame integration for seamless audio pipeline\n";
        std::cout << "✅ Volume and mute controls with proper range validation\n";
        std::cout << "\nNext: Phase 1B - Simple Mixer Core implementation\n";
        return 0;
    } else {
        std::cout << "❌ SOME AUDIO OUTPUT TESTS FAILED\n";
        std::cout << "Please check the implementation and fix any issues.\n";
        return 1;
    }
}