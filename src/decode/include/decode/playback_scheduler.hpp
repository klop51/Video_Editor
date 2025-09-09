#pragma once
#include <cstdint>
#include <functional>
#include <chrono>
#include <thread>

namespace ve::decode {

// Enhanced playback scheduler with proper frame rate synchronization
class PlaybackScheduler {
public:
    using Clock = std::chrono::steady_clock;
    using FramePresentCallback = std::function<void(int64_t pts_us)>;

    void start(int64_t start_pts_us, double rate = 1.0, double frame_rate = 0.0) {
        start_media_pts_us_ = start_pts_us;
        rate_ = rate;
        frame_rate_ = frame_rate;
        start_wall_ = Clock::now();
        running_ = true;
        
        // Calculate frame duration for proper timing
        if (frame_rate_ > 0.0) {
            frame_duration_us_ = static_cast<int64_t>(1000000.0 / frame_rate_);
            use_frame_timing_ = true;
        } else {
            use_frame_timing_ = false;
        }
        
        last_frame_time_ = start_wall_;
    }

    void stop() { running_ = false; }

    int64_t current_media_pts() const {
        if(!running_) return start_media_pts_us_;
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_wall_).count();
        return start_media_pts_us_ + static_cast<int64_t>(static_cast<double>(elapsed) * rate_);
    }
    
    // Wait for next frame presentation time
    bool wait_for_frame_time(int64_t target_pts_us) {
        if (!running_ || !use_frame_timing_) return true;
        
        auto now = Clock::now();
        auto target_wall_us = static_cast<int64_t>(static_cast<double>(target_pts_us - start_media_pts_us_) / rate_);
        auto target_wall_time = start_wall_ + std::chrono::microseconds(target_wall_us);
        
        if (now < target_wall_time) {
            // Sleep until presentation time
            std::this_thread::sleep_until(target_wall_time);
            return true;
        }
        
        // Frame is late - check if we should drop it
        auto late_duration = now - target_wall_time;
        auto late_us = std::chrono::duration_cast<std::chrono::microseconds>(late_duration).count();
        return late_us < frame_duration_us_; // Drop if more than one frame late
    }
    
    // Calculate expected frame presentation time
    int64_t get_next_frame_pts() const {
        if (!use_frame_timing_) return current_media_pts();
        
        auto current_pts = current_media_pts();
        int64_t frame_boundary = (current_pts / frame_duration_us_) * frame_duration_us_;
        if (frame_boundary <= current_pts) {
            frame_boundary += frame_duration_us_;
        }
        return frame_boundary;
    }

    void set_rate(double r) { 
        rate_ = r; 
        // Recalculate frame timing if rate changes
        if (use_frame_timing_ && frame_rate_ > 0.0) {
            frame_duration_us_ = static_cast<int64_t>(1000000.0 / (frame_rate_ * rate_));
        }
    }
    
    void set_frame_rate(double fps) {
        frame_rate_ = fps;
        if (fps > 0.0) {
            frame_duration_us_ = static_cast<int64_t>(1000000.0 / fps);
            use_frame_timing_ = true;
        } else {
            use_frame_timing_ = false;
        }
    }
    
    // Get frame timing statistics
    struct TimingStats {
        double actual_fps = 0.0;
        int64_t avg_frame_duration_us = 0;
        int dropped_frames = 0;
        int presented_frames = 0;
    };
    
    TimingStats get_timing_stats() const {
        return timing_stats_;
    }

private:
    bool running_ = false;
    int64_t start_media_pts_us_ = 0;
    double rate_ = 1.0;
    double frame_rate_ = 0.0;
    Clock::time_point start_wall_{};
    Clock::time_point last_frame_time_{};
    
    // Frame timing
    bool use_frame_timing_ = false;
    int64_t frame_duration_us_ = 0;
    
    // Statistics
    mutable TimingStats timing_stats_{};
};

} // namespace ve::decode
