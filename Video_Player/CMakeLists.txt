cmake_minimum_required(VERSION 3.16)
project(VideoPlayer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find pkg-config
find_package(PkgConfig REQUIRED)

# Find SDL2
pkg_check_modules(SDL2 REQUIRED sdl2)

# Find FFmpeg
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

# Find ImGui
find_package(imgui CONFIG REQUIRED)

# Add executable
add_executable(video_player
    src/main.cpp
    src/VideoPlayer.cpp
    src/AudioManager.cpp
    third_party/imgui_backends/imgui_impl_sdl2.cpp
    third_party/imgui_backends/imgui_impl_sdlrenderer2.cpp
)

# Include directories
target_include_directories(video_player PRIVATE ${SDL2_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS} third_party/imgui_backends)

# Link directories
target_link_directories(video_player PRIVATE ${SDL2_LIBRARY_DIRS} ${FFMPEG_LIBRARY_DIRS})

# Link libraries
target_link_libraries(video_player ${SDL2_LIBRARIES} SDL2main ${FFMPEG_LIBRARIES} imgui::imgui)

# Copy SDL2.dll to output directory on Windows
if(WIN32)
    find_file(SDL2_DLL SDL2.dll PATHS ${SDL2_LIBRARY_DIRS} NO_DEFAULT_PATH)
    if(SDL2_DLL)
        add_custom_command(TARGET video_player POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDL2_DLL}
            $<TARGET_FILE_DIR:video_player>
        )
    endif()
endif()