cmake_minimum_required(VERSION 3.25)

# Video utilities library - UI frame types and video-related helper classes
# This module provides thread-safe video frame handling for Qt UI integration

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

set(VIDEO_SOURCES
    UiImageFrame.cpp
)

set(VIDEO_HEADERS
    UiImageFrame.h
)

# Create the video library
add_library(video STATIC ${VIDEO_SOURCES} ${VIDEO_HEADERS})

# Set target properties
set_target_properties(video PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(video
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link Qt libraries
target_link_libraries(video
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

# Add compiler-specific flags
if(MSVC)
    target_compile_options(video PRIVATE /W4)
    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(video PRIVATE /WX)
    endif()
else()
    target_compile_options(video PRIVATE -Wall -Wextra -Wpedantic)
    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(video PRIVATE -Werror)
    endif()
endif()

# Add to video_editor_modules for main executable linkage
if(NOT TARGET video_editor_modules)
    add_library(video_editor_modules INTERFACE)
endif()
target_link_libraries(video_editor_modules INTERFACE video)