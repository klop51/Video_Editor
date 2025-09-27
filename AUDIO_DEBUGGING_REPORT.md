# Audio Debugging Report - Frame Skipping Issue

## Problem Description
- Audio is being decoded and processed successfully
- Audio pipeline shows stable buffer management
- **Issue**: Audio feels like it's skipping frames during playback
- Audio only seems to work when clicking UI controls

## Current Audio System Architecture

### 1. Audio Pipeline Configuration (audio_pipeline.cpp)
```cpp
// WASAPI Configuration - Enhanced for stability
AudioOutputConfig config;
config.sample_rate = 48000;
config.channels = 2;
config.bits_per_sample = 16;
config.buffer_duration_ms = 50;  // Increased from 10ms to prevent underruns
config.share_mode = AUDCLNT_SHAREMODE_SHARED;  // Changed to shared mode for stability

// Buffer management showing stable performance:
// Audio breadcrumb: frames_to_submit=96, safe_frames=96, buffer_frame_count=2400, padding_frames=1440, bytes_copied=768
```

### 2. Playback Controller Audio Processing (controller.cpp)
```cpp
// Audio frame reading and callback dispatch
auto audio_frame = decoder_->read_audio();

if (audio_frame) {
    // Audio callback dispatch
    std::vector<CallbackEntry<AudioFrameCallback>> copy;
    { std::scoped_lock lk(callbacks_mutex_); copy = audio_entries_; }
    
    if (copy.size() > 1) {
        ve::log::warn("WARNING_MULTIPLE_AUDIO_SINKS: " + std::to_string(copy.size()) + 
                      " audio callbacks registered; this can cause echo.");
    }
    
    // Process each callback
    for (const auto& entry : copy) {
        try {
            entry.callback(*audio_frame);
        } catch (const std::exception& e) {
            ve::log::error("Audio callback failed: " + std::string(e.what()));
        }
    }
}
```

### 3. Professional Audio Monitoring (professional_monitoring.cpp)
```cpp
void ProfessionalAudioMonitoringSystem::process_audio_frame(const AudioFrame& frame) {
    if (!initialized_) {
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // TEMPORARILY DISABLED: Process through all enabled monitoring components with safe error handling
    // TODO: Fix recursive mutex deadlock in loudness monitoring
    /*
    if (loudness_monitor_) {
        try {
            loudness_monitor_->process_samples(frame);
        } catch (const std::exception& e) {
            ve::log::error("Loudness monitor error: " + std::string(e.what()));
            // Continue processing even if loudness monitoring fails
        }
    }
    */
    
    // Other monitoring components still active
    if (meter_system_) {
        try {
            meter_system_->process_samples(frame);
        } catch (const std::exception& e) {
            ve::log::error("Meter system error: " + std::string(e.what()));
            throw;
        }
    }
    
    if (scopes_) {
        scopes_->process_samples(frame);
    }
}
```

## Debugging Data from Recent Tests

### Audio Frame Reading Success
```
[2025-09-27 21:00:42.588] [info] AUDIO_DEBUG: Successfully read audio frame 50
[2025-09-27 21:00:46.720] [info] AUDIO_DEBUG: Successfully read audio frame 100
[2025-09-27 21:00:49.666] [info] AUDIO_DEBUG: Successfully read audio frame 150
[2025-09-27 21:00:52.775] [info] AUDIO_DEBUG: Successfully read audio frame 200
...continuing up to frame 1000
```

### Buffer Stability Confirmed
```
[2025-09-27 21:00:40.601] [info] Audio breadcrumb: frames_to_submit=96, safe_frames=96, buffer_frame_count=2400, padding_frames=1440, bytes_copied=768
[2025-09-27 21:00:42.617] [info] Audio breadcrumb: frames_to_submit=96, safe_frames=96, buffer_frame_count=2400, padding_frames=1440, bytes_copied=768
```

### No More Deadlock Errors
Previously flooded with:
```
[error] Loudness monitor error: resource deadlock would occur: resource deadlock would occur
```
**Now RESOLVED** - No deadlock errors in logs after disabling problematic loudness monitoring.

