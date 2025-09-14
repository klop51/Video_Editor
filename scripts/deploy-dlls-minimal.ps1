# Minimal DLL Deployment Script for Video Editor
param(
    [Parameter(Mandatory=$true)]
    [string]$TargetBinary,
    
    [Parameter(Mandatory=$true)] 
    [string]$VcpkgInstalledDir,
    
    [Parameter(Mandatory=$false)]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Minimal DLL Deployment Script ===" -ForegroundColor Green
Write-Host "Target Binary: $TargetBinary" -ForegroundColor Cyan

# Get target directory from the binary path
$targetDir = Split-Path $TargetBinary -Parent
Write-Host "Target Dir: $targetDir" -ForegroundColor Yellow

# Simple copy function
function Copy-DllSimple {
    param([string]$Source, [string]$Destination)
    
    if (Test-Path $Source) {
        try {
            Copy-Item -Path $Source -Destination $Destination -Force
            Write-Host "Copied: $(Split-Path $Source -Leaf)" -ForegroundColor Green
            return $true
        } catch {
            Write-Warning "Failed to copy: $(Split-Path $Source -Leaf)"
            return $false
        }
    }
    return $false
}

Write-Host "DLL deployment script completed." -ForegroundColor Green
exit 0