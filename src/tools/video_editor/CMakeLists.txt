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

# Disable vcpkg's problematic applocal.ps1 that causes DLL file locking issues
set_target_properties(video_editor PROPERTIES VCPKG_APPLOCAL_DEPS OFF)

# Deploy Qt6 DLLs using our custom robust script (Windows only)
if(WIN32)
    # Use our custom PowerShell script that handles file locking properly
    # Use explicit PowerShell path for CI environment compatibility
    add_custom_command(TARGET video_editor POST_BUILD
        COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe" 
            -NoProfile -ExecutionPolicy Bypass 
            -File "${CMAKE_SOURCE_DIR}/scripts/deploy-dlls-simple.ps1"
            -TargetBinary "$<TARGET_FILE:video_editor>"
            -VcpkgInstalledDir "${CMAKE_SOURCE_DIR}/vcpkg_installed"
            -Configuration "$<CONFIG>"
        COMMENT "Deploying DLLs with simple robust script (bypassing vcpkg applocal.ps1)"
        VERBATIM
    )
endif()
