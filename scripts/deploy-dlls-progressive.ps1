# Progressive DLL Deployment Script for Video Editor
param(
    [Parameter(Mandatory=$true)]
    [string]$TargetBinary,
    
    [Parameter(Mandatory=$true)] 
    [string]$VcpkgInstalledDir,
    
    [Parameter(Mandatory=$false)]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Progressive DLL Deployment Script ===" -ForegroundColor Green
Write-Host "Target Binary: $TargetBinary" -ForegroundColor Cyan
Write-Host "Vcpkg Dir: $VcpkgInstalledDir" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Cyan

# Determine source directories
if ($Configuration -eq "Debug") {
    $vcpkgBinDir = Join-Path $VcpkgInstalledDir "x64-windows\debug\bin"
    $vcpkgReleaseBinDir = Join-Path $VcpkgInstalledDir "x64-windows\bin"
} else {
    $vcpkgBinDir = Join-Path $VcpkgInstalledDir "x64-windows\bin"
    $vcpkgReleaseBinDir = $vcpkgBinDir
}

$vcpkgPluginsDir = Join-Path $VcpkgInstalledDir "x64-windows\Qt6\plugins"
$targetDir = Split-Path $TargetBinary -Parent
$platformsDir = Join-Path $targetDir "platforms"

Write-Host "Source Dir: $vcpkgBinDir" -ForegroundColor Yellow
Write-Host "Target Dir: $targetDir" -ForegroundColor Yellow

# Simple but robust copy function
function Copy-DllRobust {
    param([string]$Source, [string]$Destination, [string]$Description = "DLL")
    
    if (-not (Test-Path $Source)) {
        Write-Warning "Source not found: $Source"
        return $false
    }
    
    try {
        if (-not (Test-Path $Destination)) {
            New-Item -ItemType Directory -Path $Destination -Force | Out-Null
        }
        
        Copy-Item -Path $Source -Destination $Destination -Force -ErrorAction Stop
        Write-Host "  ✓ $Description copied: $(Split-Path $Source -Leaf)" -ForegroundColor Green
        return $true
        
    } catch {
        Write-Warning "  Failed to copy $Description`: $(Split-Path $Source -Leaf) - $($_.Exception.Message)"
        return $false
    }
}

# Deploy Qt6 Core DLLs
Write-Host "`n=== Deploying Qt6 Core DLLs ===" -ForegroundColor Magenta

$qt6CoreDlls = @("Qt6Core*.dll", "Qt6Gui*.dll", "Qt6Widgets*.dll", "Qt6Network*.dll")

foreach ($pattern in $qt6CoreDlls) {
    $files = Get-ChildItem -Path $vcpkgBinDir -Name $pattern -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        $sourcePath = Join-Path $vcpkgBinDir $file
        Copy-DllRobust -Source $sourcePath -Destination $targetDir -Description "Qt6 Core DLL"
    }
}

# Deploy platform plugins
Write-Host "`n=== Deploying Platform Plugins ===" -ForegroundColor Magenta

if (-not (Test-Path $platformsDir)) {
    New-Item -ItemType Directory -Path $platformsDir -Force | Out-Null
    Write-Host "Created platforms directory" -ForegroundColor Cyan
}

$platformSourceDir = Join-Path $vcpkgPluginsDir "platforms"
if (Test-Path $platformSourceDir) {
    $platformFiles = Get-ChildItem -Path $platformSourceDir -Name "qwindows*.dll" -ErrorAction SilentlyContinue
    foreach ($file in $platformFiles) {
        $sourcePath = Join-Path $platformSourceDir $file
        Copy-DllRobust -Source $sourcePath -Destination $platformsDir -Description "Platform Plugin"
    }
}

# Summary check
Write-Host "`n=== Deployment Summary ===" -ForegroundColor Green

$criticalDlls = @("Qt6Core*.dll", "Qt6Gui*.dll", "Qt6Widgets*.dll")
$allPresent = $true

foreach ($pattern in $criticalDlls) {
    $found = Get-ChildItem -Path $targetDir -Name $pattern -ErrorAction SilentlyContinue
    if ($found) {
        Write-Host "✓ Critical DLL present: $($found.Name)" -ForegroundColor Green
    } else {
        Write-Host "✗ Critical DLL missing: $pattern" -ForegroundColor Red
        $allPresent = $false
    }
}

if ($allPresent) {
    Write-Host "`n🎉 DLL deployment completed successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n❌ DLL deployment incomplete!" -ForegroundColor Red
    exit 1
}