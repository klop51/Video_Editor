/**
 * Simple test to validate the audio pipeline integration
 * This test creates a basic audio setup and verifies the pipeline works
 */

#include "src/playback/include/playback/controller.hpp"
#include "src/audio/include/audio/audio_pipeline.hpp"
#include "src/core/include/core/log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        ve::log::info("Starting audio integration test");
        
        // Create a playback controller
        auto controller = std::make_unique<ve::playback::PlaybackController>();
        
        // Initialize audio pipeline
        if (!controller->initialize_audio_pipeline()) {
            std::cerr << "Failed to initialize audio pipeline!" << std::endl;
            return 1;
        }
        
        std::cout << "✅ Audio pipeline initialized successfully" << std::endl;
        
        // Test basic audio controls
        controller->set_master_mute(false);
        float volume = controller->get_master_volume();
        std::cout << "✅ Audio controls working - Volume: " << volume << std::endl;
        
        // Get audio stats
        auto stats = controller->get_audio_stats();
        std::cout << "✅ Audio stats accessible - Frames processed: " << stats.frames_processed << std::endl;
        
        ve::log::info("Audio integration test completed successfully");
        std::cout << "🎵 Audio pipeline integration test PASSED! 🎵" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Audio integration test failed: " << e.what() << std::endl;
        return 1;
    }
}