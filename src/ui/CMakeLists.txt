cmake_minimum_required(VERSION 3.25)

# Only build UI if Qt is available and enabled
if(NOT ENABLE_QT_TOOLS)
    message(STATUS "Skipping UI module: ENABLE_QT_TOOLS is OFF")
    return()
endif()

# Find Qt6
find_package(Qt6 QUIET COMPONENTS Core Widgets Multimedia)
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
    src/viewer_panel.cpp
    
    # Headers with Q_OBJECT (for MOC processing)
    include/ui/main_window.hpp
    include/ui/timeline_panel.hpp
    include/ui/viewer_panel.hpp
)

target_include_directories(ve_ui
    PUBLIC include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/src/core/include
    # Needed for referencing Application signals in main_window (no link dependency)
    PRIVATE ${CMAKE_SOURCE_DIR}/src/app/include
)

target_link_libraries(ve_ui
    PUBLIC
        ve_core
        ve_media_io
        ve_decode
        ve_timeline
        ve_playback
        ve_commands
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
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