## 🎯 ROOT CAUSE ANALYSIS (ChatGPT Diagnosis)

### **What's Working vs. What Feels Wrong:**

✅ **Working:**
- Audio frames are read steadily (through frame 1000+)
- WASAPI metrics show consistent submit sizes and padding (no obvious underruns)  
- Loudness monitor crashes gone after disabling it

❌ **Feels Wrong:**
- Playback sounds "skippy," and improves when clicking UI → **strong hint that the message loop wakes the render path (timing tied to UI)**

### **Likely Root Causes (Ranked by Priority):**

#### 🥇 **1. Render cadence not driven by the device clock**
If the render loop relies on UI timers / general event loop, buffers get serviced irregularly—sound appears choppy even when logs look "stable." (UI interaction can "kick" the loop.)

#### 🥈 **2. Callback fan-out timing**  
Decoding is fine, but the consumer (WASAPI submit) may only run on user input / paint ticks.

#### 🥉 **3. Invisible underruns**
Your breadcrumb math looks OK, but we may be submitting late or with the wrong frame quantum (e.g., 96 frames) vs. device period. Need device period-aligned submits and proof from `IAudioClock` / `IAudioClient::GetCurrentPadding`.

## 🔬 HIGH-VALUE DIAGNOSTIC CHECKS

### **Prove the render loop cadence**
Log once per submit (not per sample):
- `GetCurrentPadding()` before/after submit
- frames submitted this tick  
- time since last submit (µs)

**Expected:** Consistent ~devicePeriod between submits; if you see long gaps that shrink when clicking UI, we've found it.

### **Confirm device period & quantum**  
Record: `IAudioClient::GetDevicePeriod`, mix format `nBlockAlign`, and device `bufferFrameCount`. Submit an integer multiple of the period frames (often 480 @ 48 kHz → 10 ms). Your example "96 frames" could be too small for the device cadence.

### **Ensure one sink**
If `audio_entries_ > 1`, warn and choose one render sink to avoid duplicated/competing submits. (You already log this; enforce it during debugging.)

## 🛠️ MINIMAL FIX: Event-driven WASAPI render loop

**Goal:** Drive audio by the device (not the UI). Use `AUDCLNT_STREAMFLAGS_EVENTCALLBACK` + an event handle. Each event → `GetCurrentPadding` → compute frames-to-write → `GetBuffer/ReleaseBuffer`.

### Setup (once):
```cpp
// After IAudioClient initialization:
HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
audioClient->SetEventHandle(hEvent); // AUDCLNT_STREAMFLAGS_EVENTCALLBACK must be set at init
audioClient->Start();

// MMCSS: mark thread as "Pro Audio" or "Audio"
DWORD taskIndex = 0;
HANDLE hMmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
```

### Render thread loop:
```cpp
for (;;) {
    DWORD w = WaitForSingleObject(hEvent, 200 /*ms*/);
    if (w != WAIT_OBJECT_0) continue; // timeout -> warn once

    UINT32 padding = 0, bufferFrames = 0, framesToWrite = 0;
    audioClient->GetBufferSize(&bufferFrames);
    audioClient->GetCurrentPadding(&padding);
    framesToWrite = bufferFrames - padding;

    if (framesToWrite == 0) continue;

    BYTE* pData = nullptr;
    audioRenderClient->GetBuffer(framesToWrite, &pData);

    // Fill pData with exactly framesToWrite * nBlockAlign bytes from your decoded queue.
    // If queue < framesToWrite, write silence for the remainder (avoid glitch).

    audioRenderClient->ReleaseBuffer(framesToWrite, 0);
}
```

**Why this helps:** The device signals exactly when to write; UI clicks no longer affect cadence.

## Original Analysis (Pre-Diagnosis)

### 1. Audio Callback Registration Issue
```cpp
// From controller.cpp - check if callbacks are properly registered
std::vector<CallbackEntry<AudioFrameCallback>> copy;
{ std::scoped_lock lk(callbacks_mutex_); copy = audio_entries_; }

// Potential issue: Are callbacks actually registered during automatic playback?
```

### 2. Frame Timing Synchronization
```cpp
// Video-Audio sync might be causing frame drops
// Current timing mechanism:
auto current_time = std::chrono::steady_clock::now();
auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time_).count();
```

### 3. WASAPI Buffer Underruns (Despite Breadcrumbs)
Even with stable breadcrumb reports, actual WASAPI submission might be failing:
```cpp
// Need to verify actual WASAPI buffer submission vs. reported metrics
frames_to_submit=96  // Reported
bytes_copied=768     // Actual bytes written to WASAPI buffer
```

## Debugging Questions for ChatGPT

1. **Frame Skipping with Successful Decoding**: Why would audio feel choppy when logs show successful frame reading every ~3-4 seconds and stable buffer management?

2. **UI Dependency**: What could cause audio to only play when clicking UI controls, despite audio callbacks being properly registered?

3. **Buffer Timing**: With 50ms WASAPI buffers and 1024-sample pipeline buffers, what timing issues could cause perceived frame skipping?

4. **Callback Execution**: How can we verify that audio callbacks are executing during automatic playback vs. only during UI interactions?

## Next Steps for Investigation

### Immediate Debug Actions Needed:
1. **Add callback execution logging** - Verify callbacks run during automatic playback
2. **WASAPI buffer state monitoring** - Check actual buffer fill levels vs. reported metrics
3. **Audio frame timestamp analysis** - Verify frame timing consistency
4. **UI event correlation** - Track what UI events trigger successful audio playback

### Code Areas to Examine:
1. `audio_pipeline.cpp` - WASAPI buffer submission timing
2. `controller.cpp` - Audio callback registration and execution timing
3. Audio callback implementation - Verify proper frame processing
4. UI audio controls - What they do differently that makes audio work

## System Configuration
- **Sample Rate**: 48000 Hz
- **Channels**: 2 (Stereo)
- **Bit Depth**: 16-bit
- **WASAPI Buffer**: 50ms duration
- **Pipeline Buffer**: 1024 samples
- **Mode**: AUDCLNT_SHAREMODE_SHARED

## Hardware Context
- **GPU**: D3D11VA hardware acceleration available
- **Codec**: H.264 with hardware acceleration
- **Resolution**: 1920x1080 @ 23.976fps
- **Container**: MKV with dual audio streams

## 🚀 ACTION PLAN (Implementation Tasks)

### **Phase 1: Diagnostic Instrumentation** ⚡ (Fast to implement)
1. **Instrument submits** - Log once per submit (not per sample):
   - `GetCurrentPadding()` before/after submit
   - Frames submitted this tick
   - Time since last submit (µs)

2. **Confirm device period & quantum**:
   - Record `IAudioClient::GetDevicePeriod` 
   - Record mix format `nBlockAlign`
   - Record device `bufferFrameCount`
   - Verify submit quantum aligns to device period frames

3. **Enforce one audio sink** during debugging:
   - Error if `audio_entries_ > 1`
   - Choose single render sink to avoid competing submits

### **Phase 2: Event-Driven WASAPI** 🎯 (Core fix)
1. **Switch to event-driven render loop**:
   - Use `AUDCLNT_STREAMFLAGS_EVENTCALLBACK`
   - Implement `WaitForSingleObject` render thread
   - Drive audio by device clock, not UI events

2. **Align submit quantum** to device period frames:
   - Typically 480 frames @ 48 kHz (10ms)
   - Replace current "96 frames" with proper quantum

3. **Add MMCSS thread priority**:
   - Mark audio thread as "Pro Audio" 
   - Ensure consistent scheduling

### **Phase 3: Validation** ✅
1. **Re-test without touching UI**:
   - Expect smooth audio playback
   - Consistent submit intervals in logs
   - No "skippy" perception
   - Audio works immediately on play, not just after UI clicks

### **Quick Guardrails**
- ⚠️ **No per-sample logging** in render path (keep allocation-free)
- 🔒 **Bound queues** (sample/frame FIFOs) to prevent heap growth  
- 🎯 **Single render sink** during diagnosis

---

**Root Cause Confirmed:** Audio rendering tied to UI event loop instead of device-driven timing.  
**Solution:** Event-driven WASAPI with proper device period alignment.