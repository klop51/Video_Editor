@echo off
setlocal enabledelayedexpansion

REM =============================================================================
REM GitHub Actions Runner Configuration Script
REM This script configures an already installed runner
REM Run this AFTER setup-runner-complete.bat and system restart
REM =============================================================================

echo.
echo ===============================================
echo GitHub Actions Runner Configuration
echo ===============================================
echo.

REM Check if running in the correct directory
if not exist "config.cmd" (
    echo ERROR: This script must be run from the GitHub Actions Runner directory
    echo Expected files: config.cmd, run.cmd
    echo.
    echo Current directory: %CD%
    echo.
    echo Please navigate to your runner installation directory first.
    echo Example: cd C:\Users\%USERNAME%\actions-runner
    pause
    exit /b 1
)

REM Get repository information
echo Please provide the following information:
echo.
set /p REPO_URL="Repository URL (e.g., https://github.com/owner/repo): "
set /p RUNNER_TOKEN="Runner Token (from GitHub Settings > Actions > Runners): "
set /p RUNNER_NAME="Runner Name (optional, default: windows-runner): "

REM Set default runner name if not provided
if "!RUNNER_NAME!"=="" set "RUNNER_NAME=windows-runner"

echo.
echo ===============================================
echo Configuration Summary
echo ===============================================
echo Repository: !REPO_URL!
echo Runner Name: !RUNNER_NAME!
echo Token: !RUNNER_TOKEN!
echo.
echo Continue with configuration? (Y/N)
set /p CONFIRM="Enter choice: "
if /i not "!CONFIRM!"=="Y" (
    echo Configuration cancelled.
    pause
    exit /b 0
)

echo.
echo ===============================================
echo Configuring Runner
echo ===============================================

REM Configure the runner
echo Running configuration...
config.cmd --url "!REPO_URL!" --token "!RUNNER_TOKEN!" --name "!RUNNER_NAME!" --work "_work" --labels "self-hosted,Windows,X64" --unattended

if !errorlevel! neq 0 (
    echo.
    echo ERROR: Runner configuration failed!
    echo.
    echo Common issues:
    echo 1. Invalid repository URL
    echo 2. Expired or incorrect token
    echo 3. Runner name already exists
    echo 4. Network connectivity issues
    echo.
    echo Please check your inputs and try again.
    pause
    exit /b 1
)

echo.
echo ===============================================
echo Configuration Successful!
echo ===============================================
echo.
echo Your runner has been configured successfully.
echo.
echo Choose how you want to run the runner:
echo.
echo 1. Interactive Mode (run now, stops when you close terminal)
echo 2. Windows Service (runs automatically in background)
echo 3. Manual (configure only, run later)
echo.
set /p RUN_MODE="Enter choice (1-3): "

if "!RUN_MODE!"=="1" (
    echo.
    echo Starting runner in interactive mode...
    echo Press Ctrl+C to stop the runner.
    echo.
    run.cmd
) else if "!RUN_MODE!"=="2" (
    echo.
    echo Installing runner as Windows Service...
    
    REM Install as service
    .\svc.sh install
    if !errorlevel! neq 0 (
        echo ERROR: Failed to install service
        pause
        exit /b 1
    )
    
    REM Start service
    .\svc.sh start
    if !errorlevel! neq 0 (
        echo ERROR: Failed to start service
        pause
        exit /b 1
    )
    
    echo.
    echo SUCCESS: Runner installed and started as Windows Service
    echo The runner will now start automatically on system boot.
    echo.
    echo Service management commands:
    echo - Stop:    .\svc.sh stop
    echo - Start:   .\svc.sh start
    echo - Status:  .\svc.sh status
    echo - Remove:  .\svc.sh uninstall
    echo.
    pause
) else if "!RUN_MODE!"=="3" (
    echo.
    echo Configuration complete. Runner is ready but not started.
    echo.
    echo To start manually later:
    echo - Interactive: run.cmd
    echo - As service:  .\svc.sh install && .\svc.sh start
    echo.
    pause
) else (
    echo Invalid choice. Configuration complete but runner not started.
    echo Run this script again to start the runner.
    pause
)

endlocal