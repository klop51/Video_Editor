#include "playback/controller.hpp" // renamed class PlaybackController
#include "audio/timeline_audio_manager.hpp"
#include "core/log.hpp"
#include "core/profiling.hpp"
#include "media_io/media_probe.hpp"
#include "../../config/debug.hpp"
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>

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

namespace ve::playback {

PlaybackController::PlaybackController() {
    playback_thread_ = std::thread(&PlaybackController::playback_thread_main, this);
    
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
    thread_should_exit_.store(true);
    if (playback_thread_.joinable()) {
        playback_thread_.join();
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
}

bool PlaybackController::load_media(const std::string& path) {
    ve::log::info("Loading media: " + path);
    
    // Check if this is an MP4 file that might have hardware acceleration issues
    std::string filename = path;
    std::transform(filename.begin(), filename.end(), filename.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    bool is_mp4 = filename.find(".mp4") != std::string::npos;
    
    decoder_ = decode::create_decoder();
    if (!decoder_) {
        ve::log::error("Failed to create decoder");
        return false;
    }
    
    decode::OpenParams params;
    params.filepath = path;
    params.video = true;
    params.audio = true;
    
    bool success = false;
    
    if (is_mp4) {
        // For MP4 files, use software decoding to prevent crashes
        ve::log::info("MP4 file detected - using software decoding to prevent crashes: " + path);
        params.hw_accel = false;
        success = decoder_->open(params);
        
        if (success) {
            ve::log::info("MP4 software decoding successful");
        } else {
            ve::log::error("MP4 software decoding failed for: " + path);
            decoder_.reset();
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
    
    // Get duration from media probe
    auto probe_result = ve::media::probe_file(path);
    if (probe_result.success && probe_result.duration_us > 0) {
        duration_us_ = probe_result.duration_us;
        // Derive fps from first video stream if present
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
                break; 
            }
        }
        ve::log::info("Media duration: " + std::to_string(duration_us_) + " us (" + 
                     std::to_string(static_cast<double>(duration_us_) / 1000000.0) + " seconds)");
    } else {
        ve::log::warn("Could not determine media duration");
        duration_us_ = 0;
    }
    
    ve::log::info("Media loaded successfully: " + path);
    set_state(PlaybackState::Stopped);
    current_time_us_.store(0);

    // Immediately decode first frame for instant preview (avoid blank)
    decode_one_frame_if_paused(0);
    return true;
}

void PlaybackController::close_media() {
    stop();
    decoder_.reset();
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
}void PlaybackController::play() {
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
    if (state_.load() != PlaybackState::Stopped) {
        ve::log::info("Stopping playback");
        
        // Stop timeline audio if available
        if (timeline_audio_manager_) {
            timeline_audio_manager_->stop_playback();
        }
        
        set_state(PlaybackState::Stopped);
        current_time_us_.store(0);
    }
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

void PlaybackController::playback_thread_main() {
    ve::log::info("Playback thread started");
    
    auto last_frame_time = std::chrono::steady_clock::now();
    bool first_frame = true;
    
    while (!thread_should_exit_.load()) {
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
                first_frame = true; // Reset timing after seek
                // If not playing, decode a preview frame immediately
                if(state_.load() != PlaybackState::Playing) {
                    decode_one_frame_if_paused(seek_target);
                }
            }
            seek_requested_.store(false);
        }
        
        // Process frames if playing
        if (state_.load() == PlaybackState::Playing) {
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
                VE_DEBUG_ONLY({
                    bool seek_req = seek_requested_.load();
                    std::string mode = is_timeline_mode ? "Timeline" : "Single-media";
                    ve::log::info("Processing frame in playback thread, mode=" + mode + ", state=Playing, seek_requested=" + std::string(seek_req ? "true" : "false"));
                });
                
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
                VE_DEBUG_ONLY(ve::log::info("Cache lookup for pts: " + std::to_string(key.pts_us)));
                if(frame_cache_.get(key, cached)) {
                        VE_PROFILE_SCOPE_UNIQ("playback.cache_hit");
                    cache_hits_.fetch_add(1);
                    VE_DEBUG_ONLY(ve::log::info("Cache HIT for pts: " + std::to_string(key.pts_us)));
                    decode::VideoFrame vf; vf.width = cached.width; vf.height = cached.height; vf.pts = single_media_pts; vf.data = cached.data; // copy buffer
                    vf.format = cached.format;
                    vf.color_space = cached.color_space;
                    vf.color_range = cached.color_range;
                    {
                        std::vector<CallbackEntry<VideoFrameCallback>> copy;
                        { std::scoped_lock lk(callbacks_mutex_); copy = video_video_entries_; }
                        VE_DEBUG_ONLY(ve::log::info("Dispatching " + std::to_string(copy.size()) + " video callbacks (cache hit) for pts=" + std::to_string(vf.pts)));
                        for(auto &entry : copy) if(entry.fn) entry.fn(vf);
                        VE_DEBUG_ONLY(ve::log::info("Dispatched video callbacks (cache hit) for pts=" + std::to_string(vf.pts)));
                    }
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
                    VE_DEBUG_ONLY(ve::log::info("Advanced time to: " + std::to_string(next_pts) + " (cache hit path)"));
                    // ChatGPT optimization: Frame pacing for cache hit - precise alignment to frame interval
                    {
                        using namespace std::chrono;

                        auto target_interval = microseconds(delta);  // delta = frame us (from fps accumulator or 1e6/fps)
                        if (first_frame) {
                            last_frame_time = steady_clock::now();
                            first_frame = false;
                        }

                        auto next_target = last_frame_time + target_interval;
                        precise_sleep_until(next_target);

                        last_frame_time = steady_clock::now();

                        // keep existing time advance:
                        // current_time_us_.store(next_pts); // already done above
                    }
                    continue; // Skip decoding new frame this iteration
                } else {
                    VE_DEBUG_ONLY(ve::log::info("Cache MISS for pts: " + std::to_string(key.pts_us) + ", proceeding to decoder"));
                }
            } else {
                VE_DEBUG_ONLY(ve::log::info(bypass_cache ? "Bypassing cache for single-step" : "Skipping frame processing because seek_requested=true"));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (!bypass_cache) continue; // for seek, skip; for bypass, fall-through to decoder read
            }

            // Read video frame (decode path)
            VE_PROFILE_SCOPE_UNIQ("playback.decode_video");
            auto video_frame = decoder_->read_video();
            
            if (video_frame) {
                // Future: traverse snapshot (immutable) to determine which clip/segment is active
                // For now we rely solely on decoder PTS ordering.
                // Cache store (basic) - store raw data buffer
                ve::cache::CachedFrame cf; cf.width = video_frame->width; cf.height = video_frame->height; cf.data = video_frame->data; // copy
                cf.format = video_frame->format;
                cf.color_space = video_frame->color_space;
                cf.color_range = video_frame->color_range;
                ve::cache::FrameKey putKey; putKey.pts_us = video_frame->pts;
                VE_DEBUG_ONLY(ve::log::info("Cache PUT for pts=" + std::to_string(putKey.pts_us) + 
                                             ", size=" + std::to_string(cf.width) + "x" + std::to_string(cf.height)));
                frame_cache_.put(putKey, std::move(cf));
                VE_DEBUG_ONLY(ve::log::info("Cache size now=" + std::to_string(frame_cache_.size())));
                
                current_time_us_.store(video_frame->pts);

                // Advance time for next frame (matching cache hit logic)
                int64_t delta = step_.next_delta_us();
                int64_t next_pts = video_frame->pts + delta;
                if (duration_us_ > 0 && next_pts >= duration_us_) {
                    // Don't advance beyond end of stream, let next iteration handle EOF
                } else {
                    current_time_us_.store(next_pts);
                }

                { // video callback dispatch (profiling removed to avoid variable shadow warning on MSVC)
                    std::vector<CallbackEntry<VideoFrameCallback>> copy;
                    { std::scoped_lock lk(callbacks_mutex_); copy = video_video_entries_; }
                    VE_DEBUG_ONLY(ve::log::info("Dispatching " + std::to_string(copy.size()) + " video callbacks (decode) for pts=" + std::to_string(video_frame->pts)));
                    for(auto &entry : copy) if(entry.fn) entry.fn(*video_frame);
                    VE_DEBUG_ONLY(ve::log::info("Dispatched video callbacks (decode) for pts=" + std::to_string(video_frame->pts)));
                }

                // High-performance frame timing for smooth playback
                if (!first_frame) {
                    // Use fixed frame duration based on detected FPS for consistent timing
                    int64_t frame_duration_us = frame_duration_guess_us();
                    auto target_interval = std::chrono::microseconds(frame_duration_us);
                    auto frame_end = std::chrono::steady_clock::now();
                    auto actual_duration = frame_end - last_frame_time;

                    // Balanced optimization for 4K 60fps content - targeting 80-83% performance
                    bool is_4k_60fps = (video_frame->width >= 3840 && video_frame->height >= 2160 && probed_fps_ >= 59.0);
                    
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

                first_frame = false;
                last_frame_time = std::chrono::steady_clock::now();

                // Update performance stats
                auto frame_time = std::chrono::duration<double, std::milli>(last_frame_time - frame_start);
                update_frame_stats(frame_time.count());

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
            
            // Read audio frame
            auto audio_frame = [&](){ VE_PROFILE_SCOPE_UNIQ("playback.decode_audio"); return decoder_->read_audio(); }();
            if (audio_frame) {
                { // audio callback dispatch (profiling removed to avoid variable shadow warning on MSVC)
                    std::vector<CallbackEntry<AudioFrameCallback>> copy;
                    { std::scoped_lock lk(callbacks_mutex_); copy = audio_entries_; }
                    if (copy.size() > 1) {
                        ve::log::warn("WARNING_MULTIPLE_AUDIO_SINKS: " + std::to_string(copy.size()) +
                                      " audio callbacks registered; this can cause echo.");
                    }
                    VE_DEBUG_ONLY(ve::log::info("Dispatching " + std::to_string(copy.size()) + " audio callbacks for pts=" + std::to_string(audio_frame->pts)));
                    for(auto &entry : copy) if(entry.fn) entry.fn(*audio_frame);
                    VE_DEBUG_ONLY(ve::log::info("Dispatched audio callbacks for pts=" + std::to_string(audio_frame->pts)));
                }
            }
            
            // Process timeline audio (mix multiple tracks)
            if (timeline_audio_manager_ && timeline_) {
                // Convert current time to TimePoint for timeline audio processing
                ve::TimePoint current_pos{current_time_us_.load(), 1000000};
                
                // Process timeline audio tracks at current position
                // This will mix audio from multiple timeline tracks and send to audio pipeline
                timeline_audio_manager_->process_timeline_audio(current_pos, 1024);  // Standard frame size
            }
        } // End single-media playback mode
            } // End has_content check
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
    }
    
    ve::log::debug("Playback thread ended");
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
        // Cache it
        ve::cache::CachedFrame cf; cf.width = frame->width; cf.height = frame->height; cf.data = frame->data;
        cf.format = frame->format; cf.color_space = frame->color_space; cf.color_range = frame->color_range;
        ve::cache::FrameKey putKey; putKey.pts_us = frame->pts; frame_cache_.put(putKey, std::move(cf));
        std::vector<CallbackEntry<VideoFrameCallback>> copy;
        { std::scoped_lock lk(callbacks_mutex_); copy = video_video_entries_; }
        for(auto &entry : copy) if(entry.fn) entry.fn(*frame);
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
    // Dynamic cache sizing based on available memory and content characteristics
    
    // Get system memory info (simplified - in real implementation would use proper system calls)
    constexpr size_t ASSUMED_AVAILABLE_RAM_GB = 8; // Conservative assumption
    constexpr size_t MAX_CACHE_MEMORY_MB = (ASSUMED_AVAILABLE_RAM_GB * 1024) / 4; // Use 25% of RAM
    
    // Estimate frame size based on common resolutions
    // This is calculated before media is loaded, so we use conservative estimates
    constexpr size_t FRAME_SIZE_4K = 3840 * 2160 * 4;    // 4K RGBA
    
    // Assume worst case (4K) for cache sizing
    constexpr size_t ESTIMATED_FRAME_SIZE = FRAME_SIZE_4K; // ~31MB per frame
    
    // Calculate max frames that fit in memory budget
    size_t max_frames_by_memory = (MAX_CACHE_MEMORY_MB * 1024 * 1024) / ESTIMATED_FRAME_SIZE;
    
    // Performance considerations:
    // - For 60fps, we want aggressive caching for smoothness (4-5 seconds = 240-300 frames)
    // - For 30fps, we can afford more (6 seconds = 180 frames)
    // - Minimum should be 60 frames (1-2 seconds)
    constexpr size_t MIN_CACHE_FRAMES = 60;
    constexpr size_t PREFERRED_CACHE_60FPS = 240;  // 4 seconds at 60fps for ultra-smooth playback
    constexpr size_t PREFERRED_CACHE_30FPS = 180;  // 6 seconds at 30fps
    
    // Choose cache size based on detected content - more aggressive for high FPS
    size_t target_cache_size = PREFERRED_CACHE_30FPS; // Default
    if (probed_fps_ >= 59.0) {
        target_cache_size = PREFERRED_CACHE_60FPS; // Larger cache for 60fps content
    }
    
    // Choose the smaller of memory limit or performance preference
    size_t optimal_size = (std::max)(MIN_CACHE_FRAMES, 
                                    (std::min)(max_frames_by_memory, target_cache_size));
    
    ve::log::info("Calculated optimal frame cache size: " + std::to_string(optimal_size) + 
                  " frames (max by memory: " + std::to_string(max_frames_by_memory) + ")");
    
    return optimal_size;
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
    
    // Clear ALL audio callbacks before any new registration to prevent echo
    {
        std::scoped_lock lk(callbacks_mutex_);
        audio_entries_.clear();
        ve::log::info("Cleared all existing audio callbacks to prevent echo");
    }
    
    // Add a small delay to ensure cleanup completes
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Create audio pipeline configuration
    ve::audio::AudioPipelineConfig pipeline_config;
    pipeline_config.sample_rate = 48000;
    pipeline_config.channel_count = 2;
    pipeline_config.buffer_size = 32;  // Minimal buffer to eliminate echo
    pipeline_config.format = ve::audio::SampleFormat::Float32;
    
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
    
    // Start audio output - TEMPORARILY DISABLED FOR DEBUGGING SIGABRT  
    // DEBUGGING: Temporarily commented out to test if audio causes SIGABRT
    /*
    if (!audio_pipeline_->start_output()) {
        ve::log::error("Failed to start audio output");
        audio_pipeline_.reset();
        return false;
    }
    */
    
    // Remove any existing audio callback to prevent duplicates
    if (audio_callback_id_ != 0) {
        remove_audio_callback(audio_callback_id_);
        ve::log::info("Removed existing audio callback to prevent echo");
    }
    
    // Ensure only one audio sink at a time for echo prevention
    {
        std::scoped_lock lk(callbacks_mutex_);
        audio_entries_.clear();  // Clear any existing callbacks
    }
    
    // Register audio callback to receive frames from decoder
    audio_callback_id_ = add_audio_callback([this](const decode::AudioFrame& frame) {
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
            
            // Create audio frame with the converted parameters
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
    
    // Initialize audio stats with actual values
    audio_stats_.sample_rate = pipeline_config.sample_rate;
    audio_stats_.channels = pipeline_config.channel_count;
    audio_stats_.buffer_size = pipeline_config.buffer_size;
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
                std::vector<CallbackEntry<VideoFrameCallback>> copy;
                {
                    std::scoped_lock lk(callbacks_mutex_);
                    copy = video_video_entries_;
                }
                
                for (const auto& entry : copy) {
                    entry.fn(*video_frame);
                }
                
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
