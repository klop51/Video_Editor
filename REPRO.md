# UI Responsiveness Issue Reproduction Guide

## Environment Requirements

### Operating System
- **Windows 10/11** (tested on Windows 11)
- **Visual Studio 2022** with C++ development tools
- **Git** for version control

### Compiler & Build Tools
- **MSVC 2022** (Visual Studio 17.14.18 or later)
- **CMake 3.21+** 
- **vcpkg** package manager
- **PowerShell 5.1+** for build scripts

### Qt Framework
- **Qt6 (6.5+)** installed via vcpkg
- Qt6 components: Core, Widgets, Gui
- **Note**: Qt6 requires proper vcpkg toolchain configuration

### Dependencies (via vcpkg)
- **FFmpeg 4.4+** for media processing
- **spdlog** for logging
- **fmt** for string formatting

## Environment Variables
No special environment variables required - vcpkg handles all dependencies automatically.

## Build Instructions

### 1. Clone Repository
```powershell
git clone <your-repo-url>
cd Video_Editor
```

### 2. Install Dependencies via vcpkg
```powershell
# vcpkg will automatically install all dependencies listed in vcpkg.json
cmake --preset qt-debug
```

### 3. Build Project
```powershell
cmake --build build/qt-debug --config Debug
```

### 4. Run Application
```powershell
# Use the debug launcher script
.\run_video_editor_debug.bat
```

## Reproducing the Original UI Freeze Issue

**Note**: The UI responsiveness issues have been **FIXED** in this repository. To reproduce the original problem, you would need to revert the optimizations.

### Original Problem Symptoms
1. **UI becomes completely unresponsive** during media import
2. **Timeline rendering takes 38+ seconds** causing complete freeze
3. **Mouse/keyboard input ignored** during processing
4. **Application appears hung** but actually processing in background

### Steps to See the Fixed Behavior

1. **Launch Application**:
   ```powershell
   .\run_video_editor_debug.bat
   ```

2. **Import Large Media File**:
   - Click "Import Media" 
   - Select a large video file (>100MB recommended)
   - **Observe**: UI remains responsive, progress shown

3. **Add to Timeline**:
   - Right-click imported media
   - Select "Add to Timeline"
   - **Observe**: Background processing with responsive UI

4. **Timeline Interaction**:
   - Scroll/zoom timeline
   - Move segments
   - **Observe**: Smooth paint events, no freezing

### Performance Monitoring

The application includes extensive logging to monitor responsiveness:

- **Heartbeat logs**: `HEARTBEAT # XX - UI thread is responsive`
- **Paint timing**: Fast paint events (no more 38+ second freezes)
- **Chunked processing**: Background operations with time budgets
- **Mouse tracking**: `MOUSE MOVE # XX` for interaction verification

## Architecture of the Solution

### Key Optimizations Implemented

1. **Worker Threads**:
   - `MediaProcessingWorker` for background media operations
   - `TimelineProcessingWorker` for chunked timeline updates
   - Prevents blocking the main UI thread

2. **Chunked Processing**:
   - 12ms time budget per processing chunk
   - Queue-based batch processing
   - Allows UI to remain interactive

3. **Paint Event Optimization**:
   - Cached drawing objects (colors, fonts, brushes)
   - Viewport culling to skip off-screen elements
   - Aggressive detail thresholds (60px minimum)
   - Pre-calculated track types

4. **Performance Monitoring**:
   - Scoped timing macros
   - Real-time responsiveness heartbeats
   - Background processing timing

## Troubleshooting

### Build Issues
- Ensure vcpkg is properly installed and integrated
- Check CMAKE_PREFIX_PATH points to vcpkg_installed/x64-windows
- Verify Qt6 is installed via vcpkg (not system Qt)

### Runtime Issues
- Use `debug_dll_test.bat` to verify DLL dependencies
- Check console output for detailed logging
- Ensure video files are in supported formats (MP4, AVI, etc.)

### Performance Issues
- Monitor console for paint event timing
- Look for "SLOW PAINT EVENT" messages (should not appear)
- Check heartbeat frequency for UI responsiveness

## Development Notes

This project demonstrates a complete solution for Qt6 UI responsiveness issues in media processing applications. The implementation serves as a reference for:

- **Background processing** patterns in Qt6
- **Paint event optimization** techniques
- **vcpkg integration** with CMake and Qt6
- **Performance monitoring** and profiling
- **Cross-platform build** configuration (Windows focus)

The codebase includes comprehensive documentation, build scripts, and debugging tools to assist with development and deployment.
