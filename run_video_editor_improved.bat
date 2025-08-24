@echo off
echo ========================================
echo  Qt6 Video Editor - UI Improved Version
echo ========================================
echo.
echo This version includes UI responsiveness improvements:
echo - Chunked timeline processing (no more freezes)
echo - Optimized drawing with viewport culling
echo - Background worker threads for media processing
echo - Performance profiling for bottleneck detection
echo.

REM Check if improved video editor executable exists (Debug version with improvements)
set "VIDEO_EDITOR_PATH=%~dp0build\qt-debug\src\tools\video_editor\Debug\video_editor.exe"
if not exist "%VIDEO_EDITOR_PATH%" (
    echo ERROR: Improved video editor executable not found!
    echo Expected location: %VIDEO_EDITOR_PATH%
    echo.
    echo Please build the project first using:
    echo cmake -S . -B build/qt-debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_QT_TOOLS=ON -DCMAKE_PREFIX_PATH="C:\Users\braul\Desktop\Video_Editor\vcpkg_installed\x64-windows"
    echo cmake --build build/qt-debug --config Debug
    echo.
    pause
    exit /b 1
)

REM Add vcpkg paths to ensure all dependencies are found
echo Adding vcpkg and system paths...
set "PATH=%~dp0vcpkg_installed\x64-windows\bin;%~dp0vcpkg_installed\x64-windows\debug\bin;%PATH%"

REM Check if Qt6 DLLs are bundled
set "QT6_DLL_PATH=%~dp0build\qt-debug\src\tools\video_editor\Debug\Qt6Cored.dll"
if not exist "%QT6_DLL_PATH%" (
    echo WARNING: Qt6 DLLs not found in build directory!
    echo Relying on vcpkg PATH for DLL loading...
    echo.
) else (
    echo Qt6 DLLs found in build directory.
    echo.
)

REM Check for Visual C++ Runtime (common cause of -1073741515 error)
where vcruntime140d.dll >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo WARNING: Visual C++ Debug Runtime may not be available
    echo This could cause DLL loading errors
    echo.
)

REM Launch the improved video editor
echo Starting Video Editor (Improved UI Responsiveness)...
echo.
echo Watch the console for performance profiling output:
echo - Operations taking ^>16ms will be logged
echo - Paint events timing will be shown
echo - Timeline processing will show chunking behavior
echo.

echo Adding vcpkg to PATH for DLL resolution...
set "PATH=%~dp0vcpkg_installed\x64-windows\bin;%~dp0vcpkg_installed\x64-windows\debug\bin;%PATH%"

REM Change to the executable directory for proper DLL loading
echo Changing to executable directory...
pushd "%~dp0build\qt-debug\src\tools\video_editor\Debug"

REM Launch with full console output
echo Launching video_editor.exe...
video_editor.exe
set EXIT_CODE=%ERRORLEVEL%
popd

echo.
if %EXIT_CODE% equ 0 (
    echo Video Editor closed successfully.
) else (
    echo Video Editor exited with error code: %EXIT_CODE%
    echo.
    echo If you see DLL errors, run debug_dll_test.bat to diagnose
)

echo.
echo Testing Notes:
echo - Try importing media and adding to timeline
echo - The UI should remain responsive during processing
echo - No more "Not Responding" freezes should occur
echo.
pause
