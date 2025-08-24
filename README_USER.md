# Qt6 Video Editor

A professional video editing application built with Qt6 and C++20.

## Quick Start

### Running the Application

The easiest way to run the video editor is using one of the provided launcher scripts:

**Windows Batch File:**
```
run_video_editor.bat
```

**PowerShell Script:**
```
run_video_editor.ps1
```

Both launchers will:
- Check if the video editor is built
- Verify Qt6 dependencies are available
- Launch the application with proper error handling

### Building from Source

If you need to rebuild the application:

```bash
cmake --build build\simple-debug --config Release --target video_editor
```

## Features

### Current Functionality
- **Professional GUI** with dockable panels
- **Menu System** with File, Edit, and Playback menus
- **Timeline Panel** for video editing workflow
- **Media Browser** for asset management
- **Viewer Panel** for video preview
- **Properties Panel** for detailed settings
- **Toolbar** with common editing actions

### User Interface
- **Dockable Windows** - Rearrange panels to suit your workflow
- **Professional Layout** - Industry-standard video editing interface
- **Qt6 Native** - Modern, responsive user interface

## Technical Information

- **Framework:** Qt6 with Widgets and OpenGL support
- **Language:** C++20
- **Build System:** CMake with vcpkg package management
- **Dependencies:** Automatically bundled for standalone execution

## Logging

The application provides detailed logging information:
- Application startup and shutdown events
- User actions and menu interactions
- Error conditions and warnings

Log messages appear in the console when running from the command line.

## Troubleshooting

### Application Won't Start
1. Ensure you've built the Release version: `cmake --build build\simple-debug --config Release --target video_editor`
2. Use the provided launcher scripts for automatic dependency handling
3. Check that Visual C++ Redistributable is installed

### Missing Dependencies
The launcher scripts automatically handle Qt6 and other dependencies. If you encounter issues:
- All required DLLs are bundled in the Release build directory
- Qt6 platform plugins are automatically deployed
- No manual PATH configuration required

## Development Status

✅ **Complete:** Professional Qt6 GUI framework
✅ **Complete:** Menu system and toolbar
✅ **Complete:** Dockable panel layout
✅ **Complete:** Application lifecycle management
✅ **Complete:** Standalone deployment with all dependencies

This video editor provides a solid foundation for professional video editing applications with a modern Qt6 interface.
