#!/usr/bin/env powershell
# GitHub CI Quick Launcher
# Usage: .\github_ci.ps1 [action]

param([string]$Action = "status")

$ScriptPath = Join-Path $PSScriptRoot "scripts\github_automation_v2.ps1"

switch ($Action.ToLower()) {
    "status" { 
        & $ScriptPath monitor
    }
    "watch" { 
        & $ScriptPath continuous -CheckIntervalSeconds 20
    }
    "logs" { 
        & $ScriptPath logs
    }
    "artifacts" { 
        & $ScriptPath artifacts
    }
    "help" {
        Write-Host "GitHub CI Quick Launcher" -ForegroundColor Cyan
        Write-Host "Usage: .\github_ci.ps1 [action]" -ForegroundColor White
        Write-Host ""
        Write-Host "Actions:" -ForegroundColor Yellow
        Write-Host "  status     - Check current CI status (default)" -ForegroundColor White
        Write-Host "  watch      - Start continuous monitoring" -ForegroundColor White  
        Write-Host "  logs       - Show failure logs" -ForegroundColor White
        Write-Host "  artifacts  - Download CI artifacts" -ForegroundColor White
        Write-Host "  help       - Show this help" -ForegroundColor White
    }
    default {
        Write-Host "Unknown action: $Action" -ForegroundColor Red
        Write-Host "Use 'help' to see available actions" -ForegroundColor Yellow
    }
}
