# C++ Video Player

A simple video player application written in C++ using SDL2 and FFmpeg libraries with **full audio support** and **GUI file selection**.

## Features

- **GUI File Selection**: Browse directories and select video files through an intuitive interface
- **Audio Device Selection**: Choose which audio output device to use for playback
- Load and play video files (MP4, AVI, MKV, MOV, WMV, FLV, WebM) with **audio support**
- **GUI Controls**: Play/pause/stop buttons, volume slider, seek controls
- Basic playback controls (play, pause, stop)
- Fullscreen mode
- **Volume control with real-time audio adjustment**
- Seek functionality
- Keyboard controls and GUI interface
- Command-line support for direct file loading

## Requirements

- C++17 compatible compiler (GCC, Clang, MSVC)
- SDL2 library
- FFmpeg libraries (libavcodec, libavformat, libavutil, libswscale)
- CMake (for building)

### Installing Dependencies

#### Windows (using vcpkg - Recommended)
```
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install dependencies
.\vcpkg install sdl2 ffmpeg
```

Then build with:
```
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

#### Windows (Manual Installation)
1. Download SDL2 development libraries from https://www.libsdl.org/download-2.0.php
2. Download FFmpeg development libraries from https://ffmpeg.org/download.html
3. Extract to a folder (e.g., C:\dev\SDL2, C:\dev\FFmpeg)
4. Set environment variables or run CMake with:
```
cmake .. -DSDL2_DIR=C:\dev\SDL2 -DCMAKE_PREFIX_PATH=C:\dev\FFmpeg
```

#### Linux (Ubuntu/Debian)
```
sudo apt-get install libsdl2-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

#### macOS
```
brew install sdl2 ffmpeg
```

## Build Instructions

1. Install vcpkg (if not already installed):
   ```
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

2. Install dependencies:
   ```
   cd vcpkg
   .\vcpkg install sdl2 ffmpeg imgui[opengl3-binding]
   ```

3. Build the project:
   ```
   cd Video_Player
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DPKG_CONFIG_EXECUTABLE=C:/path/to/vcpkg/installed/x64-windows/tools/pkgconf/pkgconf.exe
   cmake --build . --config Release
   ```

4. Run the executable:
   ```
   .\Release\video_player.exe
   ```

   Or load a video directly:
   ```
   .\Release\video_player.exe path\to\your\video.mp4
   ```

## Usage

The application starts with a **GUI file selection dialog** where you can:

- Browse directories by clicking on folder names
- Navigate to parent directories with the ".." button
- Select video files from the list of supported formats
- **Choose your preferred audio output device from the dropdown (takes effect when loading videos)**
- Enter a file path manually in the text input
- Click "Quit" to exit the application

Once a video is selected, it loads and begins playing automatically.

### Alternative: Command-line Usage

You can also run the executable with a video file as argument to skip the file selection dialog:

```
./video_player path/to/your/video.mp4
```

### GUI Controls

The application features a graphical user interface with the following controls:

- **Play/Pause Button**: Start or pause video playback
- **Stop Button**: Stop playback and return to beginning
- **Fullscreen Button**: Toggle fullscreen mode
- **Volume Slider**: Adjust audio volume (0.0 to 1.0) with real-time audio control
- **Position Slider**: Seek through the video (placeholder - full implementation pending)

### Keyboard Controls

- **Space**: Play/Pause
- **S**: Stop
- **F**: Toggle fullscreen
- **Up/Down**: Volume up/down
- **Left/Right**: Seek backward/forward
- **Tab**: Show/hide GUI controls
- **Escape**: Return to file selection (during playback) or exit (during file selection)

### GUI Features

- Modern dark theme interface
- Resizable control panel
- Real-time volume and position feedback
- Keyboard navigation support
- File selection dialog with directory browsing
- **Audio device selection dropdown (applied when loading new videos)**
- Error handling for invalid video files

## Project Structure

```
src/
├── main.cpp          # Entry point
├── VideoPlayer.h     # Video player class header
├── VideoPlayer.cpp   # Video player implementation
└── AudioManager.h    # Audio handling (optional)

CMakeLists.txt        # Build configuration
README.md            # This file
```

## Code Example

The application now supports dual modes: file selection and video playback.

Basic usage with GUI file selection:

```cpp
#include "VideoPlayer.h"
#include <filesystem>
#include <vector>

// ... ImGui and SDL initialization ...

// Application state
std::string selectedVideoPath;
bool videoLoaded = false;
VideoPlayer player(renderer); // Use external renderer

// Check command line arguments
if (argc > 1) {
    selectedVideoPath = argv[1];
    if (player.loadVideo(selectedVideoPath)) {
        videoLoaded = true;
        player.play();
    }
}

// Main loop
SDL_Event event;
bool running = true;
while (running) {
    // Handle events...
    
    ImGui::NewFrame();
    
    if (!videoLoaded) {
        // Show file selection dialog
        showFileSelectionDialog(selectedVideoPath, videoLoaded, player, running);
    } else {
        // Show video controls
        // ... GUI controls ...
        player.update();
    }
    
    // Rendering...
}
```

For command-line usage (bypasses file selection):

```cpp
VideoPlayer player;
if (!player.loadVideo(argv[1])) {
    std::cerr << "Failed to load video" << std::endl;
    return 1;
}
player.play();
// ... main loop ...
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- SDL2 for multimedia framework
- FFmpeg for video decoding
- Community contributors