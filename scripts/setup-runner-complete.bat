@echo off
setlocal enabledelayedexpansion

REM =============================================================================
REM Complete GitHub Actions Runner Setup Script
REM This batch file downloads and configures everything needed for the runner
REM Can be copied to any Windows server and executed independently
REM =============================================================================

echo.
echo ===============================================
echo GitHub Actions Runner Complete Setup
echo ===============================================
echo.

REM Check if running as Administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Set working directory
set "SETUP_DIR=%USERPROFILE%\runner-setup"
set "RUNNER_DIR=%USERPROFILE%\actions-runner"
set "DOWNLOADS_DIR=%SETUP_DIR%\downloads"

echo Creating setup directories...
if not exist "%SETUP_DIR%" mkdir "%SETUP_DIR%"
if not exist "%DOWNLOADS_DIR%" mkdir "%DOWNLOADS_DIR%"
if not exist "%RUNNER_DIR%" mkdir "%RUNNER_DIR%"

cd /d "%SETUP_DIR%"

echo.
echo ===============================================
echo Step 1: Download Required Tools
echo ===============================================

REM Download PowerShell 7 (more stable than Windows PowerShell)
echo Downloading PowerShell 7...
set "PS7_URL=https://github.com/PowerShell/PowerShell/releases/download/v7.4.5/PowerShell-7.4.5-win-x64.msi"
set "PS7_FILE=%DOWNLOADS_DIR%\PowerShell-7.4.5-win-x64.msi"

if not exist "%PS7_FILE%" (
    powershell -Command "Invoke-WebRequest -Uri '%PS7_URL%' -OutFile '%PS7_FILE%'"
    if !errorlevel! neq 0 (
        echo ERROR: Failed to download PowerShell 7
        pause
        exit /b 1
    )
)

REM Download Git for Windows
echo Downloading Git for Windows...
set "GIT_URL=https://github.com/git-for-windows/git/releases/download/v2.46.0.windows.1/Git-2.46.0-64-bit.exe"
set "GIT_FILE=%DOWNLOADS_DIR%\Git-2.46.0-64-bit.exe"

if not exist "%GIT_FILE%" (
    powershell -Command "Invoke-WebRequest -Uri '%GIT_URL%' -OutFile '%GIT_FILE%'"
    if !errorlevel! neq 0 (
        echo ERROR: Failed to download Git
        pause
        exit /b 1
    )
)

REM Download Visual Studio Build Tools 2022
echo Downloading Visual Studio Build Tools 2022...
set "VS_URL=https://aka.ms/vs/17/release/vs_buildtools.exe"
set "VS_FILE=%DOWNLOADS_DIR%\vs_buildtools.exe"

if not exist "%VS_FILE%" (
    powershell -Command "Invoke-WebRequest -Uri '%VS_URL%' -OutFile '%VS_FILE%'"
    if !errorlevel! neq 0 (
        echo ERROR: Failed to download Visual Studio Build Tools
        pause
        exit /b 1
    )
)

REM Download CMake
echo Downloading CMake...
set "CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v3.30.3/cmake-3.30.3-windows-x86_64.msi"
set "CMAKE_FILE=%DOWNLOADS_DIR%\cmake-3.30.3-windows-x86_64.msi"

if not exist "%CMAKE_FILE%" (
    powershell -Command "Invoke-WebRequest -Uri '%CMAKE_URL%' -OutFile '%CMAKE_FILE%'"
    if !errorlevel! neq 0 (
        echo ERROR: Failed to download CMake
        pause
        exit /b 1
    )
)

REM Download GitHub Actions Runner
echo Downloading GitHub Actions Runner...
set "RUNNER_URL=https://github.com/actions/runner/releases/download/v2.319.1/actions-runner-win-x64-2.319.1.zip"
set "RUNNER_FILE=%DOWNLOADS_DIR%\actions-runner-win-x64-2.319.1.zip"

if not exist "%RUNNER_FILE%" (
    powershell -Command "Invoke-WebRequest -Uri '%RUNNER_URL%' -OutFile '%RUNNER_FILE%'"
    if !errorlevel! neq 0 (
        echo ERROR: Failed to download GitHub Actions Runner
        pause
        exit /b 1
    )
)

echo.
echo ===============================================
echo Step 2: Install Software
echo ===============================================

REM Install PowerShell 7
echo Installing PowerShell 7...
msiexec /i "%PS7_FILE%" /quiet /norestart
if !errorlevel! neq 0 (
    echo WARNING: PowerShell 7 installation may have failed
)

REM Install Git
echo Installing Git for Windows...
"%GIT_FILE%" /VERYSILENT /NORESTART /NOCANCEL /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /COMPONENTS="icons,ext\reg\shellhere,assoc,assoc_sh"
if !errorlevel! neq 0 (
    echo WARNING: Git installation may have failed
)

