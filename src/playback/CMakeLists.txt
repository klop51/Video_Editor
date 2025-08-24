cmake_minimum_required(VERSION 3.25)

# Playback module
add_library(ve_playback
    src/controller.cpp
)

target_include_directories(ve_playback
    PUBLIC include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/src/core/include
)

target_link_libraries(ve_playback
    PUBLIC
        ve_core
        ve_decode
)

target_compile_features(ve_playback PUBLIC cxx_std_20)

# Create alias for consistency
add_library(playback ALIAS ve_playback)
