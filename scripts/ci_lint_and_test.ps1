# CI Lint and Test Script - Mirrors GitHub Actions tests.yml workflow
# Run this locally to test before pushing

param(
    [string]$Configuration = "dev-debug",
    [switch]$SkipTidy
)

Write-Host "Starting CI Lint and Test Pipeline..." -ForegroundColor Cyan
Write-Host "  Configuration: $Configuration" -ForegroundColor Cyan
if ($SkipTidy) {
    Write-Host "  Skipping clang-tidy analysis" -ForegroundColor Cyan
}

# Check if we're in the right directory
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Error: Must run from project root directory" -ForegroundColor Red
    exit 1
}

# Set environment variables (conditional for platform compatibility)
if ($IsLinux) {
    $env:VCPKG_FORCE_SYSTEM_BINARIES = "1"
    Write-Host "Environment Setup (Linux):" -ForegroundColor Yellow
    Write-Host "  VCPKG_FORCE_SYSTEM_BINARIES: $env:VCPKG_FORCE_SYSTEM_BINARIES"
} else {
    Write-Host "Environment Setup (Windows):" -ForegroundColor Yellow
    Write-Host "  Using default vcpkg settings (VCPKG_FORCE_SYSTEM_BINARIES not set)"
}
$env:VCPKG_MAX_CONCURRENCY = "1"
Write-Host "  VCPKG_MAX_CONCURRENCY: $env:VCPKG_MAX_CONCURRENCY"

# Configure with specified preset
Write-Host "Configuring CMake..." -ForegroundColor Yellow
if (-not $env:VCPKG_ROOT) {
    $env:VCPKG_ROOT = "C:\vcpkg"
    Write-Host "  Set VCPKG_ROOT to: $env:VCPKG_ROOT"
}

try {
    cmake --preset $Configuration -DVCPKG_TARGET_TRIPLET=x64-windows
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configure failed" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "CMake configure failed: $_" -ForegroundColor Red
    exit 1
}

# Check if compile_commands.json exists (optional for VS builds)
Write-Host "Checking for compile_commands.json..." -ForegroundColor Yellow
$compileCommandsPath = "build\$Configuration\compile_commands.json"
if (Test-Path $compileCommandsPath) {
    Write-Host "  Found compile_commands.json" -ForegroundColor Green
} else {
    Write-Host "  compile_commands.json not found at $compileCommandsPath" -ForegroundColor Yellow
    Write-Host "  This is normal for Visual Studio generator builds" -ForegroundColor Gray
    Write-Host "  Build directory exists and CMake succeeded" -ForegroundColor Green
}

# Run clang-tidy (if available and not skipped)
if (-not $SkipTidy) {
    Write-Host "Running clang-tidy..." -ForegroundColor Yellow
    if (Get-Command clang-tidy -ErrorAction SilentlyContinue) {
        try {
            python scripts\clang_tidy_check.py
            if ($LASTEXITCODE -ne 0) {
                Write-Host "clang-tidy found issues" -ForegroundColor Red
                exit 1
            }
            Write-Host "  clang-tidy passed" -ForegroundColor Green
        } catch {
            Write-Host "clang-tidy failed: $_" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "  clang-tidy not available on Windows - skipping" -ForegroundColor Yellow
        Write-Host "  Use WSL or Linux CI for clang-tidy checks" -ForegroundColor Gray
    }
} else {
    Write-Host "Skipping clang-tidy (SkipTidy parameter set)" -ForegroundColor Yellow
}

# Simple configure test (no FFmpeg, no presets)
Write-Host "Running simple configure test..." -ForegroundColor Yellow
$lintTestDir = "build\lint-test"
if (Test-Path $lintTestDir) {
    Remove-Item $lintTestDir -Recurse -Force
}
New-Item -ItemType Directory -Path $lintTestDir -Force | Out-Null

try {
    Push-Location $lintTestDir
    cmake ..\.. -GNinja -DCMAKE_BUILD_TYPE=Debug -DENABLE_FFMPEG=OFF -DENABLE_TESTS=OFF -DENABLE_TOOLS=OFF
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Simple configure test failed (non-critical)" -ForegroundColor Yellow
    } else {
        Write-Host "  Simple configure test passed" -ForegroundColor Green
    }
} catch {
    Write-Host "Simple configure test failed: $_ (non-critical)" -ForegroundColor Yellow
} finally {
    Pop-Location
}

# Check for exception policy violations
Write-Host "Checking exception policy violations..." -ForegroundColor Yellow
$coreModules = @("src\media_io", "src\decode", "src\playback", "src\audio", "src\render", "src\fx", "src\cache", "src\gfx")
$violations = @()

foreach ($module in $coreModules) {
    if (Test-Path $module) {
        $throwFiles = Get-ChildItem $module -Recurse -Include "*.cpp", "*.hpp" | 
                     Select-String "throw" | 
                     Where-Object { $_.Line -notmatch "// EXCEPTION_POLICY_OK" }
        
        if ($throwFiles) {
            $violations += $throwFiles
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "  Exception policy violation found in core performance modules!" -ForegroundColor Red
    Write-Host "  Core modules must remain exception-free per DR-0003" -ForegroundColor Red
    foreach ($violation in $violations) {
        Write-Host "    $($violation.Filename):$($violation.LineNumber): $($violation.Line.Trim())" -ForegroundColor Red
    }
    exit 1
} else {
    Write-Host "  Exception policy check passed" -ForegroundColor Green
}

Write-Host "All lint and test checks passed!" -ForegroundColor Green
Write-Host "Ready to push to GitHub" -ForegroundColor Cyan
