/*
 * Audio Callback Test - Direct AudioOutput with real audio data
 * 
 * This test demonstrates connecting an external audio callback to the 
 * event-driven AudioOutput to provide real audio data instead of silence.
 */

#include <iostream>
#include <cmath>
#include <cstdint>
#include <thread>
#include <atomic>

#include "audio/audio_output.hpp"
#include "spdlog/spdlog.h"

// Define M_PI for Windows
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Simple tone generator for testing
class ToneGenerator {
private:
    double phase_ = 0.0;
    double frequency_ = 440.0; // A4 note
    double amplitude_ = 0.1;   // Low volume to protect ears
    uint32_t sample_rate_ = 48000;

public:
    void set_frequency(double freq) { frequency_ = freq; }
    void set_amplitude(double amp) { amplitude_ = amp; }

    // Generate samples for stereo output
    void generate_samples(float* buffer, uint32_t frame_count) {
        const double phase_increment = 2.0 * M_PI * frequency_ / sample_rate_;
        
        for (uint32_t i = 0; i < frame_count; ++i) {
            float sample = static_cast<float>(amplitude_ * std::sin(phase_));
            
            // Stereo: left and right channels
            buffer[i * 2]     = sample; // Left
            buffer[i * 2 + 1] = sample; // Right
            
            phase_ += phase_increment;
            if (phase_ >= 2.0 * M_PI) {
                phase_ -= 2.0 * M_PI;
            }
        }
    }
};

// Global tone generator for callback
static ToneGenerator tone_gen;

// Audio render callback function
size_t audio_render_callback(void* buffer, uint32_t frame_count, 
                           ve::audio::SampleFormat format, uint16_t channels) {
    spdlog::info("CALLBACK: Rendering {} frames, format={}, channels={}", 
                 frame_count, static_cast<int>(format), channels);
    
    if (format != ve::audio::SampleFormat::Float32 || channels != 2) {
        spdlog::error("CALLBACK: Unsupported format or channel count");
        return 0;
    }
    
    // Generate audio data
    float* float_buffer = static_cast<float*>(buffer);
    tone_gen.generate_samples(float_buffer, frame_count);
    
    size_t bytes_written = frame_count * channels * sizeof(float);
    spdlog::info("CALLBACK: Generated {} bytes of audio data", bytes_written);
    
    return bytes_written;
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== Audio Callback Test Starting ===");
    
    try {
        // Create audio output with configuration
        ve::audio::AudioOutputConfig config;
        config.sample_rate = 48000;
        config.channel_count = 2;
        config.format = ve::audio::SampleFormat::Float32;
        config.exclusive_mode = false;
        
        auto audio_output = ve::audio::AudioOutput::create(config);
        if (!audio_output) {
            spdlog::error("Failed to create audio output");
            return 1;
        }
        
        // Set up our audio callback
        spdlog::info("Setting up audio render callback...");
        audio_output->set_render_callback(audio_render_callback);
        
        // Initialize with standard settings
        auto init_result = audio_output->initialize();
        if (init_result != ve::audio::AudioOutputError::Success) {
            spdlog::error("Failed to initialize audio output: {}", static_cast<int>(init_result));
            return 1;
        }
        
        spdlog::info("Audio output initialized successfully");
        
        // Start the audio output
        auto start_result = audio_output->start();
        if (start_result != ve::audio::AudioOutputError::Success) {
            spdlog::error("Failed to start audio output: {}", static_cast<int>(start_result));
            return 1;
        }
        
        spdlog::info("Audio output started - you should hear a 440Hz tone!");
        spdlog::info("Running for 3 seconds...");
        
        // Let it run for 3 seconds
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Change frequency
        spdlog::info("Changing to 880Hz (A5)...");
        tone_gen.set_frequency(880.0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Change frequency again
        spdlog::info("Changing to 220Hz (A3)...");
        tone_gen.set_frequency(220.0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        spdlog::info("Stopping audio output...");
        audio_output->stop();
        
    } catch (const std::exception& e) {
        spdlog::error("Exception: {}", e.what());
        return 1;
    }
    
    spdlog::info("=== Audio Callback Test Complete ===");
    spdlog::info("If you heard three different tones (440Hz, 880Hz, 220Hz), the callback system is working!");
    
    return 0;
}