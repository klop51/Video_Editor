find_package(Qt6 COMPONENTS Widgets QUIET)

if(NOT Qt6_FOUND)
    message(WARNING "Qt6 not found, skipping qt_preview")
    return()
endif()

qt_standard_project_setup()

qt_add_executable(qt_preview
    src/main.cpp
    src/player_window.cpp
)

target_link_libraries(qt_preview PRIVATE Qt6::Widgets ve_decode ve_media_io ve_core)

set_target_properties(qt_preview PROPERTIES OUTPUT_NAME "ve_qt_preview")

# Disable vcpkg's problematic applocal.ps1 that causes DLL file locking issues
set_target_properties(qt_preview PROPERTIES VCPKG_APPLOCAL_DEPS OFF)

# Deploy Qt6 DLLs using our custom robust script (Windows only)
if(WIN32)
    # Use our custom PowerShell script that handles file locking properly
    # Use explicit PowerShell path for CI environment compatibility
    add_custom_command(TARGET qt_preview POST_BUILD
        COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe" 
            -NoProfile -ExecutionPolicy Bypass 
            -File "${CMAKE_SOURCE_DIR}/scripts/deploy-dlls-simple.ps1"
            -TargetBinary "$<TARGET_FILE:qt_preview>"
            -VcpkgInstalledDir "${CMAKE_SOURCE_DIR}/vcpkg_installed"
            -Configuration "$<CONFIG>"
        COMMENT "Deploying DLLs with simple robust script (bypassing vcpkg applocal.ps1)"
        VERBATIM
    )
endif()
