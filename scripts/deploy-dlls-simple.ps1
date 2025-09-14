# Simple and Reliable DLL Deployment Script for Video Editor
param(
    [Parameter(Mandatory=$true)]
    [string]$TargetBinary,
    
    [Parameter(Mandatory=$true)] 
    [string]$VcpkgInstalledDir,
    
    [Parameter(Mandatory=$false)]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Simple DLL Deployment Script ===" -ForegroundColor Green
Write-Host "Target Binary: $TargetBinary" -ForegroundColor Cyan
Write-Host "Vcpkg Dir: $VcpkgInstalledDir" -ForegroundColor Cyan

# Determine source directories
if ($Configuration -eq "Debug") {
    $vcpkgBinDir = Join-Path $VcpkgInstalledDir "x64-windows\debug\bin"
} else {
    $vcpkgBinDir = Join-Path $VcpkgInstalledDir "x64-windows\bin"
}

$vcpkgPluginsDir = Join-Path $VcpkgInstalledDir "x64-windows\Qt6\plugins"
$targetDir = Split-Path $TargetBinary -Parent
$platformsDir = Join-Path $targetDir "platforms"

Write-Host "Source Dir: $vcpkgBinDir" -ForegroundColor Yellow
Write-Host "Target Dir: $targetDir" -ForegroundColor Yellow

# Basic copy function without complex error handling
function Copy-DllBasic {
    param([string]$Source, [string]$Destination, [string]$Description = "DLL")
    
    if (Test-Path $Source) {
        if (-not (Test-Path $Destination)) {
            New-Item -ItemType Directory -Path $Destination -Force | Out-Null
        }
        
        Copy-Item -Path $Source -Destination $Destination -Force
        Write-Host "  Copied $Description`: $(Split-Path $Source -Leaf)" -ForegroundColor Green
        return $true
    } else {
        Write-Warning "Source not found: $Source"
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
        Copy-DllBasic -Source $sourcePath -Destination $targetDir -Description "Qt6 Core DLL"
    }
}

# Deploy ICU Libraries (typically in release bin)
Write-Host "`n=== Deploying ICU Libraries ===" -ForegroundColor Magenta

$vcpkgReleaseBinDir = Join-Path $VcpkgInstalledDir "x64-windows\bin"
$icuDlls = @("icudt*.dll", "icuin*.dll", "icuuc*.dll")

foreach ($pattern in $icuDlls) {
    $files = Get-ChildItem -Path $vcpkgReleaseBinDir -Name $pattern -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        $sourcePath = Join-Path $vcpkgReleaseBinDir $file
        Copy-DllBasic -Source $sourcePath -Destination $targetDir -Description "ICU Library"
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
        Copy-DllBasic -Source $sourcePath -Destination $platformsDir -Description "Platform Plugin"
    }
} else {
    Write-Warning "Platform plugins directory not found: $platformSourceDir"
}

# Summary check
Write-Host "`n=== Deployment Summary ===" -ForegroundColor Green

$criticalDlls = @("Qt6Core*.dll", "Qt6Gui*.dll", "Qt6Widgets*.dll")
$allPresent = $true

foreach ($pattern in $criticalDlls) {
    $found = Get-ChildItem -Path $targetDir -Name $pattern -ErrorAction SilentlyContinue
    if ($found) {
        Write-Host "Critical DLL present: $($found.Name)" -ForegroundColor Green
    } else {
        Write-Host "Critical DLL missing: $pattern" -ForegroundColor Red
        $allPresent = $false
    }
}

if ($allPresent) {
    Write-Host "`nDLL deployment completed successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`nDLL deployment incomplete!" -ForegroundColor Red
    exit 1
}