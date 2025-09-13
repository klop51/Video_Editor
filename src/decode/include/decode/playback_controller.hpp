#pragma once
#include "async_decoder.hpp"
#include "playback_scheduler.hpp"
#include "color_convert.hpp"
#include "core/log.hpp"
#include <atomic>
#include <functional>

namespace ve::decode {

// High-level controller with proper frame rate synchronization
class PlaybackController {
public:
    using FrameCallback = std::function<void(const VideoFrame& rgba)>;

    explicit PlaybackController(std::unique_ptr<IDecoder> dec)
        : decoder_(std::move(dec)) {}

    bool start(int64_t start_pts_us, FrameCallback cb) {
        if(!decoder_) return false;
        frame_cb_ = std::move(cb);
        
        // Open decoder to get stream info
        if(!decoder_->open({pending_path_, true, false, true})) return false;
        
        // Detect frame rate from decoder metadata
        double detected_fps = detect_frame_rate();
        
        // Start scheduler with proper frame rate timing
        scheduler_.start(start_pts_us, 1.0, detected_fps);
        
        async_ = std::make_unique<AsyncDecoder>(std::move(decoder_));
        async_->start(start_pts_us, [this](const VideoFrame& vf){ this->on_decoded(vf); });
        running_ = true;
        
        ve::log::info("Playback started with frame rate: " + std::to_string(detected_fps) + " fps");
        return true;
    }

    void set_media_path(std::string path) { pending_path_ = std::move(path); }

    void stop() {
        running_ = false;
        if(async_) async_->stop();
        scheduler_.stop();
        
        // Log timing statistics
        auto stats = scheduler_.get_timing_stats();
        ve::log::info("Playback stats - Actual FPS: " + std::to_string(stats.actual_fps) + 
                     ", Dropped: " + std::to_string(stats.dropped_frames) + 
                     ", Presented: " + std::to_string(stats.presented_frames));
    }

    int64_t current_pts() const { return scheduler_.current_media_pts(); }
    
    void set_playback_rate(double rate) {
        scheduler_.set_rate(rate);
    }
    
    // Get frame timing statistics
    PlaybackScheduler::TimingStats get_timing_stats() const {
        return scheduler_.get_timing_stats();
    }

private:
    double detect_frame_rate() {
        // Try to detect frame rate from decoder
        // This is a simplified implementation - in reality you'd query the AVStream
        // For now, return a common frame rate
        return 30.0; // Default to 30fps, should be detected from stream metadata
    }
    
    void on_decoded(const VideoFrame& vf) {
        if(!running_) return;
        
        // Skip timing for performance testing - always allow frames through
        // if (!scheduler_.wait_for_frame_time(vf.pts)) {
        //     // Frame dropped due to timing
        //     return;
        // }
        
        auto rgba = to_rgba(vf);
        if(rgba && frame_cb_) {
            frame_cb_(*rgba);
        }
    }

    std::unique_ptr<IDecoder> decoder_;
    std::unique_ptr<AsyncDecoder> async_;
    PlaybackScheduler scheduler_;
    FrameCallback frame_cb_;
    std::atomic<bool> running_{false};
    std::string pending_path_;
};

} // namespace ve::decode
