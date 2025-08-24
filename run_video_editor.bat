@echo off
echo ========================================
echo      Qt6 Video Editor Launcher
echo ========================================
echo.

REM Check if video editor executable exists (Release version)
set "VIDEO_EDITOR_PATH=%~dp0build\simple-debug\src\tools\video_editor\Release\video_editor.exe"
if not exist "%VIDEO_EDITOR_PATH%" (
    echo ERROR: Video editor executable not found!
    echo Expected location: %VIDEO_EDITOR_PATH%
    echo.
    echo Please build the project first using:
    echo cmake --build build\simple-debug --config Release --target video_editor
    echo.
    pause
    exit /b 1
)

REM Check if Qt6 DLLs are bundled, if not add vcpkg to PATH
set "QT6_DLL_PATH=%~dp0build\simple-debug\src\tools\video_editor\Release\Qt6Core.dll"
if not exist "%QT6_DLL_PATH%" (
    echo Qt6 DLLs not bundled, adding vcpkg bin to PATH...
    set "PATH=%PATH%;%~dp0vcpkg_installed\x64-windows\bin"
    echo PATH updated.
    echo.
)

REM Launch the video editor
echo Starting Video Editor...
echo.

"%VIDEO_EDITOR_PATH%"

if %ERRORLEVEL% equ 0 (
    echo.
    echo Video Editor closed successfully.
) else (
    echo.
    echo Video Editor exited with error code: %ERRORLEVEL%
    echo This may be due to missing dependencies.
)

echo.
pause
