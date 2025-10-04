#include "playback/controller.hpp" // renamed class PlaybackController
#include "audio/timeline_audio_manager.hpp"
#include "core/log.hpp"
#include "../../core/include/core/stage_timer.hpp"
#include "core/profiling.hpp"
#include "media_io/media_probe.hpp"
#include "../../config/debug.hpp"
#include "../../render/include/render/render_graph.hpp" // For GpuRenderGraph stop coordination

// Include decoder crash investigation logging with thread tracking
#include <sstream>
#include <thread>
#include <unordered_map>

// Thread-aware logging for decoder crash investigation - DISABLED FOR PERFORMANCE
#define LOG_DECODER_CORE_CALL(method, obj) do { /* DISABLED */ } while(0)
#define LOG_DECODER_CORE_RETURN(method, result) do { /* DISABLED */ } while(0)
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <QCoreApplication>
#include <QThread>

// Windows.h compatibility - undefine potential conflicting macros
#ifdef Int16
#undef Int16
#endif
#ifdef Int32
#undef Int32
#endif
#ifdef Float32
#undef Float32
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

// ChatGPT optimization: High-precision frame pacing helper
// Sleep most of the interval, spin the final ~200µs for accuracy.
static inline void precise_sleep_until(std::chrono::steady_clock::time_point target) {
    using namespace std::chrono;
    for (;;) {
        auto now = steady_clock::now();
        if (now >= target) break;
        auto remain = target - now;
        if (remain > 1ms) {
            std::this_thread::sleep_for(remain - 1ms);
        } else if (remain > 200us) {
            std::this_thread::sleep_for(200us);
        } else {
            // short busy-wait for sub-200µs precision
            do { now = steady_clock::now(); } while (now < target);
            break;
        }
    }
}

namespace {
inline int64_t fps_to_us(double fps) {
    if (fps > 1.0 && fps < 480.0)
        return static_cast<int64_t>((1'000'000.0 / fps) + 0.5);
    return 33'333; // safe default ~30fps
}

#ifdef _WIN32
static size_t rssMiB() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return static_cast<size_t>(pmc.PrivateUsage) / (1024 * 1024);
    }
    return 0;
}
#endif

inline std::string format_hresult(unsigned int hr) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << hr;
    return oss.str();
}

// Establish baseline on first frame; then align to target period.
template <typename Clock>
inline void pace_next(std::chrono::time_point<Clock>& last_frame_time,
                      int64_t target_frame_us,
                      bool& first_frame)
{
    const auto now = Clock::now();
    if (first_frame) {
        last_frame_time = now;      // set baseline once
        first_frame = false;
        return;
    }
    const auto next_target = last_frame_time + std::chrono::microseconds(target_frame_us);
    if (next_target > now) {
        precise_sleep_until(next_target);
        last_frame_time = next_target;  // landed exactly on target
    } else {
        last_frame_time = now;          // behind schedule; catch up baseline
    }
}

// SDL-style continuous timing for improved A/V sync (based on working video player approach)
template <typename Clock>
inline bool pace_next_continuous(std::chrono::time_point<Clock>& last_time,
                                double& frame_timer,
                                int64_t target_frame_us,
                                bool& first_frame,
                                int frame_count = 0)
{
    const auto now = Clock::now();
    
    if (first_frame) {
        last_time = now;
        frame_timer = 0.0;
        first_frame = false;
        ve::log::info("SDL_TIMING: First frame initialized, target_frame_us=" + std::to_string(target_frame_us));
        return true; // Always process first frame
    }
    
    // Calculate elapsed time since last frame (similar to SDL video player)
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time).count();
    last_time = now;
    
    // Accumulate frame timer (continuous timing like SDL approach)
    double frame_delay = target_frame_us / 1000000.0; // Convert to seconds
    double elapsed_sec = elapsed_us / 1000000.0;
    frame_timer += elapsed_sec;
    
    // Log detailed timing every 30 frames for more frequent feedback like working video player
    if (frame_count > 0 && frame_count % 30 == 0) {
        ve::log::info("SDL_TIMING: frame=" + std::to_string(frame_count) + 
                     ", elapsed_us=" + std::to_string(elapsed_us) + 
                     ", frame_timer=" + std::to_string(frame_timer) + 
                     ", frame_delay=" + std::to_string(frame_delay) + 
                     ", drift=" + std::to_string((frame_timer - frame_delay) * 1000.0) + "ms");
    }
    
    // Check if we should process frame(s) - similar to video player's while loop logic
    if (frame_timer >= frame_delay) {
        frame_timer -= frame_delay; // Subtract one frame time
        // Prevent excessive drift accumulation like working video player
        if (frame_timer > frame_delay * 2.0) {
            frame_timer = frame_delay; // Clamp to prevent runaway drift
        }
        return true; // Process frame
    }
    
    return false; // Skip frame, not yet time
}
} // namespace

