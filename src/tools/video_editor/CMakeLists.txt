cmake_minimum_required(VERSION 3.25)

# Only build video_editor if Qt is available and enabled
if(NOT ENABLE_QT_TOOLS)
    message(STATUS "Skipping video_editor: ENABLE_QT_TOOLS is OFF")
    return()
endif()

# Find Qt6
find_package(Qt6 QUIET COMPONENTS Core Widgets)
if(NOT Qt6_FOUND)
    message(WARNING "Qt6 not found, skipping video_editor")
    return()
endif()

# Video Editor main application
add_executable(video_editor
    main.cpp
)

target_link_libraries(video_editor
    PRIVATE
        ve_app
        ve_core
        Qt6::Core
        Qt6::Widgets
)

# Add Windows D3D11 libraries for hardware acceleration
if(WIN32 AND ENABLE_D3D11VA)
    target_link_libraries(video_editor PRIVATE d3d11 dxgi)
endif()

target_compile_features(video_editor PRIVATE cxx_std_20)

# Set the executable as the startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT video_editor)

# Deploy Qt6 DLLs automatically (Windows only)
if(WIN32)
    # Copy Qt6 core DLLs to the output directory (different for Debug/Release)
    add_custom_command(TARGET video_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/Qt6Cored.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/Qt6Core.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/Qt6Guid.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/Qt6Gui.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/Qt6Widgetsd.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/Qt6Widgets.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/Qt6Networkd.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/Qt6Network.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/Qt6DBusd.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/Qt6DBus.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/icudtd74.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/icudt74.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/icuind74.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/icuin74.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/icuucd74.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/icuuc74.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/pcre2-16d.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/pcre2-16.dll>"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/double-conversion.dll"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/zlibd1.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/zlib1.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/harfbuzz.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/harfbuzz.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/freetyped.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/freetype.dll>"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/brotlicommon.dll"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/brotlidec.dll"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/libpng16d.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libpng16.dll>"
            $<TARGET_FILE_DIR:video_editor>
        COMMENT "Copying Qt6 core DLLs to output directory"
    )
    
    # Copy additional required DLLs (Debug/Release versions)
    add_custom_command(TARGET video_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/fmtd.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/fmt.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/spdlogd.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/spdlog.dll>"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/avcodec-61.dll"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/avformat-61.dll"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/avutil-59.dll"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/swscale-8.dll"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/swresample-5.dll"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/avfilter-10.dll"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/avdevice-61.dll"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/bz2d.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/bz2.dll>"
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/lz4d.dll,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/lz4.dll>"
            "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/zstd.dll"
            $<TARGET_FILE_DIR:video_editor>
        COMMENT "Copying additional DLLs to output directory"
    )
    
    # Create platforms directory and copy Qt6 platform plugins (Debug/Release versions)
    # Use vcpkg-specific paths that we know work
    set(QT_DEBUG_PLUGINS_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/Qt6/plugins")
    set(QT_RELEASE_PLUGINS_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/Qt6/plugins")

    add_custom_command(TARGET video_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:video_editor>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${QT_DEBUG_PLUGINS_DIR}/platforms/qwindowsd.dll,${QT_RELEASE_PLUGINS_DIR}/platforms/qwindows.dll>"
            "$<IF:$<CONFIG:Debug>,${QT_DEBUG_PLUGINS_DIR}/platforms/qminimald.dll,${QT_RELEASE_PLUGINS_DIR}/platforms/qminimal.dll>"
            $<TARGET_FILE_DIR:video_editor>/platforms/
        COMMENT "Copying Qt6 platform plugins to output directory"
    )
endif()
