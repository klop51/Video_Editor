# GitHub Repository Setup Script
# Run this after creating a new repository on GitHub

# Instructions:
# 1. Go to GitHub.com and create a new repository
# 2. Copy the repository URL (e.g., https://github.com/yourusername/qt6-video-editor-responsive.git)
# 3. Run this script in PowerShell
# 4. Replace YOUR_REPO_URL with your actual GitHub repository URL

param(
    [Parameter(Mandatory=$true)]
    [string]$RepoUrl
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Qt6 Video Editor Repository Setup" -ForegroundColor Cyan  
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Navigate to project directory
$ProjectDir = "c:\Users\braul\Desktop\Video_Editor"
Set-Location $ProjectDir

Write-Host "Adding remote origin: $RepoUrl" -ForegroundColor Green
git remote add origin $RepoUrl

Write-Host "Setting up main branch..." -ForegroundColor Green
git branch -M main

Write-Host "Pushing to GitHub..." -ForegroundColor Green
git push -u origin main

Write-Host ""
Write-Host "✅ Repository successfully pushed to GitHub!" -ForegroundColor Green
Write-Host ""
Write-Host "📁 Repository contains:" -ForegroundColor Yellow
Write-Host "   • Complete Qt6 Video Editor source code" -ForegroundColor White
Write-Host "   • UI responsiveness solutions (worker threads, chunked processing)" -ForegroundColor White
Write-Host "   • Optimized paint events with caching" -ForegroundColor White
Write-Host "   • CMake build system with vcpkg integration" -ForegroundColor White
Write-Host "   • Debug tools and performance monitoring" -ForegroundColor White
Write-Host "   • Comprehensive documentation (REPRO.md)" -ForegroundColor White
Write-Host ""
Write-Host "🚀 Next steps:" -ForegroundColor Yellow
Write-Host "   1. Share the repository URL" -ForegroundColor White
Write-Host "   2. Others can clone and build using: cmake --preset qt-debug" -ForegroundColor White
Write-Host "   3. Run with: .\run_video_editor_debug.bat" -ForegroundColor White
Write-Host ""
Write-Host "📖 Key files for reviewers:" -ForegroundColor Yellow
Write-Host "   • REPRO.md - Complete reproduction guide" -ForegroundColor White
Write-Host "   • README_REPO.md - Repository overview" -ForegroundColor White
Write-Host "   • src/ui/src/main_window.cpp - Worker thread implementation" -ForegroundColor White
Write-Host "   • src/ui/src/timeline_panel.cpp - Paint optimizations" -ForegroundColor White
Write-Host ""

# Example usage information
Write-Host "📋 Example usage:" -ForegroundColor Cyan
Write-Host "   .\setup_github_repo.ps1 -RepoUrl 'https://github.com/yourusername/qt6-video-editor-responsive.git'" -ForegroundColor Gray
