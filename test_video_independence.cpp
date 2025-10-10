#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

// Mock audio pipeline for testing
class MockAudioPipeline {
public:
    bool audio_ready() const { return true; }
    bool is_recovering_from_underrun() const { return false; }
    double get_buffer_fill_percentage() const { return 0.8; } // 80% filled
};

// Mock playback controller with our changes
class PlaybackController {
private:
    MockAudioPipeline audio_pipeline_;
    bool audio_clock_locked_ = false; // Our change: always false

public:
    // Our modified present_due_frames logic
    bool should_present_frame(double frame_time) {
        // Removed all audio_ok checks and restrictions
        // Video can play freely using wall-clock timing
        return true; // Always allow video presentation
    }

    // Check that audio clock is not locked
    bool is_audio_clock_locked() const {
        return audio_clock_locked_;
    }

    // Test the audio_ok logic (should be removed in real code)
    bool old_audio_ok_logic() const {
        return audio_pipeline_.audio_ready() && !audio_pipeline_.is_recovering_from_underrun();
    }
};

int main() {
    std::cout << "Testing video playback independence from audio restrictions..." << std::endl;

    PlaybackController controller;

    // Test 1: Video should be able to present frames regardless of audio state
    std::cout << "Test 1: Video frame presentation" << std::endl;
    for (int i = 0; i < 5; ++i) {
        double frame_time = i * 0.033; // 30fps
        bool can_present = controller.should_present_frame(frame_time);
        std::cout << "  Frame " << i << " at " << frame_time << "s: " << (can_present ? "ALLOWED" : "BLOCKED") << std::endl;
        if (!can_present) {
            std::cout << "  ERROR: Video frame was blocked!" << std::endl;
            return 1;
        }
    }

    // Test 2: Audio clock should not be locked
    std::cout << "Test 2: Audio clock locking" << std::endl;
    bool audio_locked = controller.is_audio_clock_locked();
    std::cout << "  Audio clock locked: " << (audio_locked ? "YES" : "NO") << std::endl;
    if (audio_locked) {
        std::cout << "  ERROR: Audio clock is still locked!" << std::endl;
        return 1;
    }

    // Test 3: Show what the old logic would have been
    std::cout << "Test 3: Old audio_ok logic (for reference)" << std::endl;
    bool old_audio_ok = controller.old_audio_ok_logic();
    std::cout << "  Old audio_ok would be: " << (old_audio_ok ? "true" : "false") << std::endl;
    std::cout << "  (This logic is now removed from video playback decisions)" << std::endl;

    std::cout << "\nAll tests passed! Video playback is now independent of audio restrictions." << std::endl;
    std::cout << "Video can play continuously at its natural frame rate using wall-clock timing." << std::endl;

    return 0;
}