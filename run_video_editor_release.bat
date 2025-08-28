@echo off
set "ROOT=%~dp0"
echo ========================================
echo Qt6 Video Editor Release Launcher
echo ========================================
echo.

REM Resolve target executable (prefer VS Release, fallback to dev-release Ninja)
set "VIDEO_EDITOR_PATH=%ROOT%build\vs-release\src\tools\video_editor\Release\video_editor.exe"
if not exist "%VIDEO_EDITOR_PATH%" set "VIDEO_EDITOR_PATH=%ROOT%build\dev-release\src\tools\video_editor\video_editor.exe"

if not exist "%VIDEO_EDITOR_PATH%" (
	echo ERROR: video_editor.exe not found.
	echo Tried:
	echo   %ROOT%build\vs-release\src\tools\video_editor\Release\video_editor.exe
	echo   %ROOT%build\dev-release\src\tools\video_editor\video_editor.exe
	echo.
	echo Build with one of:
	echo   cmake --preset vs-release ^&^& cmake --build --preset vs-release --config Release --target video_editor
	echo   cmake --preset dev-release ^&^& cmake --build --preset dev-release --target video_editor
	echo.
	pause
	exit /b 1
)

for %%I in ("%VIDEO_EDITOR_PATH%") do set "VIDEO_EDITOR_DIR=%%~dpI"

REM Prepend vcpkg bins (release first)
set "PATH=%ROOT%vcpkg_installed\x64-windows\bin;%ROOT%vcpkg_installed\x64-windows\debug\bin;%PATH%"

echo Launching: %VIDEO_EDITOR_PATH%
pushd "%VIDEO_EDITOR_DIR%" >nul
"%VIDEO_EDITOR_PATH%" %*
set EXIT_CODE=%ERRORLEVEL%
popd >nul

echo.
if "%EXIT_CODE%"=="0" echo Video Editor closed successfully.
if not "%EXIT_CODE%"=="0" echo Video Editor exited with code %EXIT_CODE%
if not "%EXIT_CODE%"=="0" echo -1073741515 0xC0000135 missing DLL
if not "%EXIT_CODE%"=="0" echo -1073741819 0xC0000005 access violation
if not "%EXIT_CODE%"=="0" echo -1073740791 0xC0000409 stack buffer / invalid parameter
echo.
pause

