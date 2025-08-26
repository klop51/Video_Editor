add_library(ve_audio STATIC
    src/audio_engine.cpp
)

target_include_directories(ve_audio PUBLIC include)

target_link_libraries(ve_audio PUBLIC ve_core)
