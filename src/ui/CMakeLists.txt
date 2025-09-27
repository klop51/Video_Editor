cmake_minimum_required(VERSION 3.25)

# Only build UI if Qt is available and enabled
if(NOT ENABLE_QT_TOOLS)
    message(STATUS "Skipping UI module: ENABLE_QT_TOOLS is OFF")
    return()
endif()

# Find Qt6
find_package(Qt6 QUIET COMPONENTS Core Widgets Multimedia OpenGLWidgets)
if(NOT Qt6_FOUND)
    message(WARNING "Qt6 not found, skipping UI module")
    return()
endif()

# Enable Qt's MOC for this module
set(CMAKE_AUTOMOC ON)

# UI module
add_library(ve_ui
    # Source files
    src/main_window.cpp
    src/timeline_panel.cpp
    src/timeline_dock.cpp
    src/viewer_panel.cpp
    src/gl_video_widget.cpp
    
    # UI update guard header
    UiUpdateGuard.h
    
    # Week 8 Qt Audio UI Components - Minimal Working Integration
    src/minimal_waveform_widget.cpp
    src/minimal_audio_track_widget.cpp
    src/minimal_audio_meters_widget.cpp
    
    # Phase 2 Professional Audio Monitoring UI
    src/professional_audio_monitoring_ui.cpp
    
    # Headers with Q_OBJECT (for MOC processing)
    include/ui/main_window.hpp
    include/ui/timeline_panel.hpp
    include/ui/timeline_dock.hpp
    include/ui/viewer_panel.hpp
    include/ui/gl_video_widget.hpp
    
    # Week 8 Audio UI Headers - Minimal Working Integration
    include/ui/minimal_waveform_widget.hpp
    include/ui/minimal_audio_track_widget.hpp
    include/ui/minimal_audio_meters_widget.hpp
)

target_include_directories(ve_ui
    PUBLIC include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/src/core/include
    # Week 8: Include Week 7 audio waveform system headers
    PRIVATE ${CMAKE_SOURCE_DIR}/src/audio/include
    # Needed for referencing Application signals in main_window (no link dependency)
    PRIVATE ${CMAKE_SOURCE_DIR}/src/app/include
    # Include video module for UiImageFrame
    PRIVATE ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ve_ui
    PUBLIC
        ve_core
        ve_media_io
        ve_decode
        ve_timeline
        ve_playback
        ve_commands
        ve_render
        ve_gfx
        # Week 8: Link Week 7 audio engine for waveform system
        ve_audio
        # Link video module for UiImageFrame
        video
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::OpenGLWidgets
)

target_compile_features(ve_ui PUBLIC cxx_std_20)

# Qt specific settings
# Note: Removed QT_NO_KEYWORDS to allow Q_OBJECT and slots/signals macros
# Disable warnings as errors for Qt deprecated methods
if(MSVC)
    target_compile_options(ve_ui PRIVATE /WX-)
endif()

# Create alias for consistency
add_library(ui ALIAS ve_ui)
