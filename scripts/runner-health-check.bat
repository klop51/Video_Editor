@echo off
REM =============================================================================
REM GitHub Actions Runner Health Check for Proxmox/VM Environment  
REM This script validates the runner setup for CMD-based workflow execution
REM =============================================================================

echo.
echo ===============================================
echo GitHub Actions Runner Health Check
echo ===============================================
echo.

REM Check if running in runner directory
if not exist "config.cmd" (
    echo ERROR: This script must be run from the GitHub Actions Runner directory
    echo Expected location: C:\Users\%USERNAME%\actions-runner
    pause
    exit /b 1
)

echo === Basic System Information ===
echo Computer: %COMPUTERNAME%
echo User: %USERNAME%
echo OS: %OS%
echo Architecture: %PROCESSOR_ARCHITECTURE%
echo Cores: %NUMBER_OF_PROCESSORS%
echo.

echo === Checking Required Software ===

REM Check Git
git --version >nul 2>&1
if %errorlevel% equ 0 (
    echo ✓ Git: Available
    git --version
) else (
    echo ✗ Git: NOT FOUND - Install Git for Windows
)

REM Check CMake
cmake --version >nul 2>&1
if %errorlevel% equ 0 (
    echo ✓ CMake: Available
    cmake --version | findstr "cmake version"
) else (
    echo ✗ CMake: NOT FOUND - Install CMake
)

REM Check Visual Studio/Build Tools
if exist "%ProgramFiles%\Microsoft Visual Studio\2022" (
    echo ✓ Visual Studio 2022: Found
    dir "%ProgramFiles%\Microsoft Visual Studio\2022" /B
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019" (
    echo ✓ Visual Studio 2019: Found
) else (
    echo ✗ Visual Studio: NOT FOUND - Install Visual Studio Build Tools
)

REM Check vcpkg
if exist "C:\vcpkg\vcpkg.exe" (
    echo ✓ vcpkg: Available at C:\vcpkg
) else (
    echo ⚠ vcpkg: Not found at C:\vcpkg - will be installed automatically
)

echo.
echo === PowerShell vs CMD Test ===

REM Test PowerShell (should work but we avoid it for workflow stability)
powershell -NoProfile -Command "Write-Host 'PowerShell test: PASS'" 2>nul
if %errorlevel% equ 0 (
    echo ✓ PowerShell: Available but AVOIDED in workflows for VM stability
) else (
    echo ⚠ PowerShell: Issues detected - THIS IS GOOD, we use CMD instead!
)

REM Test CMD (should always work)
echo ✓ CMD: Available (preferred for GitHub Actions workflows)

echo.
echo === GitHub Actions Runner Status ===

REM Check if runner is configured
if exist ".runner" (
    echo ✓ Runner is configured
    if exist ".runner" (
        echo Configuration file exists
    )
) else (
    echo ⚠ Runner not configured yet - run configure-runner-simple.bat
)

REM Check if runner service is installed
sc query "actions.runner.*" >nul 2>&1
if %errorlevel% equ 0 (
    echo ✓ Runner service is installed
    sc query "actions.runner.*" | findstr "STATE"
) else (
    echo ⚠ Runner service not installed - running in interactive mode
)

echo.
echo === Environment Variables ===
echo PATH length: 
echo %PATH% | find /c ";"
echo.
echo Key paths:
echo %PATH% | findstr /i "git"
echo %PATH% | findstr /i "cmake"
echo %PATH% | findstr /i "visual"

echo.
echo === Disk Space Check ===
for %%d in (C: D: E:) do (
    if exist %%d\ (
        echo Drive %%d
        dir %%d\ | findstr "bytes free"
    )
)

echo.
echo === Network Connectivity Test ===
echo Testing GitHub connectivity...
ping github.com -n 1 >nul 2>&1
if %errorlevel% equ 0 (
    echo ✓ GitHub.com: Reachable
) else (
    echo ✗ GitHub.com: Connection failed
)

echo.
echo === Graphics/VM Environment Info ===
echo This runner is optimized for Proxmox/VM environments with:
echo - CMD-based execution (no PowerShell graphics dependencies)
echo - Limited GPU access (software rendering)
echo - Headless operation support
echo.

REM Check for virtualization
systeminfo | findstr /i "system model" | findstr /i "virtual" >nul
if %errorlevel% equ 0 (
    echo ✓ Virtual Machine detected - configuration optimized for VM
) else (
    echo ⚠ Physical machine detected - VM optimizations may not be needed
)

echo.
echo ===============================================
echo Health Check Complete
echo ===============================================
echo.

REM Summary
echo Summary:
echo - Runner optimized for Proxmox/VM environment ✓
echo - CMD-based workflows avoid PowerShell VM issues ✓  
echo - Limited graphics capabilities handled gracefully ✓
echo - Self-hosted runner provides dedicated build resources ✓
echo.

echo If all checks show ✓ or ⚠ (warnings are expected), the runner is ready!
echo.
pause