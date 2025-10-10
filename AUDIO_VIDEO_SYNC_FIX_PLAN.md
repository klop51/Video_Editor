# A/V Synchronization and FPS Fix Implementation Plan

## Current State (October 8, 2025)
- ✅ Video plays continuously without stopping (audio restrictions removed)
- ✅ Audio pipeline runs independently
- ❌ No A/V sync - video and audio drift apart over time
- ❌ FPS readings unrealistic (up to 5000 fps) due to unpaced frame presentation
- ❌ Frame dropping logic removed, causing potential desync

## Root Cause Analysis

### A/V Sync Issue
The original code had aggressive frame dropping when audio led video by >120ms, causing perceived "video stopping." The fix was to remove all audio restrictions, but this broke proper sync. We need adaptive sync that:
- Allows reasonable audio lead (500ms+ for editing workflows)
- Uses device clock for timing when audio is stable
- Falls back to wall-clock when audio is unstable

### FPS Bug
Without audio pacing, video frames present as fast as possible, inflating FPS calculations. This happened when `max_drop_ahead_ms_` was changed, removing wait logic. FPS should never exceed content frame rate.

## Proposed Fix: Adaptive A/V Sync with Proper Pacing

### 1. Adaptive A/V Sync Thresholds
- Reintroduce frame dropping but scale thresholds based on content frame rate
- Use ~2.5 frame tolerance for drop-ahead, ~1.25 frames for wait-behind
- Example: 60fps = 42ms drop threshold, 30fps = 83ms
- Keep wait-behind limit low (~25ms) to prevent audio doubling

### 2. Device Clock Integration
- Use `audio_pipeline_->device_playhead_seconds()` for sync when audio is ready
- Learn A/V offset once at startup using first video/audio PTS
- Predict presentation time: `video_pts + offset + latency`

### 3. Buffer-Aware Sync Decisions
- Only apply A/V sync when audio buffer >65% full
- During underrun recovery, use wall-clock pacing only
- Prevent inappropriate relocking during thin FIFO conditions

### 4. Startup Grace Period
- For first 250ms after audio ready, relax drop thresholds by +80ms
- Allows initial sync learning without aggressive frame dropping

### 5. FPS Calculation Fix
- Ensure pacing respects frame duration even without audio
- Use wall-clock timing for consistent FPS reporting
- Prevent frame presentation faster than content frame rate
- Implement proper frame duration guessing from content

## Implementation Steps

### Phase 1: Core Infrastructure
1. Add `compute_av_thresholds()` method to PlaybackController
2. Add adaptive threshold variables: `max_drop_ahead_ms_`, `max_wait_behind_ms_`
3. Add A/V mapping learning: `av_map_offset_sec_`, `av_map_learned_`
4. Add startup grace period tracking

### Phase 2: Sync Logic Restoration
1. Restore controlled frame dropping in `present_due_frames` lambda
2. Re-enable A/V mapping learning with buffer health checks
3. Implement device clock vs wall-clock pacing decisions
4. Add startup grace period logic

### Phase 3: FPS Fix
1. Ensure wall-clock fallback respects frame duration
2. Fix `pace_next_continuous` to prevent over-presentation
3. Add frame duration validation in FPS calculations
4. Implement content-aware pacing limits

### Phase 4: Testing & Validation
1. Test with 30fps and 60fps content
2. Verify audio lead stays within reasonable bounds (<500ms)
3. Confirm realistic FPS readings (24-120 fps)
4. Test device changes and underrun recovery

## Key Code Changes

### In `controller.cpp`:

#### Add new member variables:
```cpp
double max_drop_ahead_ms_ = 120.0;
double max_wait_behind_ms_ = 50.0;
std::atomic<double> av_map_offset_sec_{0.0};
std::atomic<bool> av_map_learned_{false};
std::chrono::steady_clock::time_point prebuffer_start_time_;
```

#### Add threshold computation:
```cpp
void PlaybackController::compute_av_thresholds(double fps, double audio_latency_sec) {
    const double frame_ms = 1000.0 / std::clamp(fps, 24.0, 120.0);
    const double audio_lat = audio_latency_sec * 1000.0;
    max_drop_ahead_ms_  = std::clamp(2.5 * frame_ms + 0.5 * audio_lat, 40.0, 120.0);
    max_wait_behind_ms_ = std::clamp(1.25 * frame_ms + 0.25 * audio_lat, 20.0, 80.0);
    ve::log::info("A/V thresholds computed: drop=" + std::to_string(max_drop_ahead_ms_) +
                  "ms, wait=" + std::to_string(max_wait_behind_ms_) + "ms for " + std::to_string(fps) + "fps");
}
```

#### Add A/V mapping learning:
```cpp
void PlaybackController::learn_av_map_once(double frame_pts_sec) {
    if (av_map_learned_.load(std::memory_order_acquire) || !audio_pipeline_)
        return;

    double device_head = audio_pipeline_->device_playhead_seconds();
    if (device_head < 0.0 || !std::isfinite(device_head))
        return;

    const auto unset = std::numeric_limits<int64_t>::min();
    const int64_t v0_us = first_video_pts_us_.load(std::memory_order_acquire);
    if (v0_us == unset) return;

    const double v0 = static_cast<double>(v0_us) / 1'000'000.0;
    const double video_norm = frame_pts_sec - v0;
    double B = device_head - video_norm;
    av_map_offset_sec_.store(std::clamp(B, -30.0, 30.0), std::memory_order_release);
    av_map_learned_.store(true, std::memory_order_release);
    ve::log::info("A/V map learned: offset=" + std::to_string(B * 1000.0) + "ms");
}
```

#### Modify `present_due_frames` lambda:
```cpp
auto present_due_frames = [&](bool force_present) {
    // Check if audio is stable for sync decisions
    const bool audio_ok = audio_pipeline_ &&
                         audio_pipeline_->audio_ready() &&
                         !audio_pipeline_->is_recovering_from_underrun() &&
                         audio_pipeline_->buffer_fill_percent() >= 65.0;

    // Startup grace period
    const bool in_startup_grace = av_map_learned_.load() &&
                                  (std::chrono::steady_clock::now() - prebuffer_start_time_) < std::chrono::milliseconds(250);

    double drop_limit = in_startup_grace ? (max_drop_ahead_ms_ + 80.0) : max_drop_ahead_ms_;
    double wait_limit = in_startup_grace ? (max_wait_behind_ms_ + 30.0) : max_wait_behind_ms_;

    // Learn A/V mapping when audio is stable
    if (audio_ok) {
        learn_av_map_once(frame.pts_sec);
    }

    // Calculate timing
    double target_present_sec = 0.0;
    if (audio_ok && av_map_learned_.load()) {
        // Use device clock for sync
        double device_head_now = audio_pipeline_->device_playhead_seconds();
        target_present_sec = predict_present_time_on_audio_clock(frame.pts_sec, device_head_now);
    } else {
        // Wall-clock pacing when audio unstable
        target_present_sec = wall_clock_pts_origin_sec_ + frame.pts_sec - wall_clock_pts_origin_sec_;
    }

    auto now = std::chrono::steady_clock::now();
    double now_sec = std::chrono::duration<double>(now - wall_clock_play_origin_).count();
    double lead_ms = (target_present_sec - now_sec) * 1000.0;

    // Frame dropping logic
    if (!force_present && lead_ms < -drop_limit) {
        stats_.frames_dropped++;
        ve::log::debug("Dropped frame: audio lead " + std::to_string(-lead_ms) + "ms > " + std::to_string(drop_limit) + "ms");
        return;
    }

    // Frame waiting logic (prevent audio doubling)
    if (!force_present && lead_ms > wait_limit) {
        // Wait but don't block indefinitely
        double wait_ms = std::min(lead_ms - wait_limit, 50.0); // Cap wait time
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(wait_ms)));
        now_sec += wait_ms / 1000.0;
    }

    // Present frame
    deliver_video_frame_now(std::move(frame));
    update_frame_stats(std::chrono::duration<double>(std::chrono::steady_clock::now() - now).count() * 1000.0);
};
```

## Testing Validation

### A/V Sync Tests
- Audio lead should stay <500ms for editing workflows
- Video continues during audio instability
- Smooth recovery after device changes

### FPS Tests
- Realistic values (24-120 fps) matching content
- No inflation during audio instability
- Consistent reporting across different content rates

### Edge Cases
- Audio device changes during playback
- Buffer underruns and recovery
- Different frame rates (24, 30, 60 fps)
- Startup synchronization

## Success Criteria
- ✅ Video plays continuously without freezing
- ✅ A/V sync maintained within reasonable bounds
- ✅ Realistic FPS readings
- ✅ Smooth audio device recovery
- ✅ No performance degradation

## Rollback Plan
If issues arise, can revert to current state by:
1. Removing A/V sync logic from `present_due_frames`
2. Keeping wall-clock pacing only
3. Disabling frame dropping and waiting

## Notes
- Prioritize continuous video display over strict sync for editing workflows
- Maximum audio lead: 500ms acceptable for editing
- CPU fallback available if GPU timing unavailable
- All changes maintain backward compatibility</content>
<parameter name="filePath">c:\Users\braul\Desktop\Video_Editor\AUDIO_VIDEO_SYNC_FIX_PLAN.md