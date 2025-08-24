# Video Editor Launcher Script
Write-Host "Starting Qt6 Video Editor..." -ForegroundColor Green
Write-Host ""

# Check if video editor executable exists (Release version)
$videoEditorPath = Join-Path $PSScriptRoot "build\simple-debug\src\tools\video_editor\Release\video_editor.exe"

if (-not (Test-Path $videoEditorPath)) {
    Write-Host "Error: Video editor executable not found at:" -ForegroundColor Red
    Write-Host $videoEditorPath -ForegroundColor Red
    Write-Host ""
    Write-Host "Please build the project first using:" -ForegroundColor Yellow
    Write-Host "cmake --build build\simple-debug --config Release --target video_editor" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Press any key to continue..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

# Check if Qt6 DLLs are present (Release version)
$qt6CorePath = Join-Path $PSScriptRoot "build\simple-debug\src\tools\video_editor\Release\Qt6Core.dll"
if (-not (Test-Path $qt6CorePath)) {
    Write-Host "Warning: Qt6 DLLs not found. Adding vcpkg bin to PATH..." -ForegroundColor Yellow
    
    # Add Qt6 and other dependencies to PATH for this session
    $vcpkgBin = Join-Path $PSScriptRoot "vcpkg_installed\x64-windows\bin"
    if (Test-Path $vcpkgBin) {
        $env:PATH += ";$vcpkgBin"
        Write-Host "Added $vcpkgBin to PATH" -ForegroundColor Green
    } else {
        Write-Host "Error: vcpkg bin directory not found: $vcpkgBin" -ForegroundColor Red
        Write-Host "Press any key to continue..."
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        exit 1
    }
}

# Launch the video editor
try {
    Write-Host "Launching video editor..." -ForegroundColor Green
    & $videoEditorPath
    Write-Host ""
    Write-Host "Video Editor closed successfully." -ForegroundColor Green
}
catch {
    Write-Host "Error launching Video Editor: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "This may be due to missing dependencies. Try:" -ForegroundColor Yellow
    Write-Host "1. Rebuilding the project: cmake --build build\simple-debug --config Release --target video_editor" -ForegroundColor Yellow
    Write-Host "2. Installing Visual C++ Redistributable if not already installed" -ForegroundColor Yellow
}

Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
