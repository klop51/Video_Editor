<#!
.SYNOPSIS
  Convenience script to configure, build, and run video editor tools.
.DESCRIPTION
  Wraps common CMake preset workflows. Ensures VCPKG_ROOT is set, configures if needed, builds targets, and runs selected tool(s).
.PARAMETER Preset
  CMake configure/build preset (default: dev-debug)
.PARAMETER Tool
  Tool to run after build: media_probe | playback_demo | qt_preview | none
.PARAMETER Media
  Media file path passed to tool (if required)
.PARAMETER Reconfigure
  Force reconfigure (delete existing build dir for preset)
.EXAMPLE
  ./build_and_run.ps1 -Media sample.mp4 -Tool media_probe
.EXAMPLE
  ./build_and_run.ps1 -Preset dev-release -Tool playback_demo -Media sample.mp4
#>
param(
  [string]$Preset = 'dev-debug',
  [ValidateSet('media_probe','playback_demo','qt_preview','none')]
  [string]$Tool = 'media_probe',
  [string]$Media = '',
  [switch]$Reconfigure,
  [switch]$NoBuild,
  [string]$CMakePath = '' # Optional explicit cmake.exe path
)

$ErrorActionPreference = 'Stop'

function Write-Info($m){ Write-Host "[INFO] $m" -ForegroundColor Cyan }
function Write-Warn($m){ Write-Host "[WARN] $m" -ForegroundColor Yellow }
function Write-Err($m){ Write-Host "[ERR ] $m" -ForegroundColor Red }

if(-not $env:VCPKG_ROOT){
  Write-Warn 'VCPKG_ROOT not set. FFmpeg/Qt dependencies unavailable. Either set $env:VCPKG_ROOT or use no-ffmpeg preset.'
  if($Preset -eq 'dev-debug' -or $Preset -eq 'dev-release' -or $Preset -eq 'ci-release'){
    Write-Info "Falling back to preset 'no-ffmpeg' because VCPKG_ROOT is missing."
    $Preset = 'no-ffmpeg'
  }
}

function Resolve-CMake {
  param([string]$UserPath)
  if($UserPath -and (Test-Path $UserPath)) { return $UserPath }
  $candidate = (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source
  if($candidate) { return $candidate }
  $default1 = Join-Path "$env:ProgramFiles" 'CMake/bin/cmake.exe'
  if(Test-Path $default1){ return $default1 }
  $vswhere = "$env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
  if(Test-Path $vswhere){
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null
    if($vsInstall){
      $cmakeInVS = Join-Path $vsInstall 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
      if(Test-Path $cmakeInVS){ return $cmakeInVS }
    }
  }
  return $null
}

$cmakeExe = Resolve-CMake -UserPath $CMakePath
if(-not $cmakeExe){
  Write-Err 'CMake not found. Install via: winget install Kitware.CMake  (then reopen PowerShell)'
  Write-Host 'Or download: https://cmake.org/download/' -ForegroundColor Yellow
  exit 10
}
Write-Info "Using CMake: $cmakeExe"

# Detect CMake version
$cmakeVersionOutput = & $cmakeExe --version 2>$null
$cmakeVersion = ($cmakeVersionOutput -split "\n")[0]
if($cmakeVersion -match "cmake version ([0-9]+)\.([0-9]+)"){
  $cmMajor = [int]$Matches[1]; $cmMinor=[int]$Matches[2]
  if($cmMajor -lt 3 -or ($cmMajor -eq 3 -and $cmMinor -lt 25)){
    Write-Warn "CMake version appears to be $cmMajor.$cmMinor (<3.25). Presets (version 6) may not parse. Will attempt fallback if preset configure fails."
  }
}

$buildDir = Join-Path -Path $PSScriptRoot -ChildPath "build/$Preset"
if($Reconfigure -and (Test-Path $buildDir)){
    Write-Info "Removing existing build dir: $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

if(-not (Test-Path $buildDir)){
  Write-Info "Configuring preset $Preset"
  & $cmakeExe --preset $Preset
  if($LASTEXITCODE -ne 0){
    Write-Warn "Preset configure failed (exit $LASTEXITCODE). Attempting manual fallback (no-ffmpeg)."
    $manualDir = Join-Path $PSScriptRoot 'build/manual-debug'
    if(Test-Path $manualDir){ Remove-Item -Recurse -Force $manualDir }
  & $cmakeExe -S $PSScriptRoot -B $manualDir -DCMAKE_BUILD_TYPE=Debug -DENABLE_FFMPEG=OFF -DENABLE_QT_TOOLS=OFF -DENABLE_TESTS=OFF
    if($LASTEXITCODE -ne 0){ Write-Err "Manual configure also failed (exit $LASTEXITCODE). Aborting."; exit $LASTEXITCODE }
    $buildDir = $manualDir
    $Preset = 'manual-debug'
  }
}
elseif($Reconfigure){
  Write-Info "Re-configuring preset $Preset"
  & $cmakeExe --preset $Preset
  if($LASTEXITCODE -ne 0){
    Write-Warn "Preset re-configure failed. Skipping reconfigure and continuing with existing build or attempting manual fallback."
    if(-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))){
      $manualDir = Join-Path $PSScriptRoot 'build/manual-debug'
            & $cmakeExe -S $PSScriptRoot -B $manualDir -DCMAKE_BUILD_TYPE=Debug -DENABLE_FFMPEG=OFF -DENABLE_QT_TOOLS=OFF -DENABLE_TESTS=OFF
      if($LASTEXITCODE -ne 0){ Write-Err "Manual configure failed (exit $LASTEXITCODE)."; exit $LASTEXITCODE }
      $buildDir = $manualDir
      $Preset = 'manual-debug'
    }
  }
}

if(-not $NoBuild){
  if($Preset -eq 'manual-debug'){
    Write-Info 'Building manual-debug directory'
    & $cmakeExe --build (Join-Path $PSScriptRoot 'build/manual-debug') -j
  } else {
    Write-Info "Building preset $Preset"
    & $cmakeExe --build --preset $Preset -j
  }
  if($LASTEXITCODE -ne 0){ Write-Err "Build failed (exit $LASTEXITCODE)."; exit $LASTEXITCODE }
}

function Resolve-ExePath {
  param(
    [string]$BaseDir,
    [string]$Relative
  )
  $candidate = Join-Path $BaseDir $Relative
  if(Test-Path $candidate){ return $candidate }
  # Multi-config (Visual Studio / Ninja Multi-Config) places binaries under a config subfolder
  $configGuess = 'Debug'
  if($Preset -match '(?i)release'){ $configGuess = 'Release' }
  $multi = Join-Path (Join-Path $BaseDir $configGuess) $Relative
  if(Test-Path $multi){ return $multi }
  # Fallback: search
  $name = Split-Path $Relative -Leaf
  $found = Get-ChildItem -Path $BaseDir -Recurse -Filter $name -ErrorAction SilentlyContinue | Select-Object -First 1
  if($found){ return $found.FullName }
  return $candidate # original (will fail later)
}

$exeRel = ''
switch($Tool){
  'media_probe' { $exeRel = 'src/tools/media_probe/ve_media_probe.exe' }
  'playback_demo' { $exeRel = 'src/tools/playback_demo/ve_playback_demo.exe' }
  'qt_preview' { $exeRel = 'src/tools/qt_preview/ve_qt_preview.exe' }
  'none' { }
}

$exePath = if($Tool -ne 'none') { Resolve-ExePath -BaseDir $buildDir -Relative $exeRel } else { '' }

if($Tool -ne 'none'){
  if(-not (Test-Path $exePath)){
    Write-Err "Executable not found after search: $exePath"
    Write-Info "Tried base dir: $buildDir (multi-config Debug/Release subfolders also checked)."
    exit 3
  }
  # Prepend vcpkg runtime bin paths so FFmpeg / Qt DLLs can be located when using dynamic linkage
  $vcpkgBinCandidates = @()
  $localVcpkgRoot = Join-Path $buildDir 'vcpkg_installed/x64-windows/bin'
  if(Test-Path $localVcpkgRoot){ $vcpkgBinCandidates += $localVcpkgRoot }
  if($env:VCPKG_ROOT){
    $globalTripletBin = Join-Path $env:VCPKG_ROOT 'installed/x64-windows/bin'
    if(Test-Path $globalTripletBin){ $vcpkgBinCandidates += $globalTripletBin }
  }
  foreach($p in $vcpkgBinCandidates){
    if(-not ($env:PATH -split ';' | Where-Object { $_ -ieq $p })){
      Write-Info "Adding runtime path: $p"
      $env:PATH = "$p;$env:PATH"
    }
  }
    if(($Tool -ne 'none') -and [string]::IsNullOrWhiteSpace($Media)){
        Write-Err "Media path required for tool '$Tool' (use -Media)."
        exit 4
    }
    Write-Info "Running $Tool with media '$Media'"
    & $exePath $Media
    $ec = $LASTEXITCODE
    if($ec -ne 0){ Write-Err "Tool exited with code $ec"; exit $ec }
}
else {
    Write-Info 'No tool executed (Tool=none).'
}

Write-Info 'Done.'
