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

# Try configuration-specific plugin paths first, then fallbacks
if ($Configuration -eq "Debug") {
    $possiblePluginPaths = @(
        (Join-Path $VcpkgInstalledDir "x64-windows\debug\Qt6\plugins\platforms"),    # Debug first
        (Join-Path $VcpkgInstalledDir "x64-windows\Qt6\plugins\platforms"),          # Release fallback
        (Join-Path $VcpkgInstalledDir "x64-windows\plugins\platforms"),              # Fallback 1
        (Join-Path $VcpkgInstalledDir "x64-windows\Qt\plugins\platforms"),           # Fallback 2
        (Join-Path $VcpkgInstalledDir "x64-windows\tools\Qt6\bin\platforms")         # Fallback 3
    )
    $targetPattern = "qwindows*d.dll"  # Debug plugins end with 'd'
} else {
    $possiblePluginPaths = @(
        (Join-Path $VcpkgInstalledDir "x64-windows\Qt6\plugins\platforms"),          # Release first
        (Join-Path $VcpkgInstalledDir "x64-windows\debug\Qt6\plugins\platforms"),    # Debug fallback
        (Join-Path $VcpkgInstalledDir "x64-windows\plugins\platforms"),              # Fallback 1
        (Join-Path $VcpkgInstalledDir "x64-windows\Qt\plugins\platforms"),           # Fallback 2
        (Join-Path $VcpkgInstalledDir "x64-windows\tools\Qt6\bin\platforms")         # Fallback 3
    )
    $targetPattern = "qwindows.dll"   # Release plugins without 'd'
}

$platformFound = $false
foreach ($platformSourceDir in $possiblePluginPaths) {
    Write-Host "Checking platform path: $platformSourceDir" -ForegroundColor Yellow
    if (Test-Path $platformSourceDir) {
        Write-Host "Found platform plugins at: $platformSourceDir" -ForegroundColor Green
        $platformFiles = Get-ChildItem -Path $platformSourceDir -Name $targetPattern -ErrorAction SilentlyContinue
        foreach ($file in $platformFiles) {
            $sourcePath = Join-Path $platformSourceDir $file
            Copy-DllBasic -Source $sourcePath -Destination $platformsDir -Description "Platform Plugin"
            $platformFound = $true
        }
        if ($platformFound) { break }  # Stop after finding matching plugins
    }
}

if (-not $platformFound) {
    Write-Warning "Platform plugins not found in any expected location. Searching vcpkg directory..."
    $allDlls = Get-ChildItem -Path $VcpkgInstalledDir -Recurse -Name "qwindows*.dll" -ErrorAction SilentlyContinue
    if ($allDlls) {
        Write-Host "Found qwindows.dll files at:" -ForegroundColor Yellow
        foreach ($dll in $allDlls) {
            $fullPath = (Get-ChildItem -Path $VcpkgInstalledDir -Recurse -Name $dll -ErrorAction SilentlyContinue)[0].FullName
            Write-Host "  $fullPath" -ForegroundColor Cyan
            Copy-DllBasic -Source $fullPath -Destination $platformsDir -Description "Platform Plugin"
            $platformFound = $true
            break  # Just copy the first one found
        }
    }
}

# Deploy additional Qt plugins that may be needed (configuration-aware)
Write-Host "`n=== Deploying Additional Qt Plugins ===" -ForegroundColor Magenta

$additionalPluginDirs = @("imageformats", "styles", "iconengines")
foreach ($pluginType in $additionalPluginDirs) {
    $targetPluginDir = Join-Path $targetDir $pluginType
    $pluginFound = $false
    
    foreach ($possiblePath in $possiblePluginPaths) {
        $basePluginDir = Split-Path $possiblePath -Parent
        $pluginSourceDir = Join-Path $basePluginDir $pluginType
        
        if (Test-Path $pluginSourceDir) {
            Write-Host "Found $pluginType plugins at: $pluginSourceDir" -ForegroundColor Green
            if (-not (Test-Path $targetPluginDir)) {
                New-Item -ItemType Directory -Path $targetPluginDir -Force | Out-Null
            }
            
            # Deploy only configuration-matching plugins
            if ($Configuration -eq "Debug") {
                $pluginFiles = Get-ChildItem -Path $pluginSourceDir -Name "*d.dll" -ErrorAction SilentlyContinue
            } else {
                $pluginFiles = Get-ChildItem -Path $pluginSourceDir -Name "*.dll" -ErrorAction SilentlyContinue | 
                              Where-Object { $_ -notlike "*d.dll" }
            }
            
            foreach ($file in $pluginFiles) {
                $sourcePath = Join-Path $pluginSourceDir $file
                Copy-DllBasic -Source $sourcePath -Destination $targetPluginDir -Description "$pluginType Plugin"
                $pluginFound = $true
            }
            if ($pluginFound) { break }
        }
    }
}

# Create qt.conf file to help Qt find plugins
Write-Host "`n=== Creating qt.conf ===" -ForegroundColor Magenta

$qtConfPath = Join-Path $targetDir "qt.conf"
$qtConfContent = "[Paths]`r`nPlugins = .`r`n"

# Use more reliable file creation method
[System.IO.File]::WriteAllText($qtConfPath, $qtConfContent, [System.Text.Encoding]::UTF8)
Write-Host "✅ Created qt.conf file to help Qt locate plugins" -ForegroundColor Green

# Summary check
Write-Host "`n=== Deployment Summary ===" -ForegroundColor Green

$criticalDlls = @("Qt6Core*.dll", "Qt6Gui*.dll", "Qt6Widgets*.dll")
$allPresent = $true

foreach ($pattern in $criticalDlls) {
    $found = Get-ChildItem -Path $targetDir -Name $pattern -ErrorAction SilentlyContinue
    if ($found) {
        Write-Host "✅ Critical DLL present: $($found.Name)" -ForegroundColor Green
    } else {
        Write-Host "❌ Critical DLL missing: $pattern" -ForegroundColor Red
        $allPresent = $false
    }
}

# Check for platform plugin (non-blocking)
$platformPlugin = Get-ChildItem -Path $platformsDir -Name "qwindows*.dll" -ErrorAction SilentlyContinue
if ($platformPlugin) {
    Write-Host "✅ Platform plugin present: $($platformPlugin.Name)" -ForegroundColor Green
} else {
    Write-Host "⚠️ Platform plugin missing: qwindows.dll" -ForegroundColor Yellow
    Write-Host "⚠️ This may cause 'qt.qpa.plugin' errors in headless environments" -ForegroundColor Yellow
}

# Only fail if critical DLLs are missing, not platform plugins
$criticalMissing = -not $allPresent
foreach ($pattern in $criticalDlls) {
    $found = Get-ChildItem -Path $targetDir -Name $pattern -ErrorAction SilentlyContinue
    if (-not $found) {
        $criticalMissing = $true
        break
    }
}

if ($criticalMissing) {
    Write-Host "`n❌ DLL deployment failed - missing critical DLLs!" -ForegroundColor Red
    Write-Host "Build cannot continue without core Qt libraries." -ForegroundColor Red
    exit 1
} else {
    if ($platformPlugin) {
        Write-Host "`n✅ DLL deployment completed successfully!" -ForegroundColor Green
    } else {
        Write-Host "`n⚠️ DLL deployment completed with warnings!" -ForegroundColor Yellow
        Write-Host "Core DLLs deployed but platform plugins missing (expected in CI/headless environments)" -ForegroundColor Yellow
    }
    exit 0
}