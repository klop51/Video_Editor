// NOTE: File renamed from controller.hpp to playback_controller.hpp to clarify responsibility.
#pragma once
#include "decode/decoder.hpp"
#include "decode/frame.hpp"
#include "core/time.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include "cache/frame_cache.hpp"
// Forward declaration to avoid heavy include
namespace ve { namespace timeline { class Timeline; } }

namespace ve::playback {

enum class PlaybackState { Stopped, Playing, Paused };

class PlaybackController {
public:
    using VideoFrameCallback = std::function<void(const decode::VideoFrame&)>;
    using AudioFrameCallback = std::function<void(const decode::AudioFrame&)>;
    using StateChangeCallback = std::function<void(PlaybackState)>;

    PlaybackController();
    ~PlaybackController();

    bool load_media(const std::string& path);
    void close_media();
    bool has_media() const { return decoder_ != nullptr; }

    void play();
    void pause();
    void stop();
    bool seek(int64_t timestamp_us);

    PlaybackState state() const { return state_.load(); }
    int64_t current_time_us() const { return current_time_us_.load(); }
    int64_t duration_us() const { return duration_us_; }

    void set_video_callback(VideoFrameCallback callback) { video_callback_ = std::move(callback); }
    void set_audio_callback(AudioFrameCallback callback) { audio_callback_ = std::move(callback); }
    void set_state_callback(StateChangeCallback callback) { state_callback_ = std::move(callback); }
    // Attach a timeline for snapshot-based playback (read-only consumption)
    void set_timeline(ve::timeline::Timeline* tl) { timeline_ = tl; }

    struct Stats { int64_t frames_displayed = 0; int64_t frames_dropped = 0; double avg_frame_time_ms = 0.0; };
    Stats get_stats() const { return stats_; }
    double cache_hit_ratio() const { int64_t hits=cache_hits_.load(); int64_t lookups=cache_lookups_.load(); return lookups? (double)hits / (double)lookups : 0.0; }

private:
    void playback_thread_main();
    void set_state(PlaybackState new_state);
    void update_frame_stats(double frame_time_ms);

    std::unique_ptr<decode::IDecoder> decoder_;
    // Simple frame cache (CPU) for recently decoded frames
    ve::cache::FrameCache frame_cache_{180}; // ~6 seconds at 30fps
    std::thread playback_thread_;
    std::atomic<bool> thread_should_exit_{false};
    std::atomic<PlaybackState> state_{PlaybackState::Stopped};
    std::atomic<int64_t> current_time_us_{0};
    std::atomic<bool> seek_requested_{false};
    std::atomic<int64_t> seek_target_us_{0};
    int64_t duration_us_ = 0;
    VideoFrameCallback video_callback_;
    AudioFrameCallback audio_callback_;
    StateChangeCallback state_callback_;
    mutable Stats stats_;
    std::atomic<int64_t> frame_count_{0};
    std::atomic<double> total_frame_time_ms_{0.0};
    std::atomic<int64_t> cache_hits_{0};
    std::atomic<int64_t> cache_lookups_{0};
    // Timeline snapshot consumption (pull model scaffolding)
    ve::timeline::Timeline* timeline_ = nullptr; // non-owning
    uint64_t observed_timeline_version_ = 0;
    // We keep a lightweight shared_ptr to immutable snapshot; structure defined in timeline
    std::shared_ptr<void> timeline_snapshot_; // intentionally opaque here to avoid including timeline header
};

} // namespace ve::playback
