@echo off
REM GitHub CI Monitor - Quick launcher
REM Usage: 
REM   github_monitor.bat           - Check status
REM   github_monitor.bat logs      - Show failure logs  
REM   github_monitor.bat watch     - Start continuous monitoring
REM   github_monitor.bat artifacts - Download artifacts

setlocal
set "SCRIPT_DIR=%~dp0"
set "ACTION=%1"

if "%ACTION%"=="" set "ACTION=monitor"
if "%ACTION%"=="watch" set "ACTION=continuous"

echo Starting GitHub CI automation...
powershell.exe -ExecutionPolicy Bypass -File "%SCRIPT_DIR%github_automation.ps1" %ACTION% %2 %3 %4 %5

pause
