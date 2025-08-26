@echo off
echo ========================================
echo    Qt6 Video Editor Debug Launcher
echo ========================================
echo.

REM Prefer qt-debug build; fall back to simple-debug
set "VIDEO_EDITOR_PATH=%~dp0build\qt-debug\src\tools\video_editor\Debug\video_editor.exe"
if not exist "%VIDEO_EDITOR_PATH%" (
    set "VIDEO_EDITOR_PATH=%~dp0build\simple-debug\src\tools\video_editor\Debug\video_editor.exe"
)

if not exist "%VIDEO_EDITOR_PATH%" (
    echo ERROR: video_editor.exe not found in qt-debug or simple-debug builds.
    echo Looked for:
    echo   %~dp0build\qt-debug\src\tools\video_editor\Debug\video_editor.exe
    echo   %~dp0build\simple-debug\src\tools\video_editor\Debug\video_editor.exe
    echo.
    echo Build one with for example:
    echo   cmake --build build\qt-debug --config Debug --target video_editor
    echo or:
    echo   cmake --build build\simple-debug --config Debug --target video_editor
    echo.
    pause
    exit /b 1
)

REM Check if Qt6 DLLs are bundled, if not add vcpkg to PATH
for %%I in ("%VIDEO_EDITOR_PATH%") do set "VIDEO_EDITOR_DIR=%%~dpI"
set "QT6_DLL_PATH=%VIDEO_EDITOR_DIR%Qt6Core.dll"
if not exist "%QT6_DLL_PATH%" (
    echo Qt6 DLLs not bundled, adding vcpkg bin to PATH...
    set "PATH=%PATH%;%~dp0vcpkg_installed\x64-windows\bin"
    echo PATH updated.
    echo.
)

REM Launch the debug video editor
echo Starting Video Editor (Debug Build)...
echo Executable: %VIDEO_EDITOR_PATH%
echo Note: Debug output will be shown in the console
echo.

"%VIDEO_EDITOR_PATH%"

if %ERRORLEVEL% equ 0 (
    echo.
    echo Video Editor closed successfully.
) else (
    echo.
    echo Video Editor exited with error code: %ERRORLEVEL%
    echo This may be due to missing dependencies or debugging assertions.
)

echo.
pause
