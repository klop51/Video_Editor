cmake_minimum_required(VERSION 3.25)
project(VideoEditor VERSION 0.1.0 LANGUAGES CXX)

# Options
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(ENABLE_TOOLS "Build developer / CLI tools" ON)
option(ENABLE_QT_TOOLS "Build Qt preview tools" ON)
option(ENABLE_TESTS "Build tests" ON)
option(ENABLE_FFMPEG "Enable FFmpeg integration" ON)
option(ENABLE_COVERAGE "Enable code coverage instrumentation (GCC/Clang, non-Windows)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
setup_project_warnings()

include(GNUInstallDirs)

# Coverage flags (only if enabled and supported)
if(ENABLE_COVERAGE AND NOT MSVC)
    message(STATUS "Code coverage instrumentation enabled")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

add_subdirectory(src/core) # Sets up external deps (fmt/spdlog) if not present
# Media IO (FFmpeg wrapper placeholder)
add_subdirectory(src/media_io)
# Decode (video/audio) module
add_subdirectory(src/decode)
# Playback controller
add_subdirectory(src/playback)
# Timeline and project model
add_subdirectory(src/timeline)
# Commands system
add_subdirectory(src/commands)
# GPU graphics abstraction (Vulkan-first)
add_subdirectory(src/gfx)
 # Future architecture modules (placeholders now)
add_subdirectory(src/render)
add_subdirectory(src/fx)
add_subdirectory(src/audio)
add_subdirectory(src/cache)
add_subdirectory(src/persistence)

# UI and application (only if Qt is enabled)
if(ENABLE_QT_TOOLS)
    add_subdirectory(src/ui)
    add_subdirectory(src/app)
endif()

if(ENABLE_TOOLS)
    add_subdirectory(src/tools/media_probe)
    add_subdirectory(src/tools/playback_demo)
    if(ENABLE_QT_TOOLS)
        add_subdirectory(src/tools/video_editor)
    endif()
endif()

if(ENABLE_QT_TOOLS)
    add_subdirectory(src/tools/qt_preview)
endif()

if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

set(CPACK_PACKAGE_NAME "VideoEditor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
include(CPack)
