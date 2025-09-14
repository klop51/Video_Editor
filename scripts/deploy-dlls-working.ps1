# Custom DLL Deployment Script for Video Editor - FIXED VERSION
# Bypasses vcpkg's problematic applocal.ps1 with robust file handling
# Copyright (c) 2025 - Designed for Windows CI/CD reliability

param(
    [Parameter(Mandatory=$true)]
    [string]$TargetBinary,
    
    [Parameter(Mandatory=$true)] 
    [string]$VcpkgInstalledDir,
    
    [Parameter(Mandatory=$false)]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Custom DLL Deployment Script ===" -ForegroundColor Green
Write-Host "Target Binary: $TargetBinary" -ForegroundColor Cyan
Write-Host "Vcpkg Dir: $VcpkgInstalledDir" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Cyan

# Determine source directories based on configuration
if ($Configuration -eq "Debug") {
    $vcpkgBinDir = Join-Path $VcpkgInstalledDir "x64-windows\debug\bin"
    $vcpkgReleaseBinDir = Join-Path $VcpkgInstalledDir "x64-windows\bin"  # Some DLLs are always in release
} else {
    $vcpkgBinDir = Join-Path $VcpkgInstalledDir "x64-windows\bin"
    $vcpkgReleaseBinDir = $vcpkgBinDir
}

$vcpkgPluginsDir = Join-Path $VcpkgInstalledDir "x64-windows\Qt6\plugins"

# Get target directory from the binary path
$targetDir = Split-Path $TargetBinary -Parent
$platformsDir = Join-Path $targetDir "platforms"

Write-Host "Source Dir (Debug/Release): $vcpkgBinDir" -ForegroundColor Yellow
Write-Host "Source Dir (Release): $vcpkgReleaseBinDir" -ForegroundColor Yellow  
Write-Host "Plugins Dir: $vcpkgPluginsDir" -ForegroundColor Yellow
Write-Host "Target Dir: $targetDir" -ForegroundColor Yellow
Write-Host "Platforms Dir: $platformsDir" -ForegroundColor Yellow

# Enhanced file copying function with Windows-specific handling
function Copy-DllSafely {
    param(
        [string]$Source,
        [string]$Destination,
        [string]$Description = "DLL",
        [int]$MaxRetries = 5
    )
    
    if (-not (Test-Path $Source)) {
        Write-Warning "Source file not found: $Source"
        return $false
    }
    
    $destinationFile = Join-Path $Destination (Split-Path $Source -Leaf)
    
    # Skip if target is newer than source
    if (Test-Path $destinationFile) {
        $sourceTime = (Get-Item $Source).LastWriteTime
        $destTime = (Get-Item $destinationFile).LastWriteTime
        if ($destTime -ge $sourceTime) {
            Write-Host "  ✓ $Description already up to date: $(Split-Path $Source -Leaf)" -ForegroundColor DarkGreen
            return $true
        }
    }
    
    for ($attempt = 1; $attempt -le $MaxRetries; $attempt++) {
        try {
            # Ensure destination directory exists
            if (-not (Test-Path $Destination)) {
                New-Item -ItemType Directory -Path $Destination -Force | Out-Null
            }
            
            # Remove read-only attributes if they exist
            if (Test-Path $destinationFile) {
                Set-ItemProperty -Path $destinationFile -Name IsReadOnly -Value $false -ErrorAction SilentlyContinue
            }
            
            # Use PowerShell's Copy-Item with force
            Copy-Item -Path $Source -Destination $Destination -Force -ErrorAction Stop
            
            Write-Host "  ✓ $Description copied: $(Split-Path $Source -Leaf)" -ForegroundColor Green
            return $true
            
        } catch {
            Write-Warning "  ⚠ Attempt $attempt failed for $Description $(Split-Path $Source -Leaf): $($_.Exception.Message)"
            
            if ($attempt -lt $MaxRetries) {
                # Progressive delay with process cleanup
                $delay = $attempt * 2
                Write-Host "    Waiting ${delay}s and cleaning up processes..." -ForegroundColor Yellow
                
                # Kill any processes that might be locking the DLL
                $fileName = Split-Path $Source -Leaf
                $baseName = [System.IO.Path]::GetFileNameWithoutExtension($fileName)
                
                # Try to find and kill processes using this DLL
                Get-Process | Where-Object { 
                    $_.ProcessName -like "*$baseName*" -or 
                    $_.ProcessName -eq "video_editor" -or 
                    $_.ProcessName -eq "ve_qt_preview" -or
                    $_.ProcessName -like "*qt*"
                } | ForEach-Object {
                    try {
                        Write-Host "    Killing process: $($_.ProcessName) (PID: $($_.Id))" -ForegroundColor Red
                        Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
                    } catch {
                        # Ignore errors when killing processes
                    }
                }
                
                Start-Sleep -Seconds $delay
            }
        }
    }
    
    Write-Error "Failed to copy $Description after $MaxRetries attempts: $(Split-Path $Source -Leaf)"
    return $false
}

# Validation function for deployment summary
function Test-DllDeploymentSuccess {
    param(
        [string]$TargetDirectory,
        [string]$PlatformsDirectory
    )
    
    Write-Host "`n=== DLL Deployment Summary ===" -ForegroundColor Green
    
    # Define critical DLLs to verify
    $criticalDlls = @(
        "Qt6Core*.dll",
        "Qt6Gui*.dll", 
        "Qt6Widgets*.dll"
    )
    
    $allCriticalPresent = $true
    foreach ($pattern in $criticalDlls) {
        $found = Get-ChildItem -Path $TargetDirectory -Name $pattern -ErrorAction SilentlyContinue
        if ($found) {
            Write-Host "✓ Critical DLL present: $($found.Name)" -ForegroundColor Green
        } else {
            Write-Host "✗ Critical DLL missing: $pattern" -ForegroundColor Red
            $allCriticalPresent = $false
        }
    }
    
    $platformFound = Get-ChildItem -Path $PlatformsDirectory -Name "qwindows*.dll" -ErrorAction SilentlyContinue
    if ($platformFound) {
        Write-Host "✓ Platform plugin present: $($platformFound.Name)" -ForegroundColor Green
    } else {
        Write-Host "✗ Platform plugin missing: qwindows*.dll" -ForegroundColor Red
        $allCriticalPresent = $false
    }
    
    return $allCriticalPresent
}

# Define DLL groups for organized deployment
Write-Host "`n=== Deploying Qt6 Core DLLs ===" -ForegroundColor Magenta

$qt6CoreDlls = @(
    "Qt6Core*.dll",
    "Qt6Gui*.dll", 
    "Qt6Widgets*.dll",
    "Qt6Network*.dll",
    "Qt6DBus*.dll"
)

foreach ($pattern in $qt6CoreDlls) {
    Get-ChildItem -Path $vcpkgBinDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        $sourcePath = Join-Path $vcpkgBinDir $_
        Copy-DllSafely -Source $sourcePath -Destination $targetDir -Description "Qt6 Core DLL"
    }
}

