cmake_minimum_required(VERSION 3.20)
project(Phase1Week3IntegrationTest)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog CONFIG REQUIRED)

# Include directories for the project
include_directories(src/core/include)
include_directories(src/audio/include)
include_directories(src/media_io/include)

# Enable FFmpeg support
add_definitions(-DVE_ENABLE_FFMPEG -DENABLE_FFMPEG)

# Add the test executable
add_executable(phase1_week3_integration_test 
    phase1_week3_ffmpeg_integration_test.cpp
)

# Link libraries
target_link_libraries(phase1_week3_integration_test 
    PRIVATE 
    ve_core
    ve_audio
    ve_media_io
    spdlog::spdlog
)

# Set output directory
set_target_properties(phase1_week3_integration_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)