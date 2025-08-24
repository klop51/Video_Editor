cmake_minimum_required(VERSION 3.25)

# Timeline module
add_library(ve_timeline
    src/track.cpp
    src/timeline.cpp
)

target_include_directories(ve_timeline
    PUBLIC include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/src/core/include
)

target_link_libraries(ve_timeline
    PUBLIC
        ve_core
)

target_compile_features(ve_timeline PUBLIC cxx_std_20)

# Create alias for consistency
add_library(timeline ALIAS ve_timeline)
