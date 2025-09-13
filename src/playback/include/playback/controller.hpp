// NOTE: File renamed from controller.hpp to playback_controller.hpp to clarify responsibility.
#pragma once
#include "../../decode/include/decode/decoder.hpp"
#include "../../decode/include/decode/frame.hpp"
#include "../../core/include/core/time.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include <chrono>
#include "cache/frame_cache.hpp"
#include "../../timeline/include/timeline/timeline.hpp" // Need snapshot definition for timeline-based FPS

namespace ve::playback {

enum class PlaybackState { Stopped, Playing, Paused };

class PlaybackController {
public:
    using VideoFrameCallback = std::function<void(const decode::VideoFrame&)>;
    using AudioFrameCallback = std::function<void(const decode::AudioFrame&)>;
    using StateChangeCallback = std::function<void(PlaybackState)>;
    using CallbackId = uint64_t;

    PlaybackController();
    ~PlaybackController();

    bool load_media(const std::string& path);
    void close_media();
    bool has_media() const { return decoder_ != nullptr; }

    void play();
    void pause();
    void stop();
    bool seek(int64_t timestamp_us);
    // Decode exactly one frame at the current (post-seek) position then pause again
    void step_once();

    PlaybackState state() const { return state_.load(); }
    int64_t current_time_us() const { return current_time_us_.load(); }
    int64_t duration_us() const { return duration_us_; }
    // Approximate microseconds per frame (from probed fps or stats)
    int64_t frame_duration_guess_us() const;

    // Legacy single-listener setters (clears previous list)
    void set_video_callback(VideoFrameCallback callback) { std::scoped_lock lk(callbacks_mutex_); video_video_entries_.clear(); if(callback) video_video_entries_.emplace_back(CallbackEntry<VideoFrameCallback>{{next_callback_id_++}, std::move(callback)}); }
    void set_audio_callback(AudioFrameCallback callback) { std::scoped_lock lk(callbacks_mutex_); audio_entries_.clear(); if(callback) audio_entries_.emplace_back(CallbackEntry<AudioFrameCallback>{{next_callback_id_++}, std::move(callback)}); }
    void set_state_callback(StateChangeCallback callback) { std::scoped_lock lk(callbacks_mutex_); state_entries_.clear(); if(callback) state_entries_.emplace_back(CallbackEntry<StateChangeCallback>{{next_callback_id_++}, std::move(callback)}); }
    // New multi-listener add/remove APIs (return handle id)
    CallbackId add_video_callback(VideoFrameCallback callback) { if(!callback) return 0; std::scoped_lock lk(callbacks_mutex_); CallbackId id = next_callback_id_++; video_video_entries_.emplace_back(CallbackEntry<VideoFrameCallback>{{id}, std::move(callback)}); return id; }
    CallbackId add_audio_callback(AudioFrameCallback callback) { if(!callback) return 0; std::scoped_lock lk(callbacks_mutex_); CallbackId id = next_callback_id_++; audio_entries_.emplace_back(CallbackEntry<AudioFrameCallback>{{id}, std::move(callback)}); return id; }
    CallbackId add_state_callback(StateChangeCallback callback) { if(!callback) return 0; std::scoped_lock lk(callbacks_mutex_); CallbackId id = next_callback_id_++; state_entries_.emplace_back(CallbackEntry<StateChangeCallback>{{id}, std::move(callback)}); return id; }
    bool remove_video_callback(CallbackId id);
    bool remove_audio_callback(CallbackId id);
    bool remove_state_callback(CallbackId id);
    void clear_state_callbacks() { std::scoped_lock lk(callbacks_mutex_); state_entries_.clear(); }
    // Attach a timeline for snapshot-based playback (read-only consumption)
    void set_timeline(ve::timeline::Timeline* tl) { timeline_ = tl; }

    struct Stats { int64_t frames_displayed = 0; int64_t frames_dropped = 0; double avg_frame_time_ms = 0.0; };
    Stats get_stats() const { return stats_; }
    double cache_hit_ratio() const { int64_t hits=cache_hits_.load(); int64_t lookups=cache_lookups_.load(); return lookups? (double)hits / (double)lookups : 0.0; }

private:
    void playback_thread_main();
    void set_state(PlaybackState new_state);
    void update_frame_stats(double frame_time_ms);
    void decode_one_frame_if_paused(int64_t seek_target_us);
    size_t calculate_optimal_cache_size() const;

    std::unique_ptr<decode::IDecoder> decoder_;
    // Simple frame cache (CPU) for recently decoded frames
    // Adaptive frame cache size based on content resolution and memory constraints
    ve::cache::FrameCache frame_cache_{calculate_optimal_cache_size()}; // Dynamic cache sizing
    std::thread playback_thread_;
    std::atomic<bool> thread_should_exit_{false};
    std::atomic<PlaybackState> state_{PlaybackState::Stopped};
    std::atomic<int64_t> current_time_us_{0};
    std::atomic<bool> seek_requested_{false};
    std::atomic<int64_t> seek_target_us_{0};
    std::atomic<bool> single_step_{false};
    std::atomic<bool> advance_one_frame_{false};
    std::atomic<bool> bypass_cache_once_{false};
    int64_t duration_us_ = 0;
    double probed_fps_ = 0.0; // derived from probe if available
    struct CallbackEntryBase { CallbackId id; };
    template<typename Fn>
    struct CallbackEntry : CallbackEntryBase { Fn fn; };
    std::vector<CallbackEntry<VideoFrameCallback>> video_video_entries_;
    std::vector<CallbackEntry<AudioFrameCallback>> audio_entries_;
    std::vector<CallbackEntry<StateChangeCallback>> state_entries_;
    mutable std::mutex callbacks_mutex_;
    std::atomic<CallbackId> next_callback_id_{1};
    mutable Stats stats_;
    std::atomic<int64_t> frame_count_{0};
    std::atomic<double> total_frame_time_ms_{0.0};
    std::atomic<int64_t> cache_hits_{0};
    std::atomic<int64_t> cache_lookups_{0};
    
    // 60 FPS performance tracking
    std::atomic<int64_t> frame_drops_60fps_{0};
    std::atomic<int64_t> frame_overruns_60fps_{0};
    std::chrono::steady_clock::time_point last_perf_log_{std::chrono::steady_clock::now()};
    // Timeline snapshot consumption (pull model scaffolding)
    ve::timeline::Timeline* timeline_ = nullptr; // non-owning
    uint64_t observed_timeline_version_ = 0;
    // We keep a lightweight shared_ptr to immutable snapshot; structure defined in timeline
    std::shared_ptr<ve::timeline::Timeline::Snapshot> timeline_snapshot_;
    
    // Drift-proof frame stepping for fractional framerates
    struct FrameStepAccum {
        int64_t num{30}, den{1}, rem{0};
        int64_t next_delta_us() {
            const int64_t fps_n = (num > 0 ? num : 30);
            const int64_t N = 1'000'000LL * (den > 0 ? den : 1);
            int64_t base = N / fps_n; rem += N % fps_n;
            if (rem >= fps_n) { base += 1; rem -= fps_n; }
            return base;
        }
    };
    FrameStepAccum step_;
};

} // namespace ve::playback
