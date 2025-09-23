cmake_minimum_required(VERSION 3.25)
project(VideoEditor VERSION 0.1.0 LANGUAGES CXX)

# Options
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(ENABLE_TOOLS "Build developer / CLI tools" ON)
option(ENABLE_QT_TOOLS "Build Qt preview tools" ON)
option(ENABLE_TESTS "Build tests" ON)
option(ENABLE_FFMPEG "Enable FFmpeg integration" ON)
option(ENABLE_D3D11VA "Enable D3D11VA hardware acceleration (Windows)" ON)
option(ENABLE_D3D11_ZERO_COPY "Pass ID3D11Texture2D* directly to viewer" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation (GCC/Clang, non-Windows)" OFF)
option(ENABLE_RUNTIME_DEBUG "Enable verbose runtime debug checks" OFF)
option(VE_ENABLE_PBO_UPLOAD "Enable OpenGL PBO upload path (double-buffered) for GLVideoWidget" ON)
option(VE_ENABLE_DETAILED_PROFILING "Enable fine-grained (inner loop / GPU sub-stage) profiling scopes" ON)
option(VE_HEAP_DEBUG "Enable extra heap guard checks in hot paths (Windows CRT + manual guards)" OFF)
option(VE_GL_PBO_TRIPLE "Use triple PBO ring instead of double for GL upload (reduces chance of stall)" OFF)
option(VE_GL_PBO_PERSISTENT_MAP "Experimental: persistently map PBOs (if driver supports)" OFF)

# Configure debug settings globally
if(ENABLE_RUNTIME_DEBUG)
  add_compile_definitions(
    VE_RUNTIME_DEBUG=1
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG  # compile in debug logs
  )
  message(STATUS "Runtime debug enabled: verbose logging and debug checks active")
else()
  add_compile_definitions(
    VE_RUNTIME_DEBUG=0
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO   # compile OUT debug/trace logs
    QT_NO_DEBUG_OUTPUT                       # strips qDebug() calls
  )
  message(STATUS "Runtime debug disabled: debug logs compiled out for performance")
endif()

# Propagate PBO upload toggle globally so headers can test it
if(VE_ENABLE_PBO_UPLOAD)
  add_compile_definitions(VE_ENABLE_PBO_UPLOAD=1)
  message(STATUS "PBO upload path ENABLED (VE_ENABLE_PBO_UPLOAD=1)")
else()
  add_compile_definitions(VE_ENABLE_PBO_UPLOAD=0)
  message(STATUS "PBO upload path DISABLED (VE_ENABLE_PBO_UPLOAD=0)")
endif()

# Detailed profiling toggle
if(VE_ENABLE_DETAILED_PROFILING)
  add_compile_definitions(VE_ENABLE_DETAILED_PROFILING=1)
  message(STATUS "Detailed profiling ENABLED (VE_ENABLE_DETAILED_PROFILING=1)")
else()
  add_compile_definitions(VE_ENABLE_DETAILED_PROFILING=0)
  message(STATUS "Detailed profiling DISABLED (VE_ENABLE_DETAILED_PROFILING=0)")
endif()

# Heap debug toggle
if(VE_HEAP_DEBUG)
  add_compile_definitions(VE_HEAP_DEBUG=1)
  message(STATUS "Heap debug ENABLED (VE_HEAP_DEBUG=1)")
else()
  add_compile_definitions(VE_HEAP_DEBUG=0)
endif()

# PBO advanced options
if(VE_GL_PBO_TRIPLE)
  add_compile_definitions(VE_GL_PBO_TRIPLE=1)
  message(STATUS "Triple PBO ring ENABLED (VE_GL_PBO_TRIPLE=1)")
else()
  add_compile_definitions(VE_GL_PBO_TRIPLE=0)
endif()

if(VE_GL_PBO_PERSISTENT_MAP)
  add_compile_definitions(VE_GL_PBO_PERSISTENT_MAP=1)
  message(STATUS "Persistent mapped PBOs ENABLED (VE_GL_PBO_PERSISTENT_MAP=1) [experimental]")
else()
  add_compile_definitions(VE_GL_PBO_PERSISTENT_MAP=0)
endif()

# D3D11VA hardware acceleration settings
if(ENABLE_D3D11VA)
  add_compile_definitions(VE_ENABLE_D3D11VA=1)
  message(STATUS "D3D11VA hardware acceleration ENABLED")
else()
  add_compile_definitions(VE_ENABLE_D3D11VA=0)
endif()

if(ENABLE_D3D11_ZERO_COPY)
  add_compile_definitions(VE_ZERO_COPY_D3D11=1)
  message(STATUS "D3D11 zero-copy ENABLED (experimental)")
else()
  add_compile_definitions(VE_ZERO_COPY_D3D11=0)
endif()

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

# Debug symbols and runtime guards for crash investigation
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES)
    if(MSVC)
        # Windows MSVC: debug symbols and runtime checks
        add_compile_options(/Zi /Od /RTC1)
        add_link_options(/DEBUG)
        message(STATUS "MSVC debug symbols and runtime checks enabled")
    else()
        # Linux/macOS: debug symbols and sanitizers
        add_compile_options(-g -O0 -fno-omit-frame-pointer)
        add_link_options(-g)
        message(STATUS "GCC/Clang debug symbols enabled")
    endif()
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
    add_subdirectory(src/tools/gpu_render_demo)
    add_subdirectory(src/tools/test_d3d11)
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
    
    # Phase 2 Week 5: Simple HDR Infrastructure Test
    add_executable(test_hdr_infrastructure_simple 
        test_hdr_infrastructure_simple.cpp
    )
    
    target_link_libraries(test_hdr_infrastructure_simple PRIVATE ve_media_io ve_core)
    
    target_include_directories(test_hdr_infrastructure_simple PRIVATE
        ${CMAKE_SOURCE_DIR}/src/media_io/include
        ${CMAKE_SOURCE_DIR}/src/decode/include
        ${CMAKE_SOURCE_DIR}/src/core/include
    )
    
    add_test(NAME test_hdr_infrastructure_simple COMMAND test_hdr_infrastructure_simple)