namespace ve::playback {

PlaybackController::PlaybackController() {
    playback_thread_ = std::thread(&PlaybackController::playback_thread_main, this);
    start_watchdog();
    
    // Initialize performance tracking
    frame_drops_60fps_.store(0);
    frame_overruns_60fps_.store(0);
    last_perf_log_ = std::chrono::steady_clock::now();
    // Allow env to disable decode loop for stability testing
    if (std::getenv("VE_DISABLE_PLAYBACK_DECODE") != nullptr) {
        decode_enabled_.store(false);
    }
}

PlaybackController::~PlaybackController() {
    // Use stopAndJoin for clean shutdown
    stop_.store(true, std::memory_order_release);
    running_.store(false, std::memory_order_release);
    thread_should_exit_.store(true);
    wake_cv_.notify_all();
    
    if (playback_thread_.joinable()) {
        ve::log::info("Destructor: Joining playback thread...");
        playback_thread_.join();
        ve::log::info("Destructor: Playback thread joined successfully");
    }
    
    // Clean up timeline decoder cache
    {
        std::scoped_lock lock(timeline_decoders_mutex_);
        timeline_decoders_.clear();
    }
    
    // Clean up audio pipeline
    if (audio_callback_id_ != 0) {
        remove_audio_callback(audio_callback_id_);
    }
    if (audio_pipeline_) {
        audio_pipeline_->shutdown();
        audio_pipeline_.reset();
    }

    stop_watchdog();
}

// Stop coordination helpers
void PlaybackController::onSafetyTrip_() {
    // Edge-triggered: only first caller logs and triggers shutdown
    bool was_running = !stop_.exchange(true, std::memory_order_acq_rel);
    if (!was_running) return; // Already stopping
    
    if (!safety_log_emitted_.exchange(true, std::memory_order_acq_rel)) {
        ve::log::warn("SAFETY: frame cap reached – initiating STOP");
    }
    
    // Stop producers on their own threads (non-blocking)
    if (renderer_) {
        renderer_->requestStop();
    }
    if (timeline_audio_manager_) {
        timeline_audio_manager_->requestStop();
    }
    if (audio_pipeline_) {
        audio_pipeline_->shutdown();
    }
    
    // Wake decode loop if waiting
    wake_cv_.notify_all();
    
    set_state(PlaybackState::Stopping);
    safety_tripped_.store(true, std::memory_order_release);
}

bool PlaybackController::waitForPlaybackExit_(int ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (running_.load(std::memory_order_acquire) && 
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return !running_.load(std::memory_order_acquire);
}

bool PlaybackController::load_media(const std::string& path) {
    LOG_DECODER_CORE_CALL("load_media", path.c_str());
    ve::log::info("Loading media: " + path);
    
    // Check if this is an MP4 file that might have hardware acceleration issues
    std::string filename = path;
    std::transform(filename.begin(), filename.end(), filename.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    bool is_mp4 = filename.find(".mp4") != std::string::npos;
    
    LOG_DECODER_CORE_CALL("create_decoder", "factory");
    decoder_ = decode::create_decoder();
    if (!decoder_) {
        ve::log::error("Failed to create decoder");
        LOG_DECODER_CORE_RETURN("load_media", false);
        return false;
    }
    LOG_DECODER_CORE_RETURN("create_decoder", "success");
    
    decode::OpenParams params;
    params.filepath = path;
    params.video = true;
    params.audio = true;
    params.preferred_audio_stream_index = probed_audio_stream_index_; // Use the selected stream index
    
    bool success = false;
    
    if (is_mp4) {
        // For MP4 files, use software decoding to prevent crashes
        ve::log::info("MP4 file detected - using software decoding to prevent crashes: " + path);
        params.hw_accel = false;
        LOG_DECODER_CORE_CALL("decoder->open", "software_mode");
        success = decoder_->open(params);
        LOG_DECODER_CORE_RETURN("decoder->open", success);
        
        if (success) {
            ve::log::info("MP4 software decoding successful");
        } else {
            ve::log::error("MP4 software decoding failed for: " + path);
            decoder_.reset();
            LOG_DECODER_CORE_RETURN("load_media", false);
            return false;
        }
    } else {
        // For non-MP4 files, try hardware acceleration first
        params.hw_accel = true; // Try hardware acceleration first
        
        // First attempt: try with hardware acceleration
        success = decoder_->open(params);
        
        if (!success) {
            ve::log::warn("Hardware acceleration failed, trying software decoding fallback");
            
            // Reset decoder for clean retry
            decoder_.reset();
            decoder_ = decode::create_decoder();
            if (!decoder_) {
                ve::log::error("Failed to create decoder for software fallback");
                return false;
            }
            
            // Second attempt: disable hardware acceleration
            params.hw_accel = false;
            success = decoder_->open(params);
            
            if (!success) {
                ve::log::error("Both hardware and software decoding failed for: " + path);
                decoder_.reset();
                return false;
            } else {
                ve::log::info("Software decoding fallback successful");
            }
        } else {
            ve::log::info("Hardware accelerated decoding successful");
        }
    }
    
    // Get duration from media probe and extract audio stream information
    auto probe_result = ve::media::probe_file(path);
    uint32_t probed_sample_rate = 48000;  // Default fallback
    uint16_t probed_channels = 2;         // Default fallback
    bool found_audio_stream = false;
    
    if (probe_result.success && probe_result.duration_us > 0) {
        duration_us_ = probe_result.duration_us;
        
        // Extract audio stream characteristics for universal compatibility
        // Prefer stereo (2-channel) streams over multi-channel for compatibility
        int best_score = -1;
        const ve::media::StreamInfo* preferred_audio_stream = nullptr;
        
        for(const auto& s : probe_result.streams) {
            if(s.type == "audio" && s.sample_rate > 0 && s.channels > 0) {
                // Score streams: prefer stereo (2 channels) over multi-channel
                int score = 0;
                if(s.channels == 2) {
                    score = 100; // Highest priority for stereo
                } else if(s.channels == 1) {
                    score = 50;  // Mono is second best
                } else {
                    score = 10;  // Multi-channel gets lowest priority
                }
                
                if(score > best_score) {
                    best_score = score;
                    preferred_audio_stream = &s;
                }
            }
        }
        
        if(preferred_audio_stream) {
            // Safe conversions with range checks
            probed_sample_rate = static_cast<uint32_t>(std::min(preferred_audio_stream->sample_rate, static_cast<int64_t>(UINT32_MAX)));
            probed_channels = static_cast<uint16_t>(std::min(preferred_audio_stream->channels, static_cast<int64_t>(UINT16_MAX)));
            probed_audio_stream_index_ = preferred_audio_stream->index; // Store the stream index
            found_audio_stream = true;
            ve::log::info("Selected preferred audio stream " + std::to_string(preferred_audio_stream->index) + ": " + std::to_string(preferred_audio_stream->sample_rate) + " Hz, " + 
                         std::to_string(preferred_audio_stream->channels) + " channels, codec: " + preferred_audio_stream->codec);
        }
        
        // Extract video stream characteristics for FPS detection
        for(const auto& s : probe_result.streams) {
            if(s.type == "video" && s.fps > 0) {
                probed_fps_ = s.fps; 
                // Initialize drift-proof stepping with detected fps
                if (s.fps == 29.97) {
                    step_.num = 30000; step_.den = 1001;
                } else if (s.fps == 23.976) {
                    step_.num = 24000; step_.den = 1001;
                } else if (s.fps == 59.94) {
                    step_.num = 60000; step_.den = 1001;  // 59.94 fps (NTSC 60fps)
                } else if (s.fps >= 59.9 && s.fps <= 60.1) {
                    step_.num = 60; step_.den = 1;        // 60fps (exact)
                } else {
                    step_.num = static_cast<int64_t>(s.fps + 0.5); step_.den = 1;
                }
                step_.rem = 0; // reset accumulator
                break; // Use first video stream found
            }
        }
        
        if (!found_audio_stream) {
            ve::log::warn("No audio stream detected in video file, using defaults: " + 
                         std::to_string(probed_sample_rate) + " Hz, " + std::to_string(probed_channels) + " channels");
        }
        
        ve::log::info("Media duration: " + std::to_string(duration_us_) + " us (" + 
                     std::to_string(static_cast<double>(duration_us_) / 1000000.0) + " seconds)");
    } else {
        ve::log::warn("Could not determine media duration, using default audio format");
        duration_us_ = 0;
    }
    
    // Store probed audio characteristics for pipeline configuration
    probed_audio_sample_rate_ = probed_sample_rate;
    probed_audio_channels_ = probed_channels;
    
    ve::log::info("Media loaded successfully: " + path);
    set_state(PlaybackState::Stopped);
    current_time_us_.store(0);

    // Immediately decode first frame for instant preview (avoid blank)
    decode_one_frame_if_paused(0);
    return true;
}

void PlaybackController::close_media() {
    stopAndJoin();  // Ensure fully quiesced before clearing resources
    
    // Resources already cleared by stopAndJoin, but reset state
    duration_us_ = 0;
    current_time_us_.store(0);

    // Reset stats
    stats_ = Stats{};
    frame_count_.store(0);
    total_frame_time_ms_.store(0.0);
}

bool PlaybackController::has_timeline_content() const {
    if (!timeline_) {
        ve::log::debug("has_timeline_content: No timeline set");
        return false;
    }
    
    // Check if timeline has any tracks with segments
    const auto& tracks = timeline_->tracks();
    ve::log::debug("has_timeline_content: Found " + std::to_string(tracks.size()) + " tracks");
    
    for (const auto& track : tracks) {
        if (!track->segments().empty()) {
            ve::log::debug("has_timeline_content: Found track with " + std::to_string(track->segments().size()) + " segments");
            return true;  // Found at least one segment
        }
    }
    
    ve::log::debug("has_timeline_content: No segments found in any track");
    return false;  // No segments found
}

void PlaybackController::play() {
    // Check if we have media to play (either single file or timeline content)
    if (!has_media() && !has_timeline_content()) {
        ve::log::warn("Cannot play: no media loaded and no timeline content");
        return;
    }
    
    ve::log::info("Starting playback");
    
    // Start timeline audio if available - TEMPORARILY DISABLED FOR DEBUGGING SIGABRT
    // DEBUGGING: Temporarily commented out to test if audio causes SIGABRT
    /*
    if (timeline_audio_manager_) {
        timeline_audio_manager_->start_playback();
    }
    */
    
    set_state(PlaybackState::Playing);
}

void PlaybackController::pause() {
    if (state_.load() == PlaybackState::Playing) {
        ve::log::info("Pausing playback");
        
        // Pause timeline audio if available
        if (timeline_audio_manager_) {
            timeline_audio_manager_->pause_playback();
        }
        
        set_state(PlaybackState::Paused);
    }
}

void PlaybackController::stop() {
    // Legacy stop() - prefer stopAndJoin() for clean shutdown
    // Just delegate to stopAndJoin for proper ordering
    stopAndJoin();
}

void PlaybackController::requestStop() {
    // Idempotent: exchange returns true if already stopping
    if (stop_.exchange(true, std::memory_order_acq_rel)) {
        return; // Already requested
    }
    
    ve::log::info("PlaybackController::requestStop");
    
    // Stop all producers on their own threads (non-blocking)
    if (renderer_) {
        renderer_->requestStop();
        ve::log::info("GpuRenderGraph: requestStop - drained current frame and stopped accepting frames");
    }
    if (timeline_audio_manager_) {
        timeline_audio_manager_->requestStop();
        ve::log::info("TimelineAudioManager: requestStop");
    }
    if (audio_pipeline_) {
        audio_pipeline_->shutdown();
    }
    
    // Wake decode loop if waiting
    wake_cv_.notify_all();
}

void PlaybackController::stopAsync() {
    requestStop();
    ve::log::debug("PlaybackController::stopAsync: stop requested without blocking");
}

void PlaybackController::force_cpu_render_fallback(unsigned int error_code) {
    last_gpu_error_.store(error_code, std::memory_order_release);
    bool already_forced = gpu_forced_cpu_.exchange(true, std::memory_order_acq_rel);
    if (already_forced) {
        ve::log::info("PlaybackController::force_cpu_render_fallback already active");
        return;
    }

    ve::log::warn("PlaybackController: forcing CPU render fallback (hr=0x" + format_hresult(error_code) + ")");
    if (renderer_) {
        renderer_->requestStop();
    }

    if (timeline_audio_manager_) {
        timeline_audio_manager_->pause_playback();
    }

    if (audio_pipeline_) {
        audio_pipeline_->pause_output();
    }

    wake_cv_.notify_all();
}

void PlaybackController::stopAndJoin() {
    if (QCoreApplication* app = QCoreApplication::instance()) {
        if (QThread::currentThread() == app->thread()) {
            ve::log::info("PlaybackController::stopAndJoin called on UI thread – deferring to stopAsync()");
            stopAsync();
            return;
        }
    }

    // 1) Trip stop flag and silence all producers
    requestStop();
    
    // 2) Join playback thread (avoid deadlock if called from playback thread)
    if (playback_thread_.joinable()) {
        if (std::this_thread::get_id() != playback_thread_.get_id()) {
            ve::log::info("PlaybackController::stopAndJoin: Joining playback thread");
            playback_thread_.join();
        } else {
            ve::log::warn("PlaybackController::stopAndJoin: Called from playback thread, using timed wait");
            (void)waitForPlaybackExit_(500); // Bounded fallback
        }
    } else {
        // Thread not joinable, use bounded wait
        ve::log::info("PlaybackController::stopAndJoin: Thread not joinable, using timed wait");
        (void)waitForPlaybackExit_(500);
    }
    
    // 3) Drain transient queues (do NOT clear callback registrations)
    frame_queue_.clear();
    ve::log::info("PlaybackController::stopAndJoin: Cleared frame queue");
    
    // 4) Free heavy resources AFTER threads are quiet (order matters!)
    // Decoder first (stops FFmpeg operations)
    decoder_.reset();
    ve::log::info("PlaybackController::stopAndJoin: Released decoder");
    
    // Audio resources
    if (audio_callback_id_ != 0) {
        if (remove_audio_callback(audio_callback_id_)) {
            ve::log::info("PlaybackController::stopAndJoin: Unregistered audio callback");
        }
        audio_callback_id_ = 0;
    }
    audio_callback_registered_.store(false, std::memory_order_release);
    audio_pipeline_.reset();
    timeline_audio_manager_.reset();
    ve::log::info("PlaybackController::stopAndJoin: Released audio resources");
    
    // Clear timeline decoder cache
    clear_timeline_decoder_cache();
    
    // Update state
    set_state(PlaybackState::Stopped);
    current_time_us_.store(0);
    ve::log::info("PlaybackController: stopped cleanly");
    
    // 5) Reset stop flag for next playback
    stop_.store(false, std::memory_order_release);
    running_.store(false, std::memory_order_release);
    safety_tripped_.store(false, std::memory_order_release);
    safety_log_emitted_.store(false, std::memory_order_release);
    ve::log::info("PlaybackController::stopAndJoin: Ready for next playback");
}

bool PlaybackController::seek(int64_t timestamp_us) {
    if (!has_media()) {
        return false;
    }
    
    ve::log::debug("Seeking to: " + std::to_string(timestamp_us) + " us");
    
    // Seek timeline audio if available
    if (timeline_audio_manager_) {
        ve::TimePoint seek_position{timestamp_us, 1000000};
        timeline_audio_manager_->seek_to(seek_position);
    }
    
    seek_target_us_.store(timestamp_us);
    seek_requested_.store(true);
    
    return true;
}

void PlaybackController::step_once() {
    if(!has_media()) return;
    single_step_.store(true);
    // Bypass cache for the next iteration so we decode strictly the next frame
    bypass_cache_once_.store(true);
    advance_one_frame_.store(false);
    // Force one-frame decode path even if currently paused/stopped
    if(state_.load() != PlaybackState::Playing) {
        set_state(PlaybackState::Playing);
    }
}

void PlaybackController::set_state(PlaybackState new_state) {
    PlaybackState old_state = state_.exchange(new_state);
    if (old_state != new_state) {
        VE_DEBUG_ONLY(ve::log::info(std::string("Playback state change: ") + std::to_string(static_cast<int>(old_state)) + " -> " + std::to_string(static_cast<int>(new_state))));
        std::vector<CallbackEntry<StateChangeCallback>> copy;
        {
            std::scoped_lock lk(callbacks_mutex_);
            copy = state_entries_;
        }
        for(auto &entry : copy) if(entry.fn) entry.fn(new_state);
    }
}

void PlaybackController::update_frame_stats(double frame_time_ms) {
    int64_t count = frame_count_.fetch_add(1) + 1;
    double total = total_frame_time_ms_.fetch_add(frame_time_ms) + frame_time_ms;
    
    stats_.frames_displayed = count;
    stats_.avg_frame_time_ms = total / static_cast<double>(count);
}

void PlaybackController::dispatch_video_frame(decode::VideoFrame&& frame) {
    frame_queue_.push(std::move(frame));

    decode::VideoFrame queued;
    while (frame_queue_.try_pop(queued)) {
        std::vector<CallbackEntry<VideoFrameCallback>> copy;
        {
            std::scoped_lock lk(callbacks_mutex_);
            copy = video_video_entries_;
        }

        for (auto& entry : copy) {
            if (stop_.load(std::memory_order_acquire)) {
                return;
            }

            if (!entry.fn) {
                continue;
            }

            try {
                entry.fn(queued);
            } catch (const std::exception& e) {
                ve::log::error("Video callback exception: " + std::string(e.what()));
            } catch (...) {
                ve::log::error("Video callback unknown exception - continuing");
            }
        }
    }
}

void PlaybackController::watchdogTick() {
#ifdef _WIN32
    size_t cur = rssMiB();
    size_t last = watchdog_last_rss_.load(std::memory_order_relaxed);
    static size_t peak = 0;
    peak = std::max(peak, cur);
    if (cur > last + 64) {
        ve::log::warn("PlaybackController memory watchdog: RSS rose to " + std::to_string(cur) + " MiB (peak " + std::to_string(peak) + " MiB) – trimming caches");
        frame_cache_.clear();
        frame_cache_.set_capacity(calculate_optimal_cache_size());
        if (renderer_) {
            renderer_->trim();
        }
    }
    watchdog_last_rss_.store(cur, std::memory_order_relaxed);
#endif
}

void PlaybackController::watchdog_loop() {
#ifdef _WIN32
    using namespace std::chrono_literals;
    while (!watchdog_exit_.load(std::memory_order_acquire)) {
        watchdogTick();
        for (int i = 0; i < 10 && !watchdog_exit_.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(100ms);
        }
    }
#endif
}

void PlaybackController::start_watchdog() {
#ifdef _WIN32
    watchdog_exit_.store(false, std::memory_order_release);
    watchdog_thread_ = std::thread(&PlaybackController::watchdog_loop, this);
#endif
}

void PlaybackController::stop_watchdog() {
#ifdef _WIN32
    watchdog_exit_.store(true, std::memory_order_release);
    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
#endif
}

void PlaybackController::playback_thread_main() {
    ve::log::info("Playback thread started");
    running_.store(true, std::memory_order_release);
    
    int64_t frames = 0;
    auto last_frame_time = std::chrono::steady_clock::now();
    bool first_frame = true;
    const int64_t target_frame_duration_us = fps_to_us(probed_fps_);
    
    while (!stop_.load(std::memory_order_acquire)) {
        // SAFETY cap - check early before any processing
        static int64_t kCap = [] {
            if (const char* s = std::getenv("VE_SAFETY_CAP")) {
                const long long val = std::atoll(s);
                if (val > 0) {
                    return static_cast<int64_t>(val);
                }
            }
            return static_cast<int64_t>(1'000'000'000); // effectively "infinite"
        }();
        if (++frames >= kCap) {
            onSafetyTrip_();
            break; // <<< leave loop immediately
        }
        
        // Guard: always check before dispatch
        if (stop_.load(std::memory_order_acquire)) break;
        
        auto frame_start = std::chrono::steady_clock::now();

        // Refresh timeline snapshot if timeline attached and version changed
        if(timeline_) {
            uint64_t v = timeline_->version();
            if(v != observed_timeline_version_) {
                timeline_snapshot_ = timeline_->snapshot(); // immutable copy
                observed_timeline_version_ = v;
                ve::log::debug("Playback refreshed timeline snapshot version " + std::to_string(v));
            }
        }
        
        // Handle seek requests
        if (seek_requested_.load()) {
            int64_t seek_target = seek_target_us_.load();
            if (decoder_ && decoder_->seek_microseconds(seek_target)) {
                current_time_us_.store(seek_target);
                ve::log::info("Seek completed to: " + std::to_string(seek_target) + " us");
                first_frame = true;                                   // baseline will be re-established
                last_frame_time = std::chrono::steady_clock::now();                // reset clock reference
                // If not playing, decode a preview frame immediately
                if(state_.load() != PlaybackState::Playing) {
                    decode_one_frame_if_paused(seek_target);
                }
            }
            seek_requested_.store(false);
        }
        
        // Process frames if playing
        if (state_.load() == PlaybackState::Playing) {
            // Guard: check stop before frame processing
            if (stop_.load(std::memory_order_acquire)) break;
            
            // SIGABRT FIX: Add exception protection around frame processing
            try {
                // Master switch: if decode disabled, just advance time with frame pacing
                if (!decode_enabled_.load()) {
                int64_t delta = step_.next_delta_us();
                int64_t next_pts = current_time_us_.load() + delta;
                if (duration_us_ > 0 && next_pts >= duration_us_) {
                    current_time_us_.store(duration_us_);
                    set_state(ve::playback::PlaybackState::Stopped);
                } else {
                    current_time_us_.store(next_pts);
                }
                using namespace std::chrono;
                precise_sleep_until(std::chrono::steady_clock::now() + microseconds(delta));
                continue; // Skip actual decoding
            }
            bool has_content = false;
            bool is_timeline_mode = false;
            
            // Determine playback mode: single-media or timeline
            if (decoder_) {
                has_content = true;
                is_timeline_mode = false;
            } else if (timeline_ && timeline_snapshot_) {
                // DEADLOCK FIX: Use snapshot instead of has_timeline_content() to avoid cross-thread mutex conflicts
                // The has_timeline_content() function calls timeline_->tracks() which can deadlock with main thread
                has_content = true;
                is_timeline_mode = true;
            }
            
            if (has_content) {
                // VE_DEBUG_ONLY - DISABLED FOR PERFORMANCE:
                // Processing frame logging causes video interruptions
                
                int64_t current_pts = current_time_us_.load();
                
                if (is_timeline_mode) {
                    // Timeline-driven playback
                    if (decode_timeline_frame(current_pts)) {
                        // Timeline audio is handled by TimelineAudioManager
                        // Video frame dispatching is handled in decode_timeline_frame
                        // Update timing for next frame
                        auto next_frame_delta = step_.next_delta_us();
                        current_time_us_.store(current_pts + next_frame_delta);
                    }
                } else {
                    // Single-media playback (existing logic)
            
            // If a single-step is pending, we will bypass cache below and ask decoder for next frame.
            bool bypass_cache = bypass_cache_once_.exchange(false);

            int64_t single_media_pts = current_time_us_.load();
            
            // Attempt cache reuse first (only exact pts match for now), unless bypassed for single-step
        if(seek_requested_.load()==false && !bypass_cache) { // if not in middle of seek handling
                    ve::cache::CachedFrame cached; cache_lookups_.fetch_add(1);
                ve::cache::FrameKey key; key.pts_us = single_media_pts;
                // VE_DEBUG_ONLY - DISABLED: Cache lookup logging causes performance issues
                if(frame_cache_.get(key, cached)) {
                        VE_PROFILE_SCOPE_UNIQ("playback.cache_hit");
                    cache_hits_.fetch_add(1);
                    // VE_DEBUG_ONLY - DISABLED: Cache HIT logging causes performance issues
                    decode::VideoFrame vf;
                    vf.width = cached.width;
                    vf.height = cached.height;
                    vf.pts = single_media_pts;
                    vf.format = cached.format;
                    vf.color_space = cached.color_space;
                    vf.color_range = cached.color_range;

                    const size_t srcSize = cached.data.size();
                    if (srcSize > 0) {
                        if (vf.data.capacity() < srcSize) {
                            vf.data.reserve(srcSize);
                        }
                        vf.data.resize(srcSize);
                        std::memcpy(vf.data.data(), cached.data.data(), srcSize);
                    } else {
                        vf.data.clear();
                    }
                    vf.format = cached.format;
                    vf.color_space = cached.color_space;
                    vf.color_range = cached.color_range;
                    dispatch_video_frame(std::move(vf));
                    // IMPORTANT: Advance time for next frame even when cache hits
                    // Prefer drift-proof step accumulator for fractional framerates
                    int64_t delta = step_.next_delta_us();
                    int64_t next_pts = single_media_pts + delta;
                    if (duration_us_ > 0 && next_pts >= duration_us_) {
                        current_time_us_.store(duration_us_);
                        set_state(ve::playback::PlaybackState::Stopped);   // notify UI we're done
                        ve::log::info("Reached end of stream at: " + std::to_string(duration_us_) + " us (cache hit path) - stopping");
                        continue; // exit playback loop cleanly
                    }
                    current_time_us_.store(next_pts);
                    // VE_DEBUG_ONLY - DISABLED: Time advancement logging causes performance issues
                    // SDL-style continuous timing for improved A/V sync (prevent micro-timing drift)
                    int64_t current_frame_count = frame_count_.load();
                    if (!pace_next_continuous(last_continuous_time_, frame_timer_, target_frame_duration_us, first_continuous_frame_, static_cast<int>(current_frame_count))) {
                        // Not yet time for next frame - continue playback loop
                        std::this_thread::sleep_for(std::chrono::microseconds(100)); // Short sleep to prevent busy wait
                        continue;
                    }

                    continue; // Skip decoding new frame this iteration
                } else {
                    // VE_DEBUG_ONLY - DISABLED: Cache MISS logging causes performance issues
                }
            }
            
            // FIXED: The original logic prevented continuous frame reading for playback
            // Always bypass cache during normal playback to read consecutive frames
            // Cache should only be used for seeking/scrubbing, not continuous playback
            if (!bypass_cache) {
                bypass_cache = true; // Always read new frames during playback
            }

            std::shared_ptr<ve::core::StageTimer> frame_timer;
            if (decoder_) {
                frame_timer = std::make_shared<ve::core::StageTimer>();
                frame_timer->begin();
            }

            // Read video frame (decode path)
            auto video_frame = decoder_->read_video();
            
            if (video_frame) {
                if (frame_timer) {
                    frame_timer->afterDecode();
                    video_frame->timing = frame_timer;
                }
                // Future: traverse snapshot (immutable) to determine which clip/segment is active
                // For now we rely solely on decoder PTS ordering.
                
                current_time_us_.store(video_frame->pts);

                // Advance time for next frame (matching cache hit logic)
                int64_t delta = step_.next_delta_us();
                int64_t next_pts = video_frame->pts + delta;
                if (duration_us_ > 0 && next_pts >= duration_us_) {
                    // Don't advance beyond end of stream, let next iteration handle EOF
                } else {
                    current_time_us_.store(next_pts);
                }

                const int frame_width = video_frame->width;
                const int frame_height = video_frame->height;
                dispatch_video_frame(std::move(*video_frame));

                // Update frame stats immediately after frame dispatch for reliable FPS counting
                auto frame_duration = std::chrono::duration<double, std::milli>(frame_start - last_frame_time);
                update_frame_stats(frame_duration.count());

                // High-performance frame timing (same target for decode path)
                if (!first_frame) {
                    auto target_interval = std::chrono::microseconds(target_frame_duration_us);
                    auto frame_end = std::chrono::steady_clock::now();
                    auto actual_duration = frame_end - last_frame_time;

                    // Balanced optimization for 4K 60fps content - targeting 80-83% performance
                    bool is_4k_60fps = (frame_width >= 3840 && frame_height >= 2160 && probed_fps_ >= 59.0);
                    
                    // Only sleep if we have time left in the frame interval
                    if (actual_duration < target_interval) {
                        auto sleep_time = target_interval - actual_duration;
                        
                        if (is_4k_60fps) {
                            // For 4K 60fps: Balanced sleep optimization for 80-83% performance
                            // Reduce sleep for performance while maintaining stability
                            if (sleep_time > std::chrono::microseconds(1500)) { // Sleep for 1.5ms+ gaps
                                std::this_thread::sleep_for(sleep_time - std::chrono::microseconds(500));
                            }
                            // Brief spin-wait for final precision
                            auto spin_start = std::chrono::high_resolution_clock::now();
                            while (std::chrono::high_resolution_clock::now() - spin_start < std::chrono::microseconds(300)) {
                                std::this_thread::yield();
                            }
                        } else if (probed_fps_ > 50.0) {
                            // For other high frame rates: Standard precise timing
                            std::this_thread::sleep_for(sleep_time);
                        } else {
                            // For lower frame rates, allow some tolerance
                            if (sleep_time > std::chrono::microseconds(500)) {
                                std::this_thread::sleep_for(sleep_time);
                            }
                        }
                    } else if (is_4k_60fps) {
                        // Frame is taking too long - log performance issue
                        frame_overruns_60fps_.fetch_add(1);
                        auto overrun_us = std::chrono::duration_cast<std::chrono::microseconds>(actual_duration - target_interval).count();
                        if (overrun_us > 1000) { // Log if more than 1ms overrun
                            ve::log::warn("4K 60fps frame overrun: " + std::to_string(overrun_us) + "us (total overruns: " + std::to_string(frame_overruns_60fps_.load()) + ")");
                        }
                    }
                    
                    // Periodic performance logging for 60fps content
                    if (is_4k_60fps) {
                        auto now = std::chrono::steady_clock::now();
                        auto since_last_log = now - last_perf_log_;
                        if (since_last_log > std::chrono::seconds(5)) { // Log every 5 seconds
                            int64_t total_frames = frame_count_.load();
                            int64_t overruns = frame_overruns_60fps_.load();
                            double overrun_rate = total_frames > 0 ? static_cast<double>(overruns) / static_cast<double>(total_frames) * 100.0 : 0.0;
                            ve::log::info("4K 60fps performance: " + std::to_string(total_frames) + " frames, " + 
                                        std::to_string(overruns) + " overruns (" + std::to_string(overrun_rate) + "%)");
                            last_perf_log_ = now;
                        }
                    }
                }

                // SDL-style continuous timing baseline update (no drift, improved A/V sync)
                int64_t current_frame_count = frame_count_.load();
                if (!pace_next_continuous(last_continuous_time_, frame_timer_, target_frame_duration_us, first_continuous_frame_, static_cast<int>(current_frame_count))) {
                    // This should rarely happen since we already passed timing check above
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }

                // Update performance stats - DEBUG LOGGING DISABLED FOR PERFORMANCE
                auto frame_time = std::chrono::duration<double, std::milli>(last_frame_time - frame_start);
                update_frame_stats(frame_time.count());
                
                // Let video_frame unique_ptr go out of scope here - this will call VideoFrame destructor

                // Periodic frame log (every 30 frames) for diagnostics
                if ((stats_.frames_displayed % 30) == 0) {
                    ve::log::debug("Playback frames displayed=" + std::to_string(stats_.frames_displayed));
                }
                
            } else {
                // Handle null video frame - could be EOF, memory error, or decode failure
                ve::log::warn("decoder_->read_video() returned null frame - checking if end of stream");
                
                // Handle end-of-stream and avoid busy loop when decoder has no more frames
                int64_t cur = current_time_us_.load();
                int64_t delta = step_.next_delta_us();
                int64_t next_pts = cur + delta;
                
                if (duration_us_ > 0 && next_pts >= duration_us_) {
                    current_time_us_.store(duration_us_);
                    set_state(ve::playback::PlaybackState::Stopped);
                    ve::log::info("Reached end of stream (null video frame) at: " + std::to_string(duration_us_) + " us - stopping");
                    continue;
                } else {
                    // Not end of stream, advance time anyway to prevent hang
                    current_time_us_.store(next_pts);
                    ve::log::warn("Advanced time to: " + std::to_string(next_pts) + " (null video frame - possible decode error)");
                    
                    // Add a small delay to prevent busy loop
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            
            // Read audio frame (single read to maintain perfect video timing)
            auto audio_frame = [&](){ /* VE_PROFILE_SCOPE_UNIQ("playbook.decode_audio"); */ return decoder_->read_audio(); }();
            
            
            
            if (audio_frame) {
                { // audio callback dispatch (profiling removed to avoid variable shadow warning on MSVC)
                    std::vector<CallbackEntry<AudioFrameCallback>> copy;
                    
                    { std::scoped_lock lk(callbacks_mutex_); copy = audio_entries_; }
                    
                    // PHASE 1 DIAGNOSTIC: Enforce single audio sink during debugging (throttled logging)
                    static auto last_sink_log = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    bool should_log = std::chrono::duration_cast<std::chrono::seconds>(now - last_sink_log).count() >= 5;
                    
                    if (copy.size() > 1) {
                        if (should_log) {
                            ve::log::error("PHASE1_MULTIPLE_SINKS_ERROR: " + std::to_string(copy.size()) +
                                          " audio callbacks registered; this can cause competing submits and timing issues. " +
                                          "Using only the first callback during Phase 1 debugging.");
                        }
                        // Limit to first callback only during Phase 1
                        copy.resize(1);
                    } else if (copy.size() == 0) {
                        if (should_log) {
                            ve::log::warn("PHASE1_NO_AUDIO_SINK: No audio callbacks registered - audio will not be heard.");
                        }
                    } else {
                        if (should_log) {
                            ve::log::info("PHASE1_SINGLE_SINK_OK: 1 audio callback registered - good for testing.");
                        }
                    }
                    
                    if (should_log) {
                        last_sink_log = now;
                    }
                    
                    // DEBUG LOGGING DISABLED FOR PERFORMANCE - audio callback dispatch
                    for(size_t i = 0; i < copy.size(); ++i) {
                        // ChatGPT Stop Token System: Guard audio callback dispatch
                        if (stop_.load(std::memory_order_acquire)) break; // don't post after stopping
                        
                        auto &entry = copy[i];
                        if(entry.fn) {
                            entry.fn(*audio_frame);
                        }
                    }
                }
            }
            
            // DEBUG LOGGING DISABLED FOR PERFORMANCE - timeline audio processing
            
                // Process timeline audio (mix multiple tracks)
                if (timeline_audio_manager_ && timeline_) {
                    // Convert current time to TimePoint for timeline audio processing
                    ve::TimePoint current_pos{current_time_us_.load(), 1000000};
                    
                    // Process timeline audio tracks at current position
                    // This will mix audio from multiple timeline tracks and send to audio pipeline
                    timeline_audio_manager_->process_timeline_audio(current_pos, 1024);  // Standard frame size
                }
            } // End single-media block
        } // End has_content check
            } catch (const std::exception& e) {
                ve::log::error("SIGABRT FIX: Frame processing exception caught: " + std::string(e.what()));
                // Continue playing but skip this frame to prevent crash
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            } catch (...) {
                ve::log::error("SIGABRT FIX: Unknown frame processing exception caught - continuing");
                // Continue playing but skip this frame to prevent crash
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        } else {
            // Not playing, sleep longer
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        // Handle single-step completion (after any frame decode above)
        if(single_step_.load() && state_.load() == PlaybackState::Playing) {
            // After issuing single step we pause right after first new pts displayed
            // We track step_start_pts; when it changes post-step we pause.
            static thread_local int64_t step_start_pts = -1;
            if(step_start_pts < 0) step_start_pts = current_time_us_.load();
            if(current_time_us_.load() != step_start_pts) {
                single_step_.store(false);
                step_start_pts = -1;
                set_state(PlaybackState::Paused);
            }
        }
        
        // DEBUG LOGGING DISABLED FOR PERFORMANCE - main loop iteration completed
    }
    
    // ChatGPT Crash Prevention: Clear running state on thread exit
    running_.store(false, std::memory_order_release);
    ve::log::info("Playback thread exited cleanly");
}

int64_t PlaybackController::frame_duration_guess_us() const {
    // Prefer probed fps
    // If timeline snapshot present, attempt to use active clip's fps
    if(timeline_snapshot_) {
        // Determine active time
        int64_t cur = current_time_us_.load();
        for(const auto &trk : timeline_snapshot_->tracks) {
            if(trk.type() != ve::timeline::Track::Video) continue;
            for(const auto &seg : trk.segments()) {
                auto start = seg.start_time.to_rational();
                auto dur = seg.duration.to_rational();
                int64_t seg_start_us = (start.num * 1'000'000) / start.den;
                int64_t seg_end_us = seg_start_us + (dur.num * 1'000'000) / dur.den;
                if(cur >= seg_start_us && cur < seg_end_us) {
                    auto clip_it = timeline_snapshot_->clips.find(seg.clip_id);
                    if(clip_it != timeline_snapshot_->clips.end() && clip_it->second.source) {
                        auto fr = clip_it->second.source->frame_rate;
                        if(fr.num > 0 && fr.den > 0) {
                            long double fps = (long double)fr.num / (long double)fr.den;
                            if(fps > 1.0L && fps < 480.0L) return (int64_t)((1'000'000.0L / fps) + 0.5L);
                        }
                    }
                }
            }
        }
    }
    if(probed_fps_ > 1.0 && probed_fps_ < 480.0) {
        return static_cast<int64_t>( (1'000'000.0 / probed_fps_) + 0.5 );
    }
    // Fallback: derive from recent frame stats if available, else 33_333
    if(stats_.frames_displayed >= 2) {
        double avg_ms = stats_.avg_frame_time_ms;
        if(avg_ms > 5 && avg_ms < 100) return static_cast<int64_t>(avg_ms * 1000.0);
    }
    return 33'333; // ~30fps
}

void PlaybackController::decode_one_frame_if_paused(int64_t seek_target_us) {
    if(!decoder_ || state_.load() == PlaybackState::Playing) return;
    // Attempt to read a single frame at/after seek target immediately for preview
    auto frame = decoder_->read_video();
    if(frame) {
        current_time_us_.store(frame->pts);
        // Cache it with buffer reuse to avoid reallocating preview storage each time
        ve::cache::CachedFrame cf;
        cf.width = frame->width;
        cf.height = frame->height;
        cf.format = frame->format;
        cf.color_space = frame->color_space;
        cf.color_range = frame->color_range;

        const size_t srcSize = frame->data.size();
        if (srcSize > 0) {
            auto &dst = cf.data;
            if (dst.capacity() < srcSize) {
                dst.reserve(srcSize);
            }
            dst.resize(srcSize);
            std::memcpy(dst.data(), frame->data.data(), srcSize);
        } else {
            cf.data.clear();
        }
        ve::cache::FrameKey putKey; putKey.pts_us = frame->pts; frame_cache_.put(putKey, std::move(cf));
        size_t callback_count = 0;
        {
            std::scoped_lock lk(callbacks_mutex_);
            callback_count = video_video_entries_.size();
        }
        auto video_callback_start = std::chrono::steady_clock::now();
        int64_t current_frame_count = frame_count_.load();
        const int64_t frame_pts = frame->pts;
        ve::log::info(std::string("PlaybackController: Invoking ") + std::to_string(callback_count) + " video callbacks for frame pts=" + std::to_string(frame_pts) + " frame=" + std::to_string(current_frame_count));
        dispatch_video_frame(std::move(*frame));
        auto video_callback_end = std::chrono::steady_clock::now();
        auto callback_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(video_callback_end - video_callback_start).count();
        if (current_frame_count % 60 == 0) { // Log timing every 60 frames like video player
            ve::log::info("VIDEO_TIMING: frame=" + std::to_string(current_frame_count) + 
                         ", pts=" + std::to_string(frame_pts) + 
                         ", callback_duration=" + std::to_string(callback_duration_us) + "us");
        }
    } else {
        // Fallback: keep current_time at seek target
        current_time_us_.store(seek_target_us);
    }
}

bool PlaybackController::remove_video_callback(CallbackId id) {
    if(id==0) return false; std::scoped_lock lk(callbacks_mutex_);
    auto it = std::remove_if(video_video_entries_.begin(), video_video_entries_.end(), [id](auto &e){return e.id==id;});
    bool removed = it != video_video_entries_.end();
    video_video_entries_.erase(it, video_video_entries_.end());
    return removed;
}
bool PlaybackController::remove_audio_callback(CallbackId id) {
    if(id==0) return false; std::scoped_lock lk(callbacks_mutex_);
    auto it = std::remove_if(audio_entries_.begin(), audio_entries_.end(), [id](auto &e){return e.id==id;});
    bool removed = it != audio_entries_.end();
    audio_entries_.erase(it, audio_entries_.end());
    return removed;
}
bool PlaybackController::remove_state_callback(CallbackId id) {
    if(id==0) return false; std::scoped_lock lk(callbacks_mutex_);
    auto it = std::remove_if(state_entries_.begin(), state_entries_.end(), [id](auto &e){return e.id==id;});
    bool removed = it != state_entries_.end();
    state_entries_.erase(it, state_entries_.end());
    return removed;
}

size_t PlaybackController::calculate_optimal_cache_size() const {
    constexpr size_t kHardCapFrames = 32;
    return kHardCapFrames;
}

bool PlaybackController::initialize_audio_pipeline() {
    // Add mutex lock to prevent concurrent initialization
    static std::mutex init_mutex;
    std::lock_guard<std::mutex> init_lock(init_mutex);
    
    // Guard against multiple initializations
    if (audio_pipeline_) {
        ve::log::info("Audio pipeline already initialized, skipping");
        return true;
    }
    
    ve::log::info("Initializing audio pipeline for playback controller");
    
    // Make sure any previous callback registration is torn down before proceeding
    if (audio_callback_id_ != 0) {
        if (remove_audio_callback(audio_callback_id_)) {
            ve::log::info("PlaybackController::initialize_audio_pipeline: Removed prior audio callback");
        }
        audio_callback_id_ = 0;
    }

    {
        std::scoped_lock lk(callbacks_mutex_);
        if (!audio_entries_.empty()) {
            audio_entries_.clear();
            ve::log::info("PlaybackController::initialize_audio_pipeline: Cleared pending audio callbacks to prevent echo");
        }
    }

    audio_callback_registered_.store(false, std::memory_order_release);

    // Add a small delay to ensure cleanup completes before new registration
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Create audio pipeline configuration using probed video file characteristics
    ve::audio::AudioPipelineConfig pipeline_config;
    pipeline_config.sample_rate = probed_audio_sample_rate_;
    pipeline_config.channel_count = probed_audio_channels_;
    pipeline_config.format = ve::audio::SampleFormat::Float32;
    
    // Calculate optimal buffer size based on sample rate (maintain ~10ms periods)
    // Standard WASAPI period is typically 480 frames @ 48kHz = 10ms
    // Scale proportionally for other sample rates: buffer_frames = (sample_rate * 10ms) / 1000ms
    uint32_t optimal_buffer_size = static_cast<uint32_t>((probed_audio_sample_rate_ * 10) / 1000);
    
    // Ensure buffer size is reasonable (clamp between 128 and 2048 frames)
    pipeline_config.buffer_size = std::clamp(optimal_buffer_size, 128u, 2048u);
    
    ve::log::info("Dynamic audio configuration: " + std::to_string(pipeline_config.sample_rate) + " Hz, " +
                 std::to_string(pipeline_config.channel_count) + " channels, " +
                 std::to_string(pipeline_config.buffer_size) + " frames (~" +
                 std::to_string((double)pipeline_config.buffer_size * 1000.0 / pipeline_config.sample_rate) + " ms)");
    
    // Create the audio pipeline
    audio_pipeline_ = ve::audio::AudioPipeline::create(pipeline_config);
    if (!audio_pipeline_) {
        ve::log::error("Failed to create audio pipeline");
        return false;
    }
    
    // Initialize the audio pipeline
    if (!audio_pipeline_->initialize()) {
        ve::log::error("Failed to initialize audio pipeline");
        audio_pipeline_.reset();
        return false;
    }
    
    // Check what format was actually negotiated by the device after initialization
    const auto& final_config = audio_pipeline_->get_config();
    device_audio_sample_rate_ = final_config.sample_rate;
    device_audio_channels_ = final_config.channel_count;
    
    // Log the final negotiation result for universal video compatibility
    if (device_audio_sample_rate_ != probed_audio_sample_rate_ || device_audio_channels_ != probed_audio_channels_) {
        ve::log::info("Audio format negotiation completed: video(" + std::to_string(probed_audio_sample_rate_) + "Hz/" + 
                     std::to_string(probed_audio_channels_) + "ch) → device(" + std::to_string(device_audio_sample_rate_) + 
                     "Hz/" + std::to_string(device_audio_channels_) + "ch) - automatic resampling enabled");
    } else {
        ve::log::info("Audio format negotiation: perfect match achieved - no resampling needed for optimal performance");
    }
    
    // Start audio output
    if (!audio_pipeline_->start_output()) {
        ve::log::error("Failed to start audio output");
        audio_pipeline_.reset();
        return false;
    }
    
    // Register audio callback to receive frames from decoder exactly once per playback lifecycle
    bool expected = false;
    if (!audio_callback_registered_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        ve::log::warn("PlaybackController::initialize_audio_pipeline: Audio callback already registered; skipping duplicate registration");
    } else {
        audio_callback_id_ = add_audio_callback([this](const decode::AudioFrame& frame) {
        // DEBUG LOGGING DISABLED FOR PERFORMANCE - audio callback entry/validation
        
        if (audio_pipeline_ && !master_muted_.load()) {
            // PTS-based deduplication to prevent echo
            int64_t last_pts = last_audio_pts_.exchange(frame.pts);
            if (last_pts == frame.pts) {
                ve::log::debug("Skipping duplicate audio frame with PTS: " + std::to_string(frame.pts));
                return;
            }
            
            // Audio frame processing debug logging disabled for performance
            // Debug logging to check for duplicate frames (reduced frequency)
            // static int log_counter = 0;
            // if (++log_counter % 100 == 0) { // Log every 100th frame to reduce spam
            //     ve::log::info("Processing audio frame: pts=" + std::to_string(frame.pts) + 
            //                  ", samples=" + std::to_string(frame.data.size()) +
            //                  ", rate=" + std::to_string(frame.sample_rate) +
            //                  ", channels=" + std::to_string(frame.channels));
            // }
            
            // Convert decode::AudioFrame to audio::AudioFrame for pipeline
            
            // Simple format conversion - default to Float32
            ve::audio::SampleFormat audio_format = ve::audio::SampleFormat::Float32;
            
            // Calculate sample count assuming 4 bytes per sample (Float32)
            size_t bytes_per_sample = 4;
            uint32_t sample_count = static_cast<uint32_t>(frame.data.size() / (bytes_per_sample * frame.channels));
            
            // Create timestamp from pts (convert microseconds to rational)
            ve::TimePoint timestamp(frame.pts, 1000000); // pts is in microseconds
            
            // Create audio frame with the converted parameters - DEBUG LOGGING DISABLED FOR PERFORMANCE
            
            auto audio_frame = ve::audio::AudioFrame::create_from_data(
                static_cast<uint32_t>(frame.sample_rate),
                static_cast<uint16_t>(frame.channels),
                sample_count,
                audio_format,
                timestamp,
                frame.data.data(),
                frame.data.size()
            );
            
            if (audio_frame) {
                // Send to audio pipeline for processing and output
                audio_pipeline_->process_audio_frame(audio_frame);
                
                // Update stats
                audio_stats_.frames_processed++;
                audio_stats_.total_frames_processed++;
            }
        }
    });

        if (audio_callback_id_ == 0) {
            audio_callback_registered_.store(false, std::memory_order_release);
            ve::log::error("PlaybackController::initialize_audio_pipeline: Failed to register audio callback (id=0)");
        } else {
            ve::log::info("PlaybackController::initialize_audio_pipeline: Registered audio callback with id=" + std::to_string(audio_callback_id_));
        }
    }
    
    // Initialize audio stats with actual values
    // Use the final negotiated configuration for stats
    audio_stats_.sample_rate = device_audio_sample_rate_;
    audio_stats_.channels = device_audio_channels_;
    audio_stats_.buffer_size = final_config.buffer_size;
    audio_stats_.frames_processed = 0;
    audio_stats_.total_frames_processed = 0;
    audio_stats_.buffer_underruns = 0;
    
        // Reset audio control state
    master_muted_.store(false);
    master_volume_.store(1.0f);
    
    ve::log::info("Audio pipeline initialization complete");
    return true;
}

std::shared_ptr<ve::audio::ProfessionalAudioMonitoringSystem> PlaybackController::get_monitoring_system() const {
    if (!audio_pipeline_) {
        return nullptr;
    }
    return audio_pipeline_->get_monitoring_system();
}

bool PlaybackController::enable_professional_monitoring() {
    if (!audio_pipeline_) {
        ve::log::error("Cannot enable professional monitoring: audio pipeline not available");
        return false;
    }
    
    // Create default monitoring configuration
    ve::audio::ProfessionalAudioMonitoringSystem::MonitoringConfig config;
    config.target_platform = "EBU"; // Default to EBU R128
    config.enable_loudness_monitoring = true;
    config.enable_peak_rms_meters = true;
    config.enable_audio_scopes = true;
    config.update_rate_hz = 30;
    config.reference_level_db = -20.0;
    
    bool result = audio_pipeline_->enable_professional_monitoring(config);
    if (result) {
        ve::log::info("Professional audio monitoring enabled");
    } else {
        ve::log::error("Failed to enable professional audio monitoring");
    }
    return result;
}

bool PlaybackController::initialize_timeline_audio() {
    if (!audio_pipeline_) {
        ve::log::error("Cannot initialize timeline audio: audio pipeline not available");
        return false;
    }
    
    ve::log::info("Initializing timeline audio manager");
    
    // Create timeline audio manager
    timeline_audio_manager_ = ve::audio::TimelineAudioManager::create(audio_pipeline_.get());
    if (!timeline_audio_manager_) {
        ve::log::error("Failed to create timeline audio manager");
        return false;
    }
    
    // Initialize the timeline audio manager
    if (!timeline_audio_manager_->initialize()) {
        ve::log::error("Failed to initialize timeline audio manager");
        timeline_audio_manager_.reset();
        return false;
    }
    
    // Connect to timeline if available
    if (timeline_) {
        if (!timeline_audio_manager_->set_timeline(timeline_)) {
            ve::log::warn("Failed to connect timeline to audio manager");
        }
    }
    
    ve::log::info("Timeline audio manager initialized successfully");
    return true;
}

bool PlaybackController::set_timeline_track_mute(ve::timeline::TrackId track_id, bool muted) {
    if (!timeline_audio_manager_) {
        ve::log::warn("Timeline audio manager not available");
        return false;
    }
    return timeline_audio_manager_->set_track_mute(track_id, muted);
}

bool PlaybackController::set_timeline_track_solo(ve::timeline::TrackId track_id, bool solo) {
    if (!timeline_audio_manager_) {
        ve::log::warn("Timeline audio manager not available");
        return false;
    }
    return timeline_audio_manager_->set_track_solo(track_id, solo);
}

bool PlaybackController::set_timeline_track_gain(ve::timeline::TrackId track_id, float gain_db) {
    if (!timeline_audio_manager_) {
        ve::log::warn("Timeline audio manager not available");
        return false;
    }
    return timeline_audio_manager_->set_track_gain(track_id, gain_db);
}

bool PlaybackController::set_timeline_track_pan(ve::timeline::TrackId track_id, float pan) {
    if (!timeline_audio_manager_) {
        ve::log::warn("Timeline audio manager not available");
        return false;
    }
    return timeline_audio_manager_->set_track_pan(track_id, pan);
}

bool PlaybackController::decode_timeline_frame(int64_t timestamp_us) {
    if (!timeline_) {
        return false;
    }
    
    // Convert timestamp to TimePoint
    ve::TimePoint timeline_position{timestamp_us, 1000000};
    
    // Find video tracks with segments at current time
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        if (track->type() != ve::timeline::Track::Video) {
            continue;  // Skip non-video tracks
        }
        
        // Find segment at current time
        const ve::timeline::Segment* segment = track->get_segment_at_time(timeline_position);
        if (!segment) {
            continue;  // No segment at this time
        }
        
        // Get media source for this segment
        const auto* clip = timeline_->get_clip(segment->clip_id);
        if (!clip || !clip->source) {
            continue;  // No media source
        }
        
        // Extract file path from MediaSource
        const std::string& file_path = clip->source->path;
        if (file_path.empty()) {
            ve::log::warn("Timeline segment has empty media source path");
            continue;
        }
        
        // Get decoder for this media source
        decode::IDecoder* decoder = nullptr;
        {
            std::scoped_lock lock(timeline_decoders_mutex_);
            auto it = timeline_decoders_.find(file_path);
            if (it != timeline_decoders_.end() && it->second) {
                decoder = it->second.get();
            } else {
                // Create new decoder if not in cache
                auto new_decoder = decode::create_decoder();
                if (!new_decoder) {
                    ve::log::error("Failed to create decoder for timeline media: " + file_path);
                    continue;
                }
                
                // Open the media file
                decode::OpenParams params;
                params.filepath = file_path;
                params.video = true;
                params.audio = true;
                
                if (!new_decoder->open(params)) {
                    ve::log::error("Failed to open timeline media file: " + file_path);
                    continue;
                }
                
                decoder = new_decoder.get();
                timeline_decoders_[file_path] = std::move(new_decoder);
                ve::log::info("Created and cached timeline decoder for: " + file_path);
            }
        }
        
        if (!decoder) {
            continue;  // Failed to get decoder
        }
        
        // Calculate position within the segment's source media
        // segment->start_time is timeline position, we need source position
        int64_t source_timestamp_us = timestamp_us - segment->start_time.to_rational().num / segment->start_time.to_rational().den * 1000000;
        source_timestamp_us = std::max(0LL, source_timestamp_us);  // Clamp to non-negative
        
        // Seek and decode frame
        if (decoder->seek_microseconds(source_timestamp_us)) {
            auto video_frame = decoder->read_video();
            if (video_frame) {
                // Update frame timestamp to timeline time
                video_frame->pts = timestamp_us;
                
                // Dispatch to video callbacks
                dispatch_video_frame(std::move(*video_frame));
                return true;  // Successfully decoded and dispatched frame
            }
        }
    }
    
    return false;  // No video frame found at this time
}

decode::IDecoder* PlaybackController::get_timeline_decoder_at_time(int64_t timestamp_us) {
    if (!timeline_) {
        return nullptr;
    }
    
    // Convert timestamp to TimePoint
    ve::TimePoint timeline_position{timestamp_us, 1000000};
    
    // Find video tracks with segments at current time to get the media source
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        if (track->type() != ve::timeline::Track::Video) {
            continue;  // Skip non-video tracks
        }
        
        // Find segment at current time
        const ve::timeline::Segment* segment = track->get_segment_at_time(timeline_position);
        if (!segment) {
            continue;  // No segment at this time
        }
        
        // Get media source for this segment
        const auto* clip = timeline_->get_clip(segment->clip_id);
        if (!clip || !clip->source) {
            continue;  // No media source
        }
        
        // Extract file path from MediaSource
        const std::string& file_path = clip->source->path;
        if (file_path.empty()) {
            ve::log::warn("Timeline segment has empty media source path");
            continue;
        }
        
        // Check decoder cache first
        {
            std::scoped_lock lock(timeline_decoders_mutex_);
            auto it = timeline_decoders_.find(file_path);
            if (it != timeline_decoders_.end() && it->second) {
                // Return raw pointer to cached decoder
                return it->second.get();
            }
        }
        
        // Create new decoder if not in cache
        auto decoder = decode::create_decoder();
        if (!decoder) {
            ve::log::error("Failed to create decoder for timeline media: " + file_path);
            continue;
        }
        
        // Open the media file
        decode::OpenParams params;
        params.filepath = file_path;
        params.video = true;
        params.audio = true;
        
        if (!decoder->open(params)) {
            ve::log::error("Failed to open timeline media file: " + file_path);
            continue;
        }
        
        // Cache the decoder and return raw pointer
        decode::IDecoder* result = decoder.get();
        {
            std::scoped_lock lock(timeline_decoders_mutex_);
            timeline_decoders_[file_path] = std::move(decoder);
        }
        
        ve::log::info("Created and cached timeline decoder for: " + file_path);
        return result;
    }
    
    return nullptr;  // No video segment found at this time
}

bool PlaybackController::has_timeline_video_at_time(int64_t timestamp_us) const {
    if (!timeline_) {
        return false;
    }
    
    ve::TimePoint timeline_position{timestamp_us, 1000000};
    const auto& tracks = timeline_->tracks();
    
    for (const auto& track : tracks) {
        if (track->type() == ve::timeline::Track::Video) {
            const ve::timeline::Segment* segment = track->get_segment_at_time(timeline_position);
            if (segment) {
                return true;  // Found video segment at this time
            }
        }
    }
    
    return false;  // No video segments at this time
}

void PlaybackController::clear_timeline_decoder_cache() {
    std::scoped_lock lock(timeline_decoders_mutex_);
    timeline_decoders_.clear();
    ve::log::info("Timeline decoder cache cleared");
}

} // namespace ve::playback
