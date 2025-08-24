add_library(ve_decode STATIC
    src/decoder.cpp
    src/video_decoder_ffmpeg.cpp
    src/color_convert.cpp
    src/async_decoder.cpp
    src/audio_resampler.cpp
    src/playback_controller.cpp
    src/gpu_upload_stub.cpp
)

add_library(VideoEditor::decode ALIAS ve_decode)
add_library(decode ALIAS ve_decode)

target_include_directories(ve_decode
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core/include
)

target_link_libraries(ve_decode
    PUBLIC 
        VideoEditor::core 
        VideoEditor::media_io
        ve_core
        ve_media_io
)

if(ENABLE_FFMPEG)
    target_compile_definitions(ve_decode PUBLIC VE_HAVE_FFMPEG=1)
    # Disable warnings as errors for FFmpeg headers on MSVC
    if(MSVC)
        target_compile_options(ve_decode PRIVATE /W3)
    endif()
else()
    target_compile_definitions(ve_decode PUBLIC VE_HAVE_FFMPEG=0)
endif()

install(TARGETS ve_decode EXPORT VideoEditorTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
