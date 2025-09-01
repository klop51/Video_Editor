add_library(ve_media_io STATIC
    src/media_probe.cpp
    src/demuxer.cpp
    src/format_detector.cpp  # Phase 1 Week 1: Professional format detection system
    src/prores_support.cpp   # Phase 1 Week 2: ProRes professional codec support
    src/dnxhd_support.cpp    # Phase 1 Week 3: DNxHD/DNxHR broadcast codec support
    src/modern_codec_support.cpp  # Phase 1 Week 4: Modern codec support (AV1, HEVC, VP9)
    src/modern_codec_format_integration.cpp  # Phase 1 Week 4: Modern codec integration
    src/hdr_infrastructure.cpp  # Phase 2 Week 5: HDR Infrastructure and processing
    src/hdr_utilities.cpp       # Phase 2 Week 5: HDR Workflow utilities
    src/log_format_support.cpp  # Phase 2 Week 6: Log Format Support for color grading
    src/high_bitdepth_support.cpp  # Phase 2 Week 7: High Bit Depth Pipeline
    src/color_management.cpp    # Phase 2 Week 8: Color Management Integration
    src/high_resolution_support.cpp  # Phase 3 Week 9: 8K Support Infrastructure
    src/raw_format_support.cpp  # Phase 3 Week 10: RAW Format Foundation
    src/container_formats.cpp   # Phase 3 Week 11: Container Format Expansion
    src/performance_optimizer.cpp  # Phase 3 Week 12: Performance Optimization
)

add_library(VideoEditor::media_io ALIAS ve_media_io)
add_library(media_io ALIAS ve_media_io)

target_include_directories(ve_media_io
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core/include
        ${CMAKE_SOURCE_DIR}/src/decode/include  # For format_detector.hpp dependency
)

if(ENABLE_FFMPEG)
    # Add vcpkg FFmpeg find module to path
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/share/ffmpeg")
    
    # Try to find FFmpeg using vcpkg's CMake config files first
    find_package(PkgConfig QUIET)
    
    if(PkgConfig_FOUND)
        # Use pkg-config on Unix systems
        pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
        pkg_check_modules(AVCODEC  REQUIRED IMPORTED_TARGET libavcodec)
        pkg_check_modules(AVUTIL   REQUIRED IMPORTED_TARGET libavutil)
        pkg_check_modules(SWSCALE  REQUIRED IMPORTED_TARGET libswscale)
        
        set(FFMPEG_LIBRARIES 
            PkgConfig::AVFORMAT 
            PkgConfig::AVCODEC 
            PkgConfig::AVUTIL 
            PkgConfig::SWSCALE)
    else()
        # Use vcpkg's FindFFMPEG module on Windows
        find_package(FFMPEG REQUIRED)
        set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES})
    endif()

    target_compile_definitions(ve_media_io PUBLIC VE_HAVE_FFMPEG=1)

    # Link against FFmpeg libraries
    target_link_libraries(ve_media_io
        PUBLIC
            ${FFMPEG_LIBRARIES}
    )

    # Relax warnings for external headers on MSVC
    if(MSVC)
        target_compile_options(ve_media_io PRIVATE /W3)
    endif()
else()
    target_compile_definitions(ve_media_io PUBLIC VE_HAVE_FFMPEG=0)
endif()

target_link_libraries(ve_media_io PUBLIC VideoEditor::core ve_core)

install(TARGETS ve_media_io EXPORT VideoEditorTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
