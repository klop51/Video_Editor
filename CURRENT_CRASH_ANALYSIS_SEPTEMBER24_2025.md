# Current Crash Analysis - September 24, 2025
## Post-Qt Signal-Slot Implementation Testing

### 🎯 Test Results Summary
**Status**: SIGABRT crash confirmed after 3-4 seconds of video playback  
**Architecture Discovery**: Original crash analysis assumptions were incorrect  
**Root Cause**: Different than initially analyzed - not orphaned Qt signals  

### 📊 Key Findings

#### ✅ What Works
- Application launches successfully
- Video decoding operates correctly (`DEBUG: read_video()` messages)
- Frame processing works (multiple 1080x1920 frames processed)
- UI rendering system functions normally (Qt painting messages)
- Our Qt signal-slot implementation compiles and loads without issues

#### ❌ What's Broken
- **SIGABRT crash after ~3-4 seconds** (consistent with previous behavior)
- **QtPlaybackController connection fails** - No debug messages from our implementation
- **Architecture mismatch** - System uses `ve::playback::PlaybackController` callbacks, not Qt signals

### 🔍 Critical Discovery

**The crash analysis report assumed the wrong architecture:**

```cpp
// ANALYSIS ASSUMED (but incorrect):
QtPlaybackController* -> emit uiFrameReady() -> NO RECEIVER -> SIGABRT

// ACTUAL SYSTEM:
ve::playback::PlaybackController* -> FrameCallback lambda -> CRASH IN CALLBACK SYSTEM
```

**Evidence:**
1. **No Qt signal debug messages** - Our `QtPlaybackController` debug logging never appears
2. **dynamic_cast fails** - `dynamic_cast<ve::decode::QtPlaybackController*>(controller)` returns nullptr
3. **Callback system active** - ViewerPanel uses `controller->add_video_callback()` (line 72 in viewer_panel.cpp)

### 📝 Actual System Architecture

```cpp
// MainWindow creates:
playback_controller_ = new ve::playback::PlaybackController();  // NOT QtPlaybackController

// ViewerPanel connects via:
controller->add_video_callback([this](const ve::decode::VideoFrame& f) {
    // Callback executes in decoder thread
    // QTimer::singleShot(0, ...) defers to UI thread
    // CRASH occurs somewhere in this callback chain
});
```

### 🚨 Immediate Issues

1. **Wrong fix applied** - We fixed QtPlaybackController signal emissions, but that system isn't used
2. **Real crash cause unknown** - Need to investigate callback system threading/memory issues
3. **Mixed architecture** - QtPlaybackController exists but isn't connected to the main application flow

### 📋 Next Steps Required

1. **Investigate callback system crash** - The real crash is in `ve::playback::PlaybackController` callback execution
2. **Add callback debug logging** - Trace the actual execution path causing SIGABRT
3. **Consider architecture decision** - Should we switch to QtPlaybackController or fix callback system?

### 🔬 Crash Timeline (from log output)

```
19:59:02.849 - Application starts, video decoding begins
19:59:02.878 - First frame processed successfully
19:59:02.913 - Second frame processed
19:59:02.958 - Third frame processed 
19:59:02.996 - Fourth frame processed
[CRASH] SIGABRT - Consistent ~3-4 second timing
```

**Pattern**: Crash occurs after consistent number of frame processing cycles, suggesting memory corruption or resource exhaustion in callback system.

### 📄 Implementation Status

- ✅ Qt slot declaration added to ViewerPanel.hpp
- ✅ Qt slot implementation with thread safety guards
- ✅ Qt signal-slot connection with proper flags
- ✅ Debug logging for Qt signal emissions
- ❌ **ARCHITECTURE MISMATCH** - QtPlaybackController not used by application
- ❌ Real crash cause unaddressed

### 🎯 Recommendation

**Immediate Priority**: Investigate the actual `ve::playback::PlaybackController` callback system crash:

1. Add debug logging to the callback lambda in ViewerPanel::set_playback_controller()
2. Trace memory/threading issues in the existing callback system
3. Consider if callback->Qt signal bridge is needed vs. fixing callback system directly

The Qt signal-slot implementation we built is architecturally sound but addresses the wrong subsystem.