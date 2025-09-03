# CMake Build Script - Mirrors GitHub Actions build.yml workflow
# Run this locally to test builds before pushing

param(
    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    
    [Parameter()]
    [ValidateSet("qt-debug", "qt-release", "vs-debug", "vs-release", "dev-debug", "dev-release")]
    [string]$Preset = $null
)

Write-Host "🔨 Starting CMake Build Pipeline..." -ForegroundColor Cyan
Write-Host "  Configuration: $Config" -ForegroundColor Yellow

# Check if we're in the right directory
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "❌ Error: Must run from project root directory" -ForegroundColor Red
    exit 1
}

# Set environment variables
$env:VCPKG_FORCE_SYSTEM_BINARIES = "1"
$env:VCPKG_MAX_CONCURRENCY = "2"

# Determine preset based on config if not specified
if (-not $Preset) {
    if ($Config -eq "Debug") {
        $Preset = "qt-debug"  # Default to Qt debug for Windows
    } else {
        $Preset = "vs-release"  # Default to VS release for Windows
    }
}

Write-Host "📋 Build Configuration:" -ForegroundColor Yellow
Write-Host "  Preset: $Preset"
Write-Host "  VCPKG_FORCE_SYSTEM_BINARIES: $env:VCPKG_FORCE_SYSTEM_BINARIES"
Write-Host "  VCPKG_MAX_CONCURRENCY: $env:VCPKG_MAX_CONCURRENCY"

# Set up VCPKG if not already set
if (-not $env:VCPKG_ROOT) {
    $env:VCPKG_ROOT = "C:\vcpkg"
    Write-Host "  Set VCPKG_ROOT to: $env:VCPKG_ROOT"
}

# Configure
Write-Host "⚙️ Configuring CMake with preset '$Preset'..." -ForegroundColor Yellow
try {
    cmake --preset $Preset -DVCPKG_TARGET_TRIPLET=x64-windows
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ CMake configure failed" -ForegroundColor Red
        exit 1
    }
    Write-Host "  ✅ Configure completed successfully" -ForegroundColor Green
} catch {
    Write-Host "❌ CMake configure failed: $_" -ForegroundColor Red
    exit 1
}

# Determine build directory based on preset
$buildDir = switch ($Preset) {
    "qt-debug" { "build\qt-debug" }
    "qt-release" { "build\qt-release" }
    "vs-debug" { "build\vs-debug" }
    "vs-release" { "build\vs-release" }
    "dev-debug" { "build\dev-debug" }
    "dev-release" { "build\dev-release" }
    default { "build\$Preset" }
}

Write-Host "🔨 Building with preset '$Preset'..." -ForegroundColor Yellow
Write-Host "  Build directory: $buildDir"

try {
    cmake --build $buildDir --config $Config --parallel 2
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Build failed" -ForegroundColor Red
        exit 1
    }
    Write-Host "  ✅ Build completed successfully" -ForegroundColor Green
} catch {
    Write-Host "❌ Build failed: $_" -ForegroundColor Red
    exit 1
}

# Run tests if available
Write-Host "🧪 Running tests..." -ForegroundColor Yellow
try {
    Push-Location $buildDir
    $testResult = ctest --config $Config --output-on-failure 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✅ All tests passed" -ForegroundColor Green
    } else {
        Write-Host "  ⚠️ Tests not available or some tests failed" -ForegroundColor Yellow
        Write-Host "  Test output: $testResult" -ForegroundColor Gray
    }
} catch {
    Write-Host "  ⚠️ Tests not available: $_" -ForegroundColor Yellow
} finally {
    Pop-Location
}

# Check for build artifacts
Write-Host "📦 Checking build artifacts..." -ForegroundColor Yellow
$artifactDirs = @(
    "$buildDir\Debug",
    "$buildDir\Release", 
    "$buildDir\src\app\Debug",
    "$buildDir\src\app\Release",
    "$buildDir\src\app",
    "$buildDir"
)

$foundArtifacts = $false
foreach ($dir in $artifactDirs) {
    if (Test-Path $dir) {
        $exeFiles = Get-ChildItem $dir -Filter "*.exe" -ErrorAction SilentlyContinue
        if ($exeFiles) {
            Write-Host "  ✅ Found executables in ${dir}:" -ForegroundColor Green
            foreach ($exe in $exeFiles) {
                Write-Host "    - $($exe.Name) ($([math]::Round($exe.Length / 1MB, 2)) MB)" -ForegroundColor Gray
            }
            $foundArtifacts = $true
        }
    }
}

if (-not $foundArtifacts) {
    Write-Host "  ⚠️ No executable artifacts found" -ForegroundColor Yellow
    Write-Host "  Build directory structure:" -ForegroundColor Gray
    if (Test-Path $buildDir) {
        Get-ChildItem $buildDir -Recurse -Include "*.exe", "*.dll" | ForEach-Object {
            Write-Host "    $($_.FullName.Replace((Get-Location).Path, '.'))" -ForegroundColor Gray
        }
    }
}

# Check for profiling data
$profilingFiles = Get-ChildItem $buildDir -Filter "profiling*.json" -Recurse -ErrorAction SilentlyContinue
if ($profilingFiles) {
    Write-Host "📊 Found profiling data:" -ForegroundColor Yellow
    foreach ($file in $profilingFiles) {
        Write-Host "  - $($file.FullName.Replace((Get-Location).Path, '.'))" -ForegroundColor Gray
    }
}

Write-Host "🎉 Build pipeline completed successfully!" -ForegroundColor Green
Write-Host "Build artifacts are ready in: $buildDir" -ForegroundColor Cyan

# Show quick start command
if ($Config -eq "Debug" -and $Preset -eq "qt-debug") {
    Write-Host ""
    Write-Host "🚀 Quick start:" -ForegroundColor Cyan
    Write-Host "  .\run_video_editor_debug.bat" -ForegroundColor White
}
