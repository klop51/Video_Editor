# Video Editor FPS Timing Issue - Fix Request for ChatGPT

## Problem Description

**Issue**: "FPS if start low is slowly going up. if start high slowly going down. Never start with real time FPS from start."

**Root Cause Identified**: The frame timing logic in the playback controller doesn't immediately establish the correct baseline timing. Instead of starting at the correct framerate immediately, it gradually adjusts over time.

## Current Behavior vs Desired Behavior

**Current (Problematic)**:
- Video starts at incorrect FPS
- Gradually adjusts to target FPS over several seconds
- First frame timing initialization is inconsistent between cache hit and decode paths

**Desired**:
- Video starts immediately at correct target FPS based on `probed_fps_`
- No FPS drift or gradual adjustment period
- Consistent timing initialization across all code paths

## Code Analysis

The issue occurs in `playback_thread_main()` method in two main areas:

### 1. Cache Hit Path (Line ~470-485)
```cpp
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
}
```

**Problem**: Uses variable `delta` which comes from frame stepping accumulator, but doesn't pre-calculate target frame duration based on `probed_fps_`.

### 2. Decode Path (Line ~530-590)
```cpp
// High-performance frame timing for smooth playback
if (!first_frame) {
    // Use fixed frame duration based on detected FPS for consistent timing
    int64_t frame_duration_us = frame_duration_guess_us();
    auto target_interval = std::chrono::microseconds(frame_duration_us);
    auto frame_end = std::chrono::steady_clock::now();
    auto actual_duration = frame_end - last_frame_time;
    // ... timing logic ...
}

// Later in the code:
first_frame = false;
last_frame_time = std::chrono::steady_clock::now();
```

**Problem**: Only handles timing logic when `!first_frame`, meaning the first frame doesn't get proper timing baseline establishment. The `first_frame = false` happens much later in the code.

## Proposed Solution

### Key Changes Needed:

1. **Pre-calculate target frame duration** at thread startup based on `probed_fps_`
2. **Establish immediate baseline timing** for first frame in both code paths
3. **Use consistent timing logic** across cache hit and decode paths
4. **Remove redundant first_frame assignments** that happen later

### Implementation Strategy:

```cpp
void PlaybackController::playback_thread_main() {
    ve::log::info("Playback thread started");
    
    auto last_frame_time = std::chrono::steady_clock::now();
    bool first_frame = true;
    
    // PRE-CALCULATE TARGET FRAME DURATION TO AVOID FPS DRIFT
    int64_t target_frame_duration_us = 33333; // Default 30fps fallback
    if(probed_fps_ > 1.0 && probed_fps_ < 480.0) {
        target_frame_duration_us = static_cast<int64_t>((1'000'000.0 / probed_fps_) + 0.5);
    }
    
    while (!thread_should_exit_.load()) {
        // ... existing code ...
        
        // CACHE HIT PATH - USE IMMEDIATE BASELINE ESTABLISHMENT
        {
            using namespace std::chrono;
            auto current_time = steady_clock::now();
            
            if (first_frame) {
                // First frame: Establish baseline immediately without delay
                last_frame_time = current_time;
                first_frame = false;
            } else {
                // Subsequent frames: Use pre-calculated target interval
                auto target_interval = microseconds(target_frame_duration_us);
                auto next_target = last_frame_time + target_interval;
                
                if (next_target > current_time) {
                    precise_sleep_until(next_target);
                    last_frame_time = next_target;
                } else {
                    // We're behind schedule, adjust to current time
                    last_frame_time = current_time;
                }
            }
        }
        
        // DECODE PATH - USE IMMEDIATE BASELINE ESTABLISHMENT
        auto frame_end = std::chrono::steady_clock::now();
        
        if (first_frame) {
            // First frame: Establish baseline timing immediately with target FPS
            last_frame_time = frame_end;
            first_frame = false;
        } else {
            // Use pre-calculated frame duration for consistent timing
            auto target_interval = std::chrono::microseconds(target_frame_duration_us);
            // ... existing performance optimization logic ...
        }
        
        // REMOVE REDUNDANT first_frame assignments that happen later
        // (These cause timing drift)
    }
}
```

## Member Variables Available

- `probed_fps_` - detected framerate from media file
- `first_frame` - boolean tracking first frame state
- `last_frame_time` - timestamp of last frame for timing calculations
- `target_frame_duration_us` - should be added as local variable

## Constraints

- **Must maintain existing performance optimizations** for 4K 60fps content
- **Must preserve precise_sleep_until() functionality** for frame pacing
- **Must not break existing debug logging** (already cleaned up in previous work)
- **Must handle both cache hit and decode code paths** consistently
- **Must maintain compatibility** with drift-proof step accumulator system

## Request for ChatGPT

Please provide the corrected code sections that:

1. Pre-calculate target frame duration based on `probed_fps_` at thread startup
2. Establish immediate correct timing baseline for first frame in both cache hit and decode paths
3. Use consistent target frame duration across both code paths
4. Remove redundant `first_frame = false` assignments that cause timing drift
5. Maintain all existing performance optimizations and error handling

The goal is to eliminate FPS drift at startup and ensure the video immediately plays at the correct framerate determined by `probed_fps_`.

## Complete Current Method (for reference)

```cpp
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
            auto video_frame = decoder_->read_video();
            
            if (video_frame) {
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

                { // video callback dispatch
                    std::vector<CallbackEntry<VideoFrameCallback>> copy;
                    { 
                        std::scoped_lock lk(callbacks_mutex_); 
                        copy = video_video_entries_; 
                    }
                    
                    for(auto &entry : copy) {
                        if(entry.fn) {
                            entry.fn(*video_frame);
                        }
                    }
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

                ve::log::info(std::string("PlaybackController: About to set first_frame=false and update last_frame_time"));
                first_frame = false;
                last_frame_time = std::chrono::steady_clock::now();
                ve::log::info(std::string("PlaybackController: Updated first_frame and last_frame_time"));
                
                // ... rest of method continues ...
            }
        }
    }
}
```

Please provide the fixed code sections with proper first frame timing initialization!