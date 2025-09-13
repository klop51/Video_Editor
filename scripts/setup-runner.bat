@echo off
REM Self-Hosted Windows Runner Setup (Batch Alternative)
REM This script works without changing PowerShell execution policies

echo Setting up Windows Runner for Video Editor...

REM Check if running as Administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: Please run as Administrator
    pause
    exit /b 1
)

REM Set environment variables
echo Setting environment variables...
setx VCPKG_ROOT "C:\vcpkg" /M
setx VCPKG_DEFAULT_TRIPLET "x64-windows" /M
setx VE_ENABLE_DETAILED_PROFILING "1" /M
setx VE_ENABLE_PBO_UPLOAD "1" /M
setx CMAKE_TOOLCHAIN_FILE "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" /M

REM Add to system PATH
echo Adding tools to PATH...
for %%P in ("C:\vcpkg" "C:\Program Files\CMake\bin" "C:\Program Files\Git\bin") do (
    echo Adding %%P to PATH...
    setx PATH "%PATH%;%%P" /M >nul 2>&1
)

REM Check vcpkg installation
if not exist "C:\vcpkg\vcpkg.exe" (
    echo Installing vcpkg...
    if exist "C:\vcpkg" rmdir /s /q "C:\vcpkg"
    git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    cd /d C:\vcpkg
    call bootstrap-vcpkg.bat
    vcpkg integrate install
    cd /d %~dp0
) else (
    echo vcpkg already installed
)

REM Verify installations
echo.
echo Verifying installations...

where cmake >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] CMake found
) else (
    echo [ERROR] CMake not found in PATH
)

where git >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Git found
) else (
    echo [ERROR] Git not found in PATH
)

if exist "C:\vcpkg\vcpkg.exe" (
    echo [OK] vcpkg found
) else (
    echo [ERROR] vcpkg not found
)

REM Check Visual Studio
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" (
    echo [OK] Visual Studio 2022 Community found
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    echo [OK] Visual Studio installer found
) else (
    echo [WARNING] Visual Studio 2022 not found - please install manually
)

echo.
echo Setup completed! Please restart your command prompt to reload environment variables.
echo.
echo Next steps:
echo 1. Install GitHub Actions runner from your repository settings
echo 2. Configure runner with labels: [self-hosted, windows]
echo 3. Test with a workflow run
echo.
pause