Write-Host "`n=== Deploying ICU Libraries ===" -ForegroundColor Magenta

$icuDlls = @(
    "icudt*.dll",
    "icuin*.dll", 
    "icuuc*.dll"
)

foreach ($pattern in $icuDlls) {
    # ICU DLLs are typically in release bin even for debug builds
    Get-ChildItem -Path $vcpkgReleaseBinDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        $sourcePath = Join-Path $vcpkgReleaseBinDir $_
        Copy-DllSafely -Source $sourcePath -Destination $targetDir -Description "ICU Library"
    }
}

Write-Host "`n=== Deploying Qt6 Dependencies ===" -ForegroundColor Magenta

$qt6Dependencies = @(
    "pcre2-16*.dll",
    "double-conversion*.dll",
    "zlib*.dll",
    "harfbuzz*.dll", 
    "freetype*.dll",
    "brotlicommon*.dll",
    "brotlidec*.dll",
    "libpng16*.dll"
)

foreach ($pattern in $qt6Dependencies) {
    # Try debug dir first, then release dir
    $found = $false
    
    Get-ChildItem -Path $vcpkgBinDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        $sourcePath = Join-Path $vcpkgBinDir $_
        Copy-DllSafely -Source $sourcePath -Destination $targetDir -Description "Qt6 Dependency"
        $found = $true
    }
    
    if (-not $found) {
        Get-ChildItem -Path $vcpkgReleaseBinDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
            $sourcePath = Join-Path $vcpkgReleaseBinDir $_
            Copy-DllSafely -Source $sourcePath -Destination $targetDir -Description "Qt6 Dependency"
        }
    }
}

