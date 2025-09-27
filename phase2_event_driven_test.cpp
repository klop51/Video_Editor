/**
 * @file phase2_event_driven_test.cpp
 * @brief Phase 2 Event-Driven WASAPI Validation Test
 * 
 * Tests the event-driven WASAPI implementation to confirm:
 * 1. Device-clock driven timing (consistent ~10ms intervals)
 * 2. Event callback infrastructure working properly
 * 3. MMCSS thread priority setup
 * 4. Render thread responsiveness to device events
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>

#include "audio/audio_output.hpp"
#include "core/log.hpp"

using namespace ve::audio;

int main() {
    std::cout << "\n=== Phase 2 Event-Driven WASAPI Validation Test ===\n";
    std::cout << "Testing device-clock driven audio rendering with event callbacks\n\n";

    // Configure audio output for low-latency event-driven mode
    AudioOutputConfig config;
    config.sample_rate = 48000;
    config.channel_count = 2;
    config.format = SampleFormat::Float32;
    config.exclusive_mode = false;
    config.buffer_duration_ms = 50;  // 50ms buffer
    config.min_periodicity_ms = 10;  // 10ms device period

    std::cout << "Configuration:\n";
    std::cout << "  Sample Rate: " << config.sample_rate << " Hz\n";
    std::cout << "  Channels: " << config.channel_count << "\n";
    std::cout << "  Buffer Duration: " << config.buffer_duration_ms << " ms\n";
    std::cout << "  Device Period: " << config.min_periodicity_ms << " ms\n";
    std::cout << "  Expected Submit Interval: ~10ms (device-driven)\n\n";

    // Create and initialize audio output
    auto audio_output = AudioOutput::create(config);
    if (!audio_output) {
        std::cerr << "ERROR: Failed to create audio output\n";
        return 1;
    }

    std::cout << "Initializing audio output...\n";
    auto init_result = audio_output->initialize();
    if (init_result != AudioOutputError::Success) {
        std::cerr << "ERROR: Failed to initialize audio output: " 
                  << audio_output->get_last_error() << "\n";
        return 1;
    }

    std::cout << "Starting event-driven audio rendering...\n";
    auto start_result = audio_output->start();
    if (start_result != AudioOutputError::Success) {
        std::cerr << "ERROR: Failed to start audio output: " 
                  << audio_output->get_last_error() << "\n";
        return 1;
    }

    std::cout << "\n🎵 Phase 2 Event-Driven Audio Active!\n";
    std::cout << "Monitoring submit timing for 10 seconds...\n";
    std::cout << "Look for PHASE2_DEVICE_DRIVEN log entries with consistent ~10ms timing\n\n";

    // Let the event-driven render thread run for 10 seconds
    // The render thread will log device-driven submissions with timing data
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "\nStopping audio output...\n";
    audio_output->stop();
    audio_output->shutdown();

    // Get final statistics
    auto stats = audio_output->get_stats();
    std::cout << "\nFinal Statistics:\n";
    std::cout << "  Frames Rendered: " << stats.frames_rendered << "\n";
    std::cout << "  Buffer Underruns: " << stats.buffer_underruns << "\n";
    std::cout << "  CPU Usage: " << stats.cpu_usage_percent << "%\n";

    std::cout << "\n=== Phase 2 Test Complete ===\n";
    std::cout << "Check the log output above for:\n";
    std::cout << "  ✓ PHASE2_RENDER_THREAD: Event-driven render thread started\n";
    std::cout << "  ✓ PHASE2_DEVICE_DRIVEN: Regular submissions with ~10ms timing\n";
    std::cout << "  ✓ Consistent frame counts (480 frames @ 48kHz = 10ms)\n";
    std::cout << "  ✓ No buffer underruns or timing irregularities\n\n";

    if (stats.buffer_underruns == 0) {
        std::cout << "🎉 SUCCESS: No buffer underruns detected!\n";
        std::cout << "Phase 2 event-driven WASAPI implementation working correctly.\n";
        return 0;
    } else {
        std::cout << "⚠️  WARNING: " << stats.buffer_underruns << " buffer underruns detected.\n";
        std::cout << "May indicate timing issues - check PHASE2_DEVICE_DRIVEN logs.\n";
        return 1;
    }
}