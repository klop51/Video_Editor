# Qt6 Video Editor - UI Responsiveness Solution

A modern Qt6-based video editor demonstrating comprehensive solutions for UI responsiveness issues during media processing operations.

## 🚀 Key Features

- **Responsive UI**: No freezing during media import/processing
- **Background Processing**: Worker threads with chunked operations
- **Optimized Rendering**: Fast timeline paint events with caching
- **Asynchronous GPU Upload**: Optional double-buffered OpenGL PBO path (toggle with `-DVE_ENABLE_PBO_UPLOAD=ON/OFF`, default ON)
 - **Asynchronous GPU Upload**: Optional OpenGL PBO path (double or triple buffered) with runtime disable + persistent map experiment
- **Modern Qt6**: Built with latest Qt6 framework via vcpkg
- **Cross-Platform**: CMake build system (Windows focus)

## 🛠️ Quick Start

### Prerequisites
- Windows 10/11
- Visual Studio 2022 with C++ tools
- CMake 3.21+
- vcpkg package manager

### Build & Run
```powershell
# Clone and build
git clone <your-repo-url>
cd Video_Editor

# Configure with vcpkg dependencies (PBO upload ON by default)
cmake --preset qt-debug

# (Optional) Disable PBO upload path or enable advanced modes
cmake -S . -B build/qt-debug -DVE_ENABLE_PBO_UPLOAD=OFF
cmake -S . -B build/qt-debug -DVE_GL_PBO_TRIPLE=ON            # triple buffering
cmake -S . -B build/qt-debug -DVE_GL_PBO_PERSISTENT_MAP=ON    # experimental

# Build
cmake --build build/qt-debug --config Debug

# Run
.\run_video_editor_debug.bat
```

## 📋 Problem Solved

This project specifically addresses **UI responsiveness issues** commonly found in media processing applications:

- ❌ **Before**: 38+ second paint freezes, unresponsive UI during media operations
- ✅ **After**: Smooth interactive UI with background processing and optimized rendering

## 🏗️ Architecture

### Core Components
- **Worker Threads**: `MediaProcessingWorker`, `TimelineProcessingWorker`
- **Chunked Processing**: 12ms time budget per operation chunk
- **Paint Optimization**: Cached objects, viewport culling, detail thresholds
- **Performance Monitoring**: Real-time responsiveness tracking

### Dependencies (via vcpkg)
- Qt6 (Core, Widgets, Gui)
- FFmpeg (media processing)
- spdlog (logging)
- fmt (formatting)

## 📁 Repository Structure

```
├── CMakeLists.txt              # Main CMake configuration
├── CMakePresets.json           # Build presets for vcpkg
├── vcpkg.json                  # Dependencies manifest
├── REPRO.md                    # Detailed reproduction guide
├── src/                        # Source code
│   ├── app/                    # Application layer
│   ├── ui/                     # UI components (main_window, timeline_panel)
│   ├── core/                   # Core functionality
│   ├── media_io/               # Media I/O operations
│   ├── timeline/               # Timeline management
│   └── playback/               # Playback engine
├── build/                      # Build artifacts
├── tests/                      # Unit tests
└── run_video_editor_debug.bat  # Debug launcher
```

## 🔧 Development

### Build System
- **CMake** with vcpkg integration
- **Multiple presets**: qt-debug, simple-debug, manual-debug
- **Automatic dependency management** via vcpkg.json

### Key Files
- `src/ui/src/main_window.cpp` - UI responsiveness implementation
- `src/ui/src/timeline_panel.cpp` - Optimized paint events
- `CMakeLists.txt` - Build configuration
 - `src/ui/include/ui/gl_video_widget.hpp` & `src/ui/src/gl_video_widget.cpp` - PBO upload logic & runtime/env toggles
### Build / Profiling / Debug Options

| CMake Option | Default | Description |
|--------------|---------|-------------|
| `VE_ENABLE_PBO_UPLOAD` | ON | Compile PBO upload path (falls back to direct glTexSubImage2D if OFF or runtime disabled) |
| `VE_GL_PBO_TRIPLE` | OFF | Use triple ring of PBOs to further reduce risk of driver sync stalls |
| `VE_GL_PBO_PERSISTENT_MAP` | OFF | Experimental persistent mapping (requires GL 4.4 + `GL_ARB_buffer_storage`) |
| `VE_ENABLE_DETAILED_PROFILING` | ON | Enables fine-grained profiling scopes (inner loops, sub-stages) |
| `VE_HEAP_DEBUG` | OFF | Adds guard allocations & CRT heap checks for diagnosing memory issues |
| `ENABLE_RUNTIME_DEBUG` | OFF | Verbose logging & extra runtime assertions |

Environment variables:

| Env Var | Effect |
|---------|--------|
| `VE_DISABLE_PBO` | Forces runtime fallback path (useful for A/B testing without rebuild) |
| `VE_DISABLE_FFMPEG_CONVERT` | Forces manual color converters instead of swscale |

These toggles allow rapid experimentation with upload strategies and profiling granularity during performance tuning.
- `vcpkg.json` - Dependencies

### Debugging Tools
- `debug_dll_test.bat` - Verify DLL dependencies
- Console logging with timing information
- Performance heartbeat monitoring

## 📊 Performance Results

### Before Optimization
- Paint events: **38,048ms** (38+ seconds)
- UI state: **Completely frozen** during operations
- User experience: **Unresponsive**, appears hung

### After Optimization
- Paint events: **<16ms** (smooth 60fps)
- UI state: **Always responsive** with background processing
- User experience: **Smooth and interactive**

## 🔍 For Detailed Information

See [REPRO.md](REPRO.md) for:
- Complete environment setup
- Step-by-step reproduction guide
- Troubleshooting instructions
- Architecture details

## 📝 License

This project serves as a reference implementation for Qt6 UI responsiveness solutions in media processing applications.

## 🤝 Contributing

This codebase demonstrates best practices for:
- Qt6 background processing patterns
- Paint event optimization techniques
- vcpkg integration with CMake
- Performance monitoring and profiling

Feel free to use this as a reference for similar UI responsiveness challenges in Qt6 applications.
