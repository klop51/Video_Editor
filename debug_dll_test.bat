@echo off
echo ========================================
echo     Debug DLL Dependencies Test
echo ========================================
echo.

set "VIDEO_EDITOR_PATH=%~dp0build\qt-debug\src\tools\video_editor\Debug\video_editor.exe"

echo Checking executable exists...
if not exist "%VIDEO_EDITOR_PATH%" (
    echo ERROR: Video editor not found at %VIDEO_EDITOR_PATH%
    pause
    exit /b 1
)
echo Found: %VIDEO_EDITOR_PATH%
echo.

echo Checking required DLLs in executable directory...
pushd "%~dp0build\qt-debug\src\tools\video_editor\Debug"

echo Qt6 Core DLLs:
if exist "Qt6Cored.dll" (echo ✓ Qt6Cored.dll) else (echo ✗ Qt6Cored.dll MISSING)
if exist "Qt6Guid.dll" (echo ✓ Qt6Guid.dll) else (echo ✗ Qt6Guid.dll MISSING)
if exist "Qt6Widgetsd.dll" (echo ✓ Qt6Widgetsd.dll) else (echo ✗ Qt6Widgetsd.dll MISSING)

echo.
echo FFmpeg DLLs:
if exist "avcodec-61.dll" (echo ✓ avcodec-61.dll) else (echo ✗ avcodec-61.dll MISSING)
if exist "avformat-61.dll" (echo ✓ avformat-61.dll) else (echo ✗ avformat-61.dll MISSING)
if exist "avutil-59.dll" (echo ✓ avutil-59.dll) else (echo ✗ avutil-59.dll MISSING)

echo.
echo System DLLs check:
where vcruntime140d.dll >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo ✓ vcruntime140d.dll found in PATH
) else (
    echo ✗ vcruntime140d.dll NOT FOUND - this is likely the problem!
    echo   Install Visual Studio 2022 C++ Redistributable (Debug)
)

where msvcp140d.dll >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo ✓ msvcp140d.dll found in PATH
) else (
    echo ✗ msvcp140d.dll NOT FOUND
)

echo.
echo Adding vcpkg to PATH...
set "PATH=%~dp0vcpkg_installed\x64-windows\bin;%~dp0vcpkg_installed\x64-windows\debug\bin;%PATH%"

echo.
echo Attempting to launch video editor...
video_editor.exe
set EXIT_CODE=%ERRORLEVEL%

echo.
echo Exit code: %EXIT_CODE%
if %EXIT_CODE% equ -1073741515 (
    echo This is DLL_NOT_FOUND error - check the missing DLLs above
)

popd
pause