endif()

# Week 2 Memory Management Test
if(ENABLE_TOOLS)
    add_executable(test_memory_management 
        test_memory_management.cpp
    )
    
    target_link_libraries(test_memory_management 
        PRIVATE 
        ve_gfx 
        ve_core
    )
    
    target_include_directories(test_memory_management 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 2 GPU Bridge Validation
    add_executable(gpu_bridge_phase2_complete 
        gpu_bridge_phase2_complete.cpp
    )
    
    target_link_libraries(gpu_bridge_phase2_complete 
        PRIVATE 
        ve_gfx 
        ve_core
    )
    
    target_include_directories(gpu_bridge_phase2_complete 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 3 Compute Pipeline Validation
    add_executable(gpu_bridge_phase3_compute_validation 
        gpu_bridge_phase3_compute_validation.cpp
    )
    
    target_link_libraries(gpu_bridge_phase3_compute_validation 
        PRIVATE 
        ve_gfx 
        ve_core
    )
    
    target_include_directories(gpu_bridge_phase3_compute_validation 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 4 Effects Pipeline Validation
    add_executable(gpu_bridge_phase4_effects_validation 
        gpu_bridge_phase4_effects_validation.cpp
    )
    
    target_link_libraries(gpu_bridge_phase4_effects_validation 
        PRIVATE 
        ve_gfx 
        ve_core
    )
    
    target_include_directories(gpu_bridge_phase4_effects_validation 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 5 Advanced Features Validation
    add_executable(gpu_bridge_phase5_advanced_validation 
        gpu_bridge_phase5_advanced_validation.cpp
    )
    
    target_link_libraries(gpu_bridge_phase5_advanced_validation 
        PRIVATE 
        ve_gfx 
        ve_core
    )
    
    target_include_directories(gpu_bridge_phase5_advanced_validation 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # AUDIO ENGINE ROADMAP Phase 1 Week 1 validation executable
    add_executable(audio_engine_phase1_week1_validation
        src/audio/validation/audio_engine_phase1_week1_validation_simple.cpp
    )
    
    target_link_libraries(audio_engine_phase1_week1_validation PRIVATE
        ve_audio
        ve_media_io
        ve_core
    )
    
    target_include_directories(audio_engine_phase1_week1_validation PRIVATE
        src/audio/include
        src/media_io/include
        src/core/include
    )

    # AUDIO ENGINE ROADMAP Phase 1 Week 2 validation executable  
    add_executable(audio_engine_phase1_week2_validation
        audio_phase1_week2_validation_simple.cpp
    )
    
    target_link_libraries(audio_engine_phase1_week2_validation PRIVATE
        ve_audio
        ve_media_io
        ve_core
    )
    
    target_include_directories(audio_engine_phase1_week2_validation PRIVATE
        src/audio/include
        src/media_io/include
        src/core/include
    )

    # AUDIO ENGINE ROADMAP Phase 2 Week 4 Advanced Mixing Graph validation executable
    add_executable(audio_engine_phase2_week4_validation
        audio_engine_phase2_week4_validation.cpp
    )
    
    target_link_libraries(audio_engine_phase2_week4_validation PRIVATE
        ve_audio
        ve_media_io
        ve_core
    )
    
    target_include_directories(audio_engine_phase2_week4_validation PRIVATE
        src/audio/include
        src/media_io/include
        src/core/include
    )
    
    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_engine_phase2_week4_validation PRIVATE /W0)
    endif()

    # AUDIO ENGINE ROADMAP Phase 2 Week 6 A/V Synchronization Integration Test
    add_executable(audio_engine_week6_integration_test
        audio_engine_week6_integration_test.cpp
    )
    
    target_link_libraries(audio_engine_week6_integration_test PRIVATE
        ve_audio
        ve_media_io
        ve_core
    )
    
    target_include_directories(audio_engine_week6_integration_test PRIVATE
        src/audio/include
        src/media_io/include
        src/core/include
    )
    
    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_engine_week6_integration_test PRIVATE /W0)
    endif()

    # AUDIO ENGINE Week 6 Debug Test
    add_executable(audio_engine_week6_debug_test
        audio_engine_week6_debug_test.cpp
    )
    
    target_link_libraries(audio_engine_week6_debug_test PRIVATE
        ve_audio
        ve_core
    )
    
    target_include_directories(audio_engine_week6_debug_test PRIVATE
        src/audio/include
        src/core/include
    )
    
    # Disable warnings for this debug test
    if(MSVC)
        target_compile_options(audio_engine_week6_debug_test PRIVATE /W0)
    endif()

    # AUDIO ENGINE ROADMAP Phase 2 Week 7 Waveform System Integration Test
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(audio_engine_week7_waveform_integration_test
            audio_engine_week7_waveform_integration_test.cpp
        )
        
        target_link_libraries(audio_engine_week7_waveform_integration_test PRIVATE
            ve_audio
            ve_media_io
            ve_core
            GTest::gtest
            GTest::gtest_main
        )
        
        target_include_directories(audio_engine_week7_waveform_integration_test PRIVATE
            src/audio/include
            src/media_io/include
            src/core/include
        )
        
        # Disable warnings for this test executable
        if(MSVC)
            target_compile_options(audio_engine_week7_waveform_integration_test PRIVATE /W0)
        endif()
    else()
        message(STATUS "Skipping audio_engine_week7_waveform_integration_test - GTest not found")
    endif()

    # AUDIO ENGINE ROADMAP Phase 2 Week 7 Basic Validation Test
    add_executable(audio_engine_week7_basic_validation
        audio_engine_week7_basic_validation.cpp
    )
    
    target_link_libraries(audio_engine_week7_basic_validation PRIVATE
        ve_core
    )
    
    target_include_directories(audio_engine_week7_basic_validation PRIVATE
        src/core/include
    )
    
    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_engine_week7_basic_validation PRIVATE /W0)
    endif()

    # AUDIO ENGINE ROADMAP Phase 1B: Simple Mixer Core Validation
    add_executable(simple_mixer_validation
        simple_mixer_validation.cpp
    )

    target_link_libraries(simple_mixer_validation PRIVATE
        ve_audio
        ve_core
    )

    target_include_directories(simple_mixer_validation PRIVATE
        src/audio/include
        src/core/include
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(simple_mixer_validation PRIVATE /W0)
    endif()

    # AUDIO ENGINE ROADMAP Phase 1C: Audio Pipeline Integration Validation
    add_executable(audio_pipeline_validation
        audio_pipeline_validation.cpp
    )

    target_link_libraries(audio_pipeline_validation PRIVATE
        ve_audio
        ve_core
    )

    target_include_directories(audio_pipeline_validation PRIVATE
        src/audio/include
        src/core/include
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_pipeline_validation PRIVATE /W0)
    endif()

    # Phase 2 Week 1: Basic Audio Pipeline Validation
    add_executable(test_phase2_week1_audio_pipeline
        test_phase2_week1_audio_pipeline.cpp
    )
    
    target_link_libraries(test_phase2_week1_audio_pipeline PRIVATE
        ve_audio
        ve_core
    )
    
    target_include_directories(test_phase2_week1_audio_pipeline PRIVATE
        src/audio/include
        src/core/include
    )
    
    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(test_phase2_week1_audio_pipeline PRIVATE /W0)
    endif()

    # Audio Pipeline Mixer Integration Test
    add_executable(audio_pipeline_mixer_test
        audio_pipeline_mixer_test.cpp
    )
    
    target_link_libraries(audio_pipeline_mixer_test PRIVATE
        ve_audio
        ve_core
    )
    
    target_include_directories(audio_pipeline_mixer_test PRIVATE
        src/audio/include
        src/core/include
    )
    
    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_pipeline_mixer_test PRIVATE /W0)
    endif()

    # Phase 2 Week 2: Audio Synchronization Validation
    add_executable(test_phase2_week2_audio_sync
        test_phase2_week2_audio_sync.cpp
    )
    
    target_link_libraries(test_phase2_week2_audio_sync PRIVATE
        ve_audio
        ve_core
    )
    
    target_include_directories(test_phase2_week2_audio_sync PRIVATE
        src/audio/include
        src/core/include
    )
    
    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(test_phase2_week2_audio_sync PRIVATE /W0)
    endif()

    # Phase 2 Week 3: Audio Rendering Engine Validation
    add_executable(test_phase2_week3_audio_render
        test_phase2_week3_audio_render.cpp
    )
    
    target_link_libraries(test_phase2_week3_audio_render PRIVATE
        ve_audio
        ve_media_io
        ve_core
    )
    
    target_include_directories(test_phase2_week3_audio_render PRIVATE
        src/audio/include
        src/core/include
    )
    
    # Add FFmpeg support for audio rendering test
    if(ENABLE_FFMPEG)
        # Explicitly link FFmpeg libraries for test
        find_package(FFMPEG REQUIRED)
        target_link_libraries(test_phase2_week3_audio_render PRIVATE ${FFMPEG_LIBRARIES})
        target_include_directories(test_phase2_week3_audio_render PRIVATE ${FFMPEG_INCLUDE_DIRS})
    endif()
    
    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(test_phase2_week3_audio_render PRIVATE /W0)
    endif()

    # Simple header test for audio render engine
    add_executable(test_audio_render_include
        test_audio_render_include.cpp
    )
    
    target_link_libraries(test_audio_render_include PRIVATE
        ve_audio
        ve_core
    )
    
    target_include_directories(test_audio_render_include PRIVATE
        src/audio/include
        src/core/include
    )
    
    if(MSVC)
        target_compile_options(test_audio_render_include PRIVATE /W0)
    endif()

    # Phase 2 Week 5: Audio Effects Suite Validation
    add_executable(audio_effects_phase2_week5_validation
        audio_effects_phase2_week5_validation.cpp
    )

    target_link_libraries(audio_effects_phase2_week5_validation PRIVATE
        ve_audio
        ve_core
    )

    target_include_directories(audio_effects_phase2_week5_validation PRIVATE
        src/core/include
        src/audio/include
    )

    set_target_properties(audio_effects_phase2_week5_validation PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_effects_phase2_week5_validation PRIVATE /W0)
    endif()

    # Week 10: Real-Time Audio Monitoring Validation (Standalone)
    add_executable(audio_monitoring_week10_validation_standalone
        audio_monitoring_week10_validation_standalone.cpp
    )

    # This is a standalone validation that doesn't depend on the audio engine
    # It demonstrates all Week 10 concepts independently
    set_target_properties(audio_monitoring_week10_validation_standalone PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_monitoring_week10_validation_standalone PRIVATE /W0)
    endif()

    # Week 10: Real-Time Audio Monitoring Validation
    add_executable(audio_monitoring_week10_validation
        audio_monitoring_week10_validation.cpp
    )

    target_link_libraries(audio_monitoring_week10_validation PRIVATE
        ve_audio
        ve_core
    )

    target_include_directories(audio_monitoring_week10_validation PRIVATE
        src/core/include
        src/audio/include
    )

    set_target_properties(audio_monitoring_week10_validation PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_monitoring_week10_validation PRIVATE /W0)
    endif()

    # Phase 2 Week 6: A/V Synchronization Validation Framework Test
    add_executable(sync_validator_test
        sync_validator_test.cpp
    )

    target_link_libraries(sync_validator_test PRIVATE
        ve_audio
        ve_core
    )

    target_include_directories(sync_validator_test PRIVATE
        src/core/include
        src/audio/include
    )

    set_target_properties(sync_validator_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(sync_validator_test PRIVATE /W0)
    endif()

    # FFmpeg Codec Enhancement Validation Test
    add_executable(ffmpeg_codec_validation_test
        ffmpeg_codec_validation_test.cpp
    )

    target_link_libraries(ffmpeg_codec_validation_test PRIVATE
        ve_audio
        ve_core
    )

    target_include_directories(ffmpeg_codec_validation_test PRIVATE
        src/core/include
        src/audio/include
    )

    set_target_properties(ffmpeg_codec_validation_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(ffmpeg_codec_validation_test PRIVATE /W0)
    endif()

    # Week 9 Audio Export Pipeline Validation Test
    add_executable(audio_export_presets_week9_validation
        audio_export_presets_week9_validation.cpp
    )

    target_link_libraries(audio_export_presets_week9_validation PRIVATE
        ve_audio
        ve_core
    )

    target_include_directories(audio_export_presets_week9_validation PRIVATE
        src/core/include
        src/audio/include
    )

    set_target_properties(audio_export_presets_week9_validation PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_export_presets_week9_validation PRIVATE /W0)
    endif()

    # Week 9 Audio Export Pipeline Simple Validation Test (Standalone)
    add_executable(audio_export_presets_week9_simple_validation
        audio_export_presets_week9_simple_validation.cpp
    )

    # No dependencies - standalone implementation
    set_target_properties(audio_export_presets_week9_simple_validation PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    # Disable warnings for this test executable
    if(MSVC)
        target_compile_options(audio_export_presets_week9_simple_validation PRIVATE /W0)
    endif()

    # AUDIO ENGINE ROADMAP Phase 1 Week 3 FFmpeg Integration Test
    add_executable(phase1_week3_ffmpeg_integration_test
        phase1_week3_ffmpeg_integration_test_simple.cpp
    )
    
    target_link_libraries(phase1_week3_ffmpeg_integration_test PRIVATE
        ve_audio
        ve_media_io
        ve_core
    )
    
    target_include_directories(phase1_week3_ffmpeg_integration_test PRIVATE
        src/audio/include
        src/media_io/include
        src/core/include
    )

    # FFmpeg Direct Functionality Test
    add_executable(ffmpeg_direct_test
        ffmpeg_direct_test.cpp
    )
    
    target_link_libraries(ffmpeg_direct_test PRIVATE
        ve_audio
        ve_media_io
        ve_core
    )
    
    if(ENABLE_FFMPEG)
        # Explicitly link FFmpeg libraries for test
        find_package(FFMPEG REQUIRED)
        target_link_libraries(ffmpeg_direct_test PRIVATE ${FFMPEG_LIBRARIES})
    endif()
    
    target_include_directories(ffmpeg_direct_test PRIVATE
        src/audio/include
        src/media_io/include
        src/core/include
    )

    # FORMAT_SUPPORT_ROADMAP.md Phase 1 Week 1 validation executable
    add_executable(format_support_phase1_week1_validation
        format_support_phase1_week1_validation.cpp
    )
    
    target_link_libraries(format_support_phase1_week1_validation PRIVATE
        ve_media_io
        ve_decode  
        ve_core
    )
    
    target_include_directories(format_support_phase1_week1_validation PRIVATE
        src/media_io/include
        src/decode/include
        src/core/include
    )

    # FORMAT_SUPPORT_ROADMAP.md Phase 1 Week 2 validation executable
    add_executable(format_support_phase1_week2_validation
        format_support_phase1_week2_validation.cpp
    )
    
    target_link_libraries(format_support_phase1_week2_validation PRIVATE
        ve_media_io
        ve_decode  
        ve_core
    )
    
    target_include_directories(format_support_phase1_week2_validation PRIVATE
        src/media_io/include
        src/decode/include
        src/core/include
    )

    # FORMAT_SUPPORT_ROADMAP.md Phase 1 Week 3 validation executable
    add_executable(format_support_phase1_week3_validation
        format_support_phase1_week3_validation.cpp
    )
    
    target_link_libraries(format_support_phase1_week3_validation PRIVATE
        ve_media_io
        ve_decode  
        ve_core
    )
    
    target_include_directories(format_support_phase1_week3_validation PRIVATE
        src/media_io/include
        src/decode/include
        src/core/include
    )

    # Phase 6: Production Readiness Testing validation executable
    add_executable(gpu_bridge_phase6_production_validation 
        gpu_bridge_phase6_production_validation.cpp
    )
    
    target_link_libraries(gpu_bridge_phase6_production_validation 
        PRIVATE 
        ve_gfx 
        ve_core
    )
    
    target_include_directories(gpu_bridge_phase6_production_validation 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 1 Week 4: Modern Codec Integration Test
    add_executable(simple_modern_codec_test 
        simple_modern_codec_test.cpp
    )
    
    target_link_libraries(simple_modern_codec_test 
        PRIVATE 
        ve_media_io 
        ve_core
    )
    
    target_include_directories(simple_modern_codec_test 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media_io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/decode/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 2 Week 5: HDR Infrastructure Validation Test
    add_executable(hdr_validation_test 
        hdr_validation_test.cpp
    )
    
    target_link_libraries(hdr_validation_test 
        PRIVATE 
        ve_media_io 
        ve_core
    )
    
    target_include_directories(hdr_validation_test 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media_io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 2 Week 6: Log Format Support Validation Test
    add_executable(log_format_validation_test 
        log_format_validation_test.cpp
    )
    
    target_link_libraries(log_format_validation_test 
        PRIVATE 
        ve_media_io 
        ve_core
    )
    
    target_include_directories(log_format_validation_test 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media_io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )
    
    # Phase 2 Week 7: High Bit Depth Pipeline Validation Test
    add_executable(high_bitdepth_validation_test 
        high_bitdepth_validation_test.cpp
    )
    
    target_link_libraries(high_bitdepth_validation_test 
        PRIVATE 
        ve_media_io 
        ve_core
    )
    
    target_include_directories(high_bitdepth_validation_test 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media_io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 3 Week 10: RAW Format Foundation Validation Test
    add_executable(raw_format_validation_test 
        raw_format_validation_test.cpp
    )
    
    target_link_libraries(raw_format_validation_test 
        PRIVATE 
        ve_media_io 
        ve_core
    )
    
    target_include_directories(raw_format_validation_test 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media_io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )

    # Phase 3 Week 11: Container Format Expansion Validation Test
    add_executable(container_format_validation_test 
        container_format_validation_test.cpp
    )
    
    target_link_libraries(container_format_validation_test 
        PRIVATE 
        ve_media_io 
        ve_core
    )
    
    target_include_directories(container_format_validation_test 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media_io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    )
endif()

# Debug tool for baby talk audio issue - sample rate analysis
add_executable(debug_sample_rate_test debug_sample_rate_test.cpp)
target_link_libraries(debug_sample_rate_test
    PRIVATE
    ve_media_io
    ve_core
    ${FFMPEG_LIBRARIES}
)
target_include_directories(debug_sample_rate_test
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/media_io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    ${FFMPEG_INCLUDE_DIRS}
)

# Week 8: Qt Timeline UI Integration Test Application
if(ENABLE_QT_TOOLS)
    find_package(Qt6 QUIET COMPONENTS Core Widgets Concurrent)
    if(Qt6_FOUND)
        # Enable Qt's MOC for this test
        set(CMAKE_AUTOMOC ON)
        
        # Create a minimal UI library with just our Week 8 components (no dependencies on existing UI)
        add_library(ve_ui_minimal
            src/ui/src/minimal_waveform_widget.cpp
            src/ui/src/minimal_audio_track_widget.cpp
            src/ui/src/minimal_audio_meters_widget.cpp
            
            src/ui/include/ui/minimal_waveform_widget.hpp
            src/ui/include/ui/minimal_audio_track_widget.hpp
            src/ui/include/ui/minimal_audio_meters_widget.hpp
        )
        
        target_include_directories(ve_ui_minimal
            PUBLIC src/ui/include
            PRIVATE src/ui/src
            PRIVATE ${CMAKE_SOURCE_DIR}/src/core/include
            PRIVATE ${CMAKE_SOURCE_DIR}/src/audio/include
        )
        
        target_link_libraries(ve_ui_minimal
            PUBLIC
                ve_core
                Qt6::Core
                Qt6::Widgets
                Qt6::Concurrent
        )
        
        add_executable(week8_integration_test
            week8_integration_test.cpp
        )
        
        target_link_libraries(week8_integration_test
            PRIVATE
            ve_ui_minimal
            ve_core
            Qt6::Core
            Qt6::Widgets
        )
        
        target_include_directories(week8_integration_test
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
        )
        
        # Disable warnings for this test executable
        if(MSVC)
            target_compile_options(week8_integration_test PRIVATE /W0)
        endif()
        
        # Simple audio widget runtime test
        add_executable(simple_audio_widget_test
            simple_audio_widget_test.cpp
        )
        
        target_link_libraries(simple_audio_widget_test
            PRIVATE
            ve_ui_minimal
            ve_core
            Qt6::Core
            Qt6::Widgets
        )
        
        target_include_directories(simple_audio_widget_test
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
        )
        
        # Disable warnings for this test executable
        if(MSVC)
            target_compile_options(simple_audio_widget_test PRIVATE /W0)
        endif()
        
        message(STATUS "Week 8 Integration Test Application configured")
    else()
        message(STATUS "Skipping Week 8 Integration Test - Qt6 not found")
    endif()
else()
    message(STATUS "Skipping Week 8 Integration Test - UI module not available")
endif()

set(CPACK_PACKAGE_NAME "VideoEditor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
include(CPack)
