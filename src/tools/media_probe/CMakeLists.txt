# Media probe stub tool
add_executable(media_probe
    src/main.cpp
)

# For now only depends on core logging/time utilities
target_link_libraries(media_probe PRIVATE ve_core ve_media_io)

set_target_properties(media_probe PROPERTIES OUTPUT_NAME "ve_media_probe")
