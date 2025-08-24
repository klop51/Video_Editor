add_executable(playback_demo
    src/main.cpp
)

target_link_libraries(playback_demo PRIVATE ve_core ve_decode ve_media_io)

set_target_properties(playback_demo PROPERTIES OUTPUT_NAME "ve_playback_demo")
