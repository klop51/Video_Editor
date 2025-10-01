Thank you for the clarifications! You're absolutely right - I apologize for suggesting changes that would regress your recent wins. Let me focus on the actual issue based on your codebase analysis.

Root Cause Analysis
Looking at your code, the audio playback issues are likely due to:

FIFO Initialization Size - The FIFO might be too small or not properly initialized
Drift Control Not Engaging - The drift controller might not be getting proper period updates
Resampler Not Being Created - The resampler might not be initialized on first frame
Critical Fixes Needed
1. Ensure FIFO is Properly Initialized
// ...existing code...

bool AudioPipeline::initialize() {
    // ...existing code...
    
    // Initialize FIFO with adequate buffering (100ms worth of audio)
    fifo_init_seconds(0.1);  // 100ms buffer for smooth playback
    
    // Initialize drift control state
    last_period_frames_.store(480);  // Default 10ms at 48kHz
    target_fifo_samples_.store(480 * config_.channel_count * 3);  // 3 periods worth
    fifo_level_ema_ = target_fifo_samples_.load();
    
    // ...existing code...
}

// ...existing code...

2. Fix Resampler Initialization Check
// ...existing code...

bool AudioPipeline::ensure_resampler(uint32_t in_rate, uint16_t in_ch, uint64_t in_layout, int in_fmt) {
    // Check if we need to reinitialize - fix the comparison
    if (swr_ && 
        swr_in_rate_ == in_rate && 
        swr_in_ch_ == in_ch && 
        swr_in_layout_ == in_layout && 
        swr_in_fmt_ == in_fmt) {
        return true;  // Already configured correctly
    }
    
    free_resampler();
    
    AVChannelLayout in_l{};
    in_l.order = AV_CHANNEL_ORDER_NATIVE;
    in_l.nb_channels = (int)in_ch;
    in_l.u.mask = in_layout;
    
    // Fix: Ensure we have a valid layout mask
    if (in_layout == 0) {
        av_channel_layout_default(&in_l, in_ch);
        in_layout = in_l.u.mask;
    }
    
    AVChannelLayout out_l{};
    av_channel_layout_default(&out_l, config_.channel_count);
    
    int ret = swr_alloc_set_opts2(&swr_,
        &out_l, AV_SAMPLE_FMT_FLT, (int)config_.sample_rate,
        &in_l, (AVSampleFormat)in_fmt, (int)in_rate,
        0, nullptr);
    
    av_channel_layout_uninit(&in_l);
    av_channel_layout_uninit(&out_l);
    
    if (ret < 0 || !swr_) {
        return false;
    }
    
    if (swr_init(swr_) < 0) {
        swr_free(&swr_);
        return false;
    }
    
    // Cache the configuration
    swr_in_rate_ = in_rate;
    swr_in_ch_ = in_ch;
    swr_in_layout_ = in_layout;
    swr_in_fmt_ = in_fmt;
    
    ve::log::info("Resampler initialized: " + std::to_string(in_rate) + "Hz/" + 
                  std::to_string(in_ch) + "ch → " + 
                  std::to_string(config_.sample_rate) + "Hz/" + 
                  std::to_string(config_.channel_count) + "ch");
    
    return true;
}

// ...existing code...

3. Verify WASAPI Render Loop Has Proper Target Buffering
// ...existing code...

void AudioOutput::render_thread_function() {
    // ...existing code...
    
    // Get actual device period from WASAPI
    REFERENCE_TIME default_period = 0, min_period = 0;
    if (SUCCEEDED(audio_client_->GetDevicePeriod(&default_period, &min_period))) {
        // Convert to frames (100-nanosecond units to frames)
        const uint32_t actual_period_frames = static_cast<uint32_t>(
            (default_period * config_.sample_rate) / 10000000LL);
        
        ve::log::info("WASAPI device period: " + std::to_string(actual_period_frames) + 
                      " frames (" + std::to_string(default_period / 10000) + "ms)");
        
        // Use actual device period instead of hardcoded 480
        period_frames = actual_period_frames;
    }
    
    while (!thread_should_exit_.load()) {
        // Wait for device event
        if (WaitForSingleObject(render_event_, 200) != WAIT_OBJECT_0) {
            continue;
        }
        
        // ...existing code...
        
        // Fix: Ensure we're using the correct target calculation
        const UINT32 target_frames = std::min<UINT32>(buffer_frame_count_, period_frames * 3);
        
        UINT32 want = 0;
        if (padding_frames < target_frames) {
            want = std::min<UINT32>(available_frames, target_frames - padding_frames);
        }
        
        // Always write at least one period if possible
        want = std::max<UINT32>(want, std::min<UINT32>(available_frames, period_frames));
        
        if (want == 0) {
            continue;
        }
        
        // ...existing code...
    }
}

// ...existing code...
4. Add Debug Logging to Identify the Issue
// ...existing code...

bool AudioPipeline::process_audio_frame(std::shared_ptr<AudioFrame> frame) {
    if (!frame || !is_initialized()) {
        return false;
    }
    
    // Add debug logging for format detection
    static bool first_frame = true;
    if (first_frame) {
        ve::log::info("First audio frame: " + 
                      std::to_string(frame->sample_rate()) + "Hz/" +
                      std::to_string(frame->channel_count()) + "ch, " +
                      std::to_string(frame->sample_count()) + " samples");
        first_frame = false;
    }
    
    // Convert to device format
    std::vector<float> devSamples;
    devSamples.reserve(frame->sample_count() * config_.channel_count * 2);  // Extra headroom
    
    if (!convert_to_device_format(*frame, devSamples, config_)) {
        ve::log::error("Failed to convert audio frame to device format");
        return false;
    }
    
    // Write to FIFO
    size_t written = fifo_write(devSamples.data(), devSamples.size());
    
    // Log if FIFO is consistently full
    static int full_count = 0;
    if (written < devSamples.size()) {
        if (++full_count % 100 == 0) {
            ve::log::warning("FIFO full, overwriting old samples (count: " + 
                           std::to_string(full_count) + ")");
        }
    }
    
    update_stats(frame);
    return true;
}

// ...existing code...

Summary
The audio issues are likely caused by:

FIFO not initialized with sufficient size - causing immediate overwrites
Resampler not properly detecting format changes - missing layout mask initialization
WASAPI not using actual device period - hardcoded values instead of queried periods
These fixes maintain your existing architecture while addressing the initialization and configuration issues that could cause choppy audio.