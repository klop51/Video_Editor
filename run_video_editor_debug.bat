@echo off
set "ROOT=%~dp0"
echo ========================================
echo Qt6 Video Editor Debug Launcher
echo ========================================
echo.

REM Resolve target executable (qt-debug then simple-debug)
set "VIDEO_EDITOR_PATH=%ROOT%build\qt-debug\src\tools\video_editor\Debug\video_editor.exe"
if not exist "%VIDEO_EDITOR_PATH%" set "VIDEO_EDITOR_PATH=%ROOT%build\simple-debug\src\tools\video_editor\Debug\video_editor.exe"

if not exist "%VIDEO_EDITOR_PATH%" (
	echo ERROR: video_editor.exe not found.
	echo Tried:
	echo   %ROOT%build\qt-debug\src\tools\video_editor\Debug\video_editor.exe
	echo   %ROOT%build\simple-debug\src\tools\video_editor\Debug\video_editor.exe
	echo.
	echo Build with one of:
	echo   cmake --build build\qt-debug --config Debug --target video_editor
	echo   cmake --build build\simple-debug --config Debug --target video_editor
	echo.
	pause
	exit /b 1
)

for %%I in ("%VIDEO_EDITOR_PATH%") do set "VIDEO_EDITOR_DIR=%%~dpI"

REM Prepend vcpkg bins (debug first)
set "PATH=%ROOT%vcpkg_installed\x64-windows\debug\bin;%ROOT%vcpkg_installed\x64-windows\bin;%PATH%"

REM Ensure Windows SDK tools (rc.exe, mt.exe) are available on PATH for builds
where rc.exe >nul 2>&1
if errorlevel 1 (
    echo Searching for Windows SDK rc.exe...
    set "_WSDK_RC_DIR="
    for /R "C:\Program Files (x86)\Windows Kits\10\bin" %%F in (rc.exe) do (
        set "_WSDK_RC_DIR=%%~dpF"
        goto :wsdk_found_rc
    )
    :wsdk_found_rc
    if defined _WSDK_RC_DIR (
        echo Adding Windows SDK bin to PATH: %_WSDK_RC_DIR%
        set "PATH=%_WSDK_RC_DIR%;%PATH%"
    ) else (
        echo WARNING: Windows SDK rc.exe not found. Install Windows 10/11 SDK.
    )
)

where mt.exe >nul 2>&1
if errorlevel 1 (
    echo Searching for Windows SDK mt.exe...
    set "_WSDK_MT_DIR="
    for /R "C:\Program Files (x86)\Windows Kits\10\bin" %%F in (mt.exe) do (
        set "_WSDK_MT_DIR=%%~dpF"
        goto :wsdk_found_mt
    )
    :wsdk_found_mt
    if defined _WSDK_MT_DIR (
        echo Adding Windows SDK bin to PATH: %_WSDK_MT_DIR%
        set "PATH=%_WSDK_MT_DIR%;%PATH%"
    ) else (
        echo WARNING: Windows SDK mt.exe not found. Install Windows 10/11 SDK.
    )
)

REM Simple Qt DLL presence note without nested blocks
if exist "%VIDEO_EDITOR_DIR%Qt6Cored.dll" echo Found Qt debug DLLs beside executable.
if not exist "%VIDEO_EDITOR_DIR%Qt6Cored.dll" if exist "%VIDEO_EDITOR_DIR%Qt6Core.dll" echo Found Qt release DLLs beside executable.
if not exist "%VIDEO_EDITOR_DIR%Qt6Cored.dll" if not exist "%VIDEO_EDITOR_DIR%Qt6Core.dll" echo Qt DLLs not beside executable - relying on PATH.

where vcruntime140d.dll >nul 2>&1
if errorlevel 1 echo WARNING: vcruntime140d.dll missing from PATH - debug runtime likely missing.

if defined VE_DUMP_FRAMES echo VE_DUMP_FRAMES=%VE_DUMP_FRAMES%

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
