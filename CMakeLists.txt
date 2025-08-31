cmake_minimum_required(VERSION 3.25)
project(VideoEditor VERSION 0.1.0 LANGUAGES CXX)

# Options
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(ENABLE_TOOLS "Build developer / CLI tools" ON)
option(ENABLE_QT_TOOLS "Build Qt preview tools" ON)
option(ENABLE_TESTS "Build tests" ON)
option(ENABLE_FFMPEG "Enable FFmpeg integration" ON)
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
endif()

set(CPACK_PACKAGE_NAME "VideoEditor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
include(CPack)