Write-Host "`n=== Deploying FFmpeg Libraries ===" -ForegroundColor Magenta

$ffmpegDlls = @(
    "avcodec-*.dll",
    "avformat-*.dll",
    "avutil-*.dll", 
    "swscale-*.dll",
    "swresample-*.dll",
    "avfilter-*.dll",
    "avdevice-*.dll"
)

foreach ($pattern in $ffmpegDlls) {
    Get-ChildItem -Path $vcpkgReleaseBinDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        $sourcePath = Join-Path $vcpkgReleaseBinDir $_
        Copy-DllSafely -Source $sourcePath -Destination $targetDir -Description "FFmpeg Library"
    }
}

Write-Host "`n=== Deploying Logging & Compression Libraries ===" -ForegroundColor Magenta

$loggingDlls = @(
    "fmt*.dll",
    "spdlog*.dll",
    "bz2*.dll",
    "lz4*.dll", 
    "zstd*.dll"
)

foreach ($pattern in $loggingDlls) {
    # Try debug dir first for debug-specific versions
    $found = $false
    
    Get-ChildItem -Path $vcpkgBinDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        $sourcePath = Join-Path $vcpkgBinDir $_
        Copy-DllSafely -Source $sourcePath -Destination $targetDir -Description "Utility Library"
        $found = $true
    }
    
    if (-not $found) {
        Get-ChildItem -Path $vcpkgReleaseBinDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
            $sourcePath = Join-Path $vcpkgReleaseBinDir $_
            Copy-DllSafely -Source $sourcePath -Destination $targetDir -Description "Utility Library"
        }
    }
}

Write-Host "`n=== Deploying Qt6 Platform Plugins ===" -ForegroundColor Magenta

# Create platforms directory
if (-not (Test-Path $platformsDir)) {
    New-Item -ItemType Directory -Path $platformsDir -Force | Out-Null
    Write-Host "Created platforms directory: $platformsDir" -ForegroundColor Cyan
}

# Deploy platform plugins
$platformPlugins = @(
    "qwindows*.dll",
    "qminimal*.dll"
)

$platformSourceDir = Join-Path $vcpkgPluginsDir "platforms"
if (Test-Path $platformSourceDir) {
    foreach ($pattern in $platformPlugins) {
        Get-ChildItem -Path $platformSourceDir -Name $pattern -ErrorAction SilentlyContinue | ForEach-Object {
            $sourcePath = Join-Path $platformSourceDir $_
            Copy-DllSafely -Source $sourcePath -Destination $platformsDir -Description "Qt6 Platform Plugin"
        }
    }
} else {
    Write-Warning "Platform plugins directory not found: $platformSourceDir"
}

# Call deployment validation function
$deploymentSuccess = Test-DllDeploymentSuccess -TargetDirectory $targetDir -PlatformsDirectory $platformsDir

if ($deploymentSuccess) {
    Write-Host "`n🎉 DLL deployment completed successfully!" -ForegroundColor Green
    Write-Host "All critical dependencies are present for Qt6 application." -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n❌ DLL deployment incomplete!" -ForegroundColor Red
    Write-Host "Some critical dependencies are missing." -ForegroundColor Red
    exit 1
}