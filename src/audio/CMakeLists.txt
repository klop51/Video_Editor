# Audio Engine Module
# Provides professional audio processing and decoding capabilities

# Audio module library
add_library(ve_audio STATIC
    src/audio_frame.cpp
    src/decoder.cpp
    src/test_decoder.cpp
    src/sample_rate_converter.cpp      # Phase 1 Week 2: High-quality resampling
    src/audio_buffer_pool.cpp          # Phase 1 Week 2: Lock-free buffer management
    src/audio_clock.cpp                # Phase 1 Week 2: Precision audio timeline sync
    src/ffmpeg_audio_decoder.cpp       # Phase 1 Week 3: FFmpeg audio decoder integration
    src/ffmpeg_audio_encoder.cpp       # Week 9: FFmpeg audio encoder integration
    src/export_presets.cpp             # Week 9: Professional export presets system
    src/mixing_graph.cpp               # Phase 2 Week 4: Node-based mixing architecture
    src/audio_effects.cpp              # Phase 2 Week 5: Professional audio effects suite
    src/audio_render_engine.cpp        # Phase 2 Week 3 + Week 9: Advanced audio rendering with presets
    src/audio_output.cpp               # Phase 1A: WASAPI audio output backend
    src/simple_mixer.cpp               # Phase 1B: Simple mixer core
    src/audio_pipeline.cpp             # Phase 1C: Audio pipeline integration
    src/timeline_audio_manager.cpp     # Phase 1D: Timeline audio integration for multi-track mixing
    src/master_clock.cpp               # Phase 2 Week 6: A/V synchronization master clock
    src/sync_validator.cpp             # Phase 2 Week 6: A/V synchronization validation framework
    src/latency_compensator.cpp        # Phase 2 Week 6: Automatic latency detection and compensation
    src/loudness_monitor.cpp           # Week 10: Real-time loudness monitoring (EBU R128)
    src/audio_meters.cpp               # Week 10: Professional audio meters with broadcast ballistics
    src/quality_dashboard.cpp          # Week 10: Quality analysis dashboard and compliance monitoring
    # Keep existing audio_engine.cpp if it exists
    $<$<BOOL:${CMAKE_CURRENT_SOURCE_DIR}/src/audio_engine.cpp>:src/audio_engine.cpp>
)

target_include_directories(ve_audio
    PUBLIC
        include
    PRIVATE
        .
)

# Link dependencies
target_link_libraries(ve_audio
    PUBLIC
        ve_core
        ve_media_io  # For FFmpeg integration and demuxer support
        ve_timeline  # For timeline integration and audio track management
        ve_decode    # For audio decoding from segments
)

# FFmpeg integration for audio decoder
if(ENABLE_FFMPEG)
    target_compile_definitions(ve_audio PUBLIC ENABLE_FFMPEG=1)
    
    # Explicitly link FFmpeg libraries for audio encoder/decoder functionality
    find_package(FFMPEG REQUIRED)
    target_link_libraries(ve_audio PUBLIC ${FFMPEG_LIBRARIES})
    target_include_directories(ve_audio PRIVATE ${FFMPEG_INCLUDE_DIRS})
    
    # Relax warnings for external headers on MSVC
    if(MSVC)
        target_compile_options(ve_audio PRIVATE /W3 /FS)
    endif()
else()
    target_compile_definitions(ve_audio PUBLIC ENABLE_FFMPEG=0)
    message(STATUS "FFmpeg disabled - audio decoder will have limited functionality")
endif()

# Set library properties
set_target_properties(ve_audio PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add definitions for audio engine configuration
target_compile_definitions(ve_audio PRIVATE
    VE_AUDIO_ENGINE_VERSION_MAJOR=1
    VE_AUDIO_ENGINE_VERSION_MINOR=0
    VE_AUDIO_ENGINE_VERSION_PATCH=0
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(ve_audio PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    
    # Link Windows audio libraries for WASAPI
    target_link_libraries(ve_audio PRIVATE
        ole32      # COM support
        avrt       # MMCSS for low-latency audio
    )
endif()

# Compiler-specific optimizations for audio processing
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(ve_audio PRIVATE
        /FS        # Enable file system synchronization for PDB files
        /arch:AVX2  # Enable AVX2 for audio SIMD operations if available
        /fp:fast    # Fast floating point for audio processing
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ve_audio PRIVATE
        -mavx2      # Enable AVX2 for audio SIMD operations if available
        -mfma       # Enable FMA for audio calculations if available
        -ffast-math # Fast math for audio processing
    )
endif()

# Audio module tests are handled in main CMakeLists.txt and tests/ directory