REM Install CMake
echo Installing CMake...
msiexec /i "%CMAKE_FILE%" /quiet /norestart
if !errorlevel! neq 0 (
    echo WARNING: CMake installation may have failed
)

echo.
echo ===============================================
echo Step 3: Install Visual Studio Build Tools
echo ===============================================

echo Installing Visual Studio Build Tools with C++ workload...
echo This may take several minutes...

"%VS_FILE%" --quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --add Microsoft.VisualStudio.Component.VC.CMake.Project

if !errorlevel! neq 0 (
    echo WARNING: Visual Studio Build Tools installation may have failed
    echo You may need to install manually later
)

echo.
echo ===============================================
echo Step 4: Extract GitHub Actions Runner
echo ===============================================

cd /d "%RUNNER_DIR%"
echo Extracting GitHub Actions Runner to %RUNNER_DIR%...

powershell -Command "Expand-Archive -Path '%RUNNER_FILE%' -DestinationPath '%RUNNER_DIR%' -Force"
if !errorlevel! neq 0 (
    echo ERROR: Failed to extract GitHub Actions Runner
    pause
    exit /b 1
)

echo.
echo ===============================================
echo Step 5: Configure Environment
echo ===============================================

REM Update PATH environment variable
echo Updating system PATH...
set "NEW_PATHS=C:\Program Files\PowerShell\7;C:\Program Files\Git\bin;C:\Program Files\CMake\bin"

for %%P in ("%NEW_PATHS:;=" "%") do (
    echo Adding %%P to PATH...
    powershell -Command "[Environment]::SetEnvironmentVariable('PATH', [Environment]::GetEnvironmentVariable('PATH', 'Machine') + ';%%P', 'Machine')"
)

REM Create runner configuration script
echo Creating runner configuration script...
(
echo @echo off
echo echo.
echo echo ===============================================
echo echo Configure GitHub Actions Runner
echo echo ===============================================
echo echo.
echo echo Before running this script, you need:
echo echo 1. Repository URL: https://github.com/OWNER/REPO
echo echo 2. Runner token from GitHub repo Settings ^> Actions ^> Runners ^> New self-hosted runner
echo echo.
echo set /p REPO_URL="Enter repository URL: "
echo set /p RUNNER_TOKEN="Enter runner token: "
echo echo.
echo echo Configuring runner...
echo ./config.cmd --url %%REPO_URL%% --token %%RUNNER_TOKEN%% --name "windows-runner" --work "_work" --runasservice
echo echo.
echo echo Starting runner service...
echo ./run.cmd
) > "%RUNNER_DIR%\configure-runner.bat"

REM Create service installation script
echo Creating service installation script...
(
echo @echo off
echo echo Installing GitHub Actions Runner as Windows Service...
echo echo This requires the runner to be configured first.
echo echo.
echo pause
echo ./svc.sh install
echo ./svc.sh start
echo echo.
echo echo Runner service installed and started.
echo pause
) > "%RUNNER_DIR%\install-service.bat"

echo.
echo ===============================================
echo Step 6: Create Quick Setup Summary
echo ===============================================

(
echo ===============================================
echo GitHub Actions Runner Setup Complete
echo ===============================================
echo.
echo Installation Location: %RUNNER_DIR%
echo.
echo Next Steps:
echo 1. Restart your computer to ensure all PATH changes take effect
echo 2. Open Command Prompt as Administrator
echo 3. Navigate to: %RUNNER_DIR%
echo 4. Run: configure-runner.bat
echo 5. Follow the prompts to connect to your GitHub repository
echo.
echo Required Information:
echo - Repository URL: https://github.com/OWNER/REPO
echo - Runner token: Get from GitHub repo Settings ^> Actions ^> Runners ^> New self-hosted runner
echo.
echo Optional: To run as Windows Service
echo - After configuration, run: install-service.bat
echo.
echo Installed Software:
echo - PowerShell 7
echo - Git for Windows  
echo - CMake
echo - Visual Studio Build Tools 2022
echo - GitHub Actions Runner
echo.
echo ===============================================
) > "%SETUP_DIR%\SETUP_COMPLETE.txt"

echo.
echo ===============================================
echo Setup Complete!
echo ===============================================
echo.
echo All software has been downloaded and installed.
echo.
echo Summary file created: %SETUP_DIR%\SETUP_COMPLETE.txt
echo Runner location: %RUNNER_DIR%
echo.
echo IMPORTANT: Restart your computer before configuring the runner!
echo.
echo After restart:
echo 1. Open Command Prompt as Administrator
echo 2. cd %RUNNER_DIR%
echo 3. Run: configure-runner.bat
echo.
pause

echo.
echo Do you want to restart now? (Y/N)
set /p RESTART_NOW="Enter choice: "
if /i "!RESTART_NOW!"=="Y" (
    echo Restarting in 10 seconds...
    timeout /t 10
    shutdown /r /t 0
) else (
    echo Please restart manually when ready.
    pause
)

endlocal