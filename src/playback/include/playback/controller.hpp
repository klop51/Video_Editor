#pragma once
#include "decode/decoder.hpp"
#include "decode/frame.hpp"
#include "core/time.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

namespace ve::playback {

enum class PlaybackState {
    Stopped,
    Playing, 
    Paused
};

class Controller {
public:
    using VideoFrameCallback = std::function<void(const decode::VideoFrame&)>;
    using AudioFrameCallback = std::function<void(const decode::AudioFrame&)>;
    using StateChangeCallback = std::function<void(PlaybackState)>;
    
    Controller();
    ~Controller();
    
    // Media loading
    bool load_media(const std::string& path);
    void close_media();
    bool has_media() const { return decoder_ != nullptr; }
    
    // Playback control
    void play();
    void pause();
    void stop();
    bool seek(int64_t timestamp_us);
    
    // State
    PlaybackState state() const { return state_.load(); }
    int64_t current_time_us() const { return current_time_us_.load(); }
    int64_t duration_us() const { return duration_us_; }
    
    // Callbacks
    void set_video_callback(VideoFrameCallback callback) { video_callback_ = std::move(callback); }
    void set_audio_callback(AudioFrameCallback callback) { audio_callback_ = std::move(callback); }
    void set_state_callback(StateChangeCallback callback) { state_callback_ = std::move(callback); }
    
    // Performance
    struct Stats {
        int64_t frames_displayed = 0;
        int64_t frames_dropped = 0;
        double avg_frame_time_ms = 0.0;
    };
    Stats get_stats() const { return stats_; }
    
private:
    void playback_thread_main();
    void set_state(PlaybackState new_state);
    void update_frame_stats(double frame_time_ms);
    
    std::unique_ptr<decode::IDecoder> decoder_;
    
    // Threading
    std::thread playback_thread_;
    std::atomic<bool> thread_should_exit_{false};
    std::atomic<PlaybackState> state_{PlaybackState::Stopped};
    std::atomic<int64_t> current_time_us_{0};
    std::atomic<bool> seek_requested_{false};
    std::atomic<int64_t> seek_target_us_{0};
    
    // Media info
    int64_t duration_us_ = 0;
    
    // Callbacks
    VideoFrameCallback video_callback_;
    AudioFrameCallback audio_callback_;
    StateChangeCallback state_callback_;
    
    // Statistics
    mutable Stats stats_;
    std::atomic<int64_t> frame_count_{0};
    std::atomic<double> total_frame_time_ms_{0.0};
};

} // namespace ve::playback
