# Test script for UI responsiveness improvements
# Run this script to quickly launch the video editor and test the improvements

Write-Host "Starting Video Editor with UI Responsiveness Improvements..." -ForegroundColor Green
Write-Host ""
Write-Host "Test steps to verify improvements:" -ForegroundColor Yellow
Write-Host "1. Create a new project (File -> New Project)"
Write-Host "2. Import a media file (File -> Import Media)"
Write-Host "3. Add media to timeline (right-click media -> Add to Timeline)"
Write-Host "4. Watch for reduced freezing during 'add to timeline' operation"
Write-Host ""
Write-Host "Key improvements implemented:" -ForegroundColor Cyan
Write-Host "- Chunked timeline processing (12ms budget per frame)"
Write-Host "- Scoped timing to detect GUI thread blocks >16ms"
Write-Host "- Viewport culling for timeline drawing"
Write-Host "- Reduced drawing complexity for small segments"
Write-Host "- Optimized waveform and thumbnail rendering"
Write-Host ""

# Launch the video editor
$editorPath = ".\build\qt-debug\src\tools\video_editor\Debug\video_editor.exe"

if (Test-Path $editorPath) {
    Write-Host "Launching video editor..." -ForegroundColor Green
    & $editorPath
} else {
    Write-Host "Error: Video editor not found at $editorPath" -ForegroundColor Red
    Write-Host "Please build the project first with:" -ForegroundColor Yellow
    Write-Host "cmake --build build/qt-debug --config Debug"
}
