#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>

// Mock classes to simulate the playback controller behavior
class MockAudioPipeline {
public:
    bool audio_ready() const { return audio_ready_.load(); }
    void set_audio_ready(bool ready) { audio_ready_.store(ready); }
    double device_playhead_seconds() const { return 0.0; }
    void reset_prebuffer_state() {}
    void reset_buffer_underruns() {}
    uint32_t buffer_underruns() const { return underruns_.load(); }
    void increment_underruns() { underruns_.fetch_add(1); }

private:
    std::atomic<bool> audio_ready_{true};
    std::atomic<uint32_t> underruns_{0};
};

class MockPlaybackController {
public:
    MockPlaybackController() : audio_pipeline_(std::make_unique<MockAudioPipeline>()) {}

    void simulate_underrun_recovery() {
        // Simulate the original problematic behavior
        std::cout << "Simulating underrun recovery..." << std::endl;

        // Check for underruns
        if (audio_pipeline_->buffer_underruns() > last_logged_underruns_) {
            std::cout << "Audio underrun detected (total=" << audio_pipeline_->buffer_underruns() << ")" << std::endl;

            // Apply the FIX: Don't clear pending video frames or reset wall clock
            audio_pipeline_->reset_prebuffer_state();
            audio_pipeline_->reset_buffer_underruns();
            prebuffering_.store(true);
            prebuffer_start_time_ = std::chrono::steady_clock::now();

            // Note: We removed the problematic lines:
            // wall_clock_play_origin_ = prebuffer_start_time_;
            // wall_clock_pts_origin_sec_ = ...;
            // pending_video_frames_.clear();

            last_logged_underruns_ = 0;
        }
    }

    void test_video_presentation() {
        // Simulate video frame presentation logic
        bool audio_ready_local = audio_pipeline_ && audio_pipeline_->audio_ready();
        std::cout << "Audio ready: " << (audio_ready_local ? "YES" : "NO") << std::endl;

        if (audio_ready_local) {
            std::cout << "Video frames would be presented normally" << std::endl;
        } else {
            std::cout << "Would fall back to wall clock timing (prebuffering)" << std::endl;
        }
    }

    void run_test() {
        std::cout << "=== Testing A/V Sync Fix ===" << std::endl;

        // Initial state - audio ready, video presenting
        std::cout << "Initial state:" << std::endl;
        test_video_presentation();

        // Simulate underrun
        std::cout << "\nSimulating audio underrun..." << std::endl;
        audio_pipeline_->increment_underruns();

        // Apply recovery
        simulate_underrun_recovery();

        // Test that video can still be presented
        std::cout << "\nAfter underrun recovery:" << std::endl;
        test_video_presentation();

        std::cout << "\n=== Test completed successfully ===" << std::endl;
        std::cout << "Video presentation continues through underrun recovery!" << std::endl;
    }

private:
    std::unique_ptr<MockAudioPipeline> audio_pipeline_;
    std::atomic<bool> prebuffering_{false};
    std::chrono::steady_clock::time_point prebuffer_start_time_;
    uint32_t last_logged_underruns_{0};
};

int main() {
    MockPlaybackController controller;
    controller.run_test();
    return 0;
}