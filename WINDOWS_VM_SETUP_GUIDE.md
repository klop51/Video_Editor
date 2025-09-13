# Windows VM Setup Guide for Video Editor Development

> Complete guide for setting up a fresh Windows VM as a development environment and runner for the C++ Video Editor project.

## Prerequisites

- Windows 10/11 VM with at least:
  - **8GB RAM** (16GB recommended for 4K video processing)
  - **100GB+ free disk space** (for tools, dependencies, and builds)
  - **GPU passthrough enabled** (if available, for hardware acceleration testing)
  - **Network access** for downloading dependencies

---

## Phase 1: Core Development Tools (Required)

### 1. Visual Studio 2022 Community 🔧
**Download**: https://visualstudio.microsoft.com/vs/community/

**Required Workloads:**
- **Desktop development with C++**
- **Game development with C++** (for DirectX support)

**Individual Components to Add:**
- Windows 10/11 SDK (latest version)
- CMake tools for Visual Studio
- Git for Windows
- MSVC v143 compiler toolset
- IntelliSense support

**Installation Steps:**
```powershell
# Run VS installer and select workloads above
# Ensure CMake and Git are included
```

### 2. Git Configuration 🌐
```powershell
# Configure Git (replace with your details)
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
git config --global init.defaultBranch main

# Enable long paths for Windows (required for deep dependency trees)
git config --global core.longpaths true
```

### 3. CMake (Latest Version) 🏗️
**Download**: https://cmake.org/download/

```powershell
# Add to PATH during installation
# Verify installation
cmake --version
```

### 4. vcpkg Package Manager 📦
```powershell
# Clone vcpkg to C:\vcpkg (standard location)
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# Integrate with Visual Studio
.\vcpkg integrate install

# Add to PATH
# Add C:\vcpkg to your system PATH environment variable
```

---

## Phase 2: Project Dependencies Setup

### 1. Clone the Video Editor Repository 📥
```powershell
# Navigate to your development directory
cd C:\Users\%USERNAME%\Desktop
git clone https://github.com/klop51/Video_Editor.git
cd Video_Editor
```

### 2. Install Project Dependencies via vcpkg 🔧
```powershell
# The project uses vcpkg.json manifest mode
# Dependencies will be installed automatically during CMake configure
# But you can pre-install them:

.\vcpkg install --triplet x64-windows

# Key dependencies that will be installed:
# - Qt6 (with widgets, multimedia)
# - FFmpeg (video processing)
# - spdlog (logging)
# - fmt (formatting)
# - Catch2 (testing)
# - gtest (additional testing)
```

---

## Phase 3: Environment Configuration

### 1. Environment Variables 🌍
Add these to your system environment variables:

```powershell
# Essential paths
VCPKG_ROOT=C:\vcpkg
PATH=%PATH%;C:\vcpkg;C:\Program Files\CMake\bin

# Optional but recommended for development
VE_ENABLE_DETAILED_PROFILING=1
VE_ENABLE_PBO_UPLOAD=1
```

### 2. PowerShell Execution Policy 🔐
```powershell
# Enable script execution (run as Administrator)
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope LocalMachine
```

### 3. Windows SDK Verification 🪟
```powershell
# Check Windows SDK installation
dir "C:\Program Files (x86)\Windows Kits\10\Include"
# Should show SDK versions like 10.0.26100.0
```

---

## Phase 4: Build Configuration & Testing

### 1. Initial CMake Configuration 🏗️
```powershell
cd C:\Users\%USERNAME%\Desktop\Video_Editor

# Configure for Qt Debug build
cmake --preset qt-debug

# This will:
# - Download and install all vcpkg dependencies
# - Configure Qt6 integration
# - Set up DirectX 11 support
# - Enable profiling and debugging features
```

### 2. Test Build 🧪
```powershell
# Build the entire project
cmake --build build/qt-debug --config Debug

# Should complete successfully with output like:
# "video_editor.vcxproj -> ...video_editor.exe"
```

### 3. Run Basic Tests ✅
```powershell
# Run unit tests
cd build\qt-debug\tests\Debug
.\unit_tests.exe

# Run GPU system validation
.\gpu_system_validation.exe

# Test media probe tool
cd ..\..\..\src\tools\media_probe\Debug
.\ve_media_probe.exe --help
```

---

## Phase 5: Optional Development Tools

### 1. Advanced IDEs (Optional) 💻
**Visual Studio Code** with extensions:
- C/C++ (Microsoft)
- CMake Tools
- Git Graph
- Qt for Python (for Qt preview)

**Qt Creator** (if you prefer Qt-specific IDE):
- Download from Qt website
- Configure to use your vcpkg Qt installation

### 2. Debugging Tools 🐛
**Visual Studio Debugger** (included with VS 2022)
**Intel VTune Profiler** (for performance analysis):
- Download from Intel website
- Useful for GPU performance optimization

### 3. Graphics Development Tools 🎨
**Intel Graphics Command Center** (for GPU debugging)
**NVIDIA Nsight Graphics** (if using NVIDIA GPU)
**RenderDoc** (for graphics debugging)

---

## Phase 6: VM-Specific Optimizations

### 1. VM Performance Tuning ⚡
```powershell
# Disable Windows visual effects for better performance
# Run: SystemPropertiesAdvanced.exe
# Performance Settings -> Adjust for best performance

# Disable Windows Defender real-time scanning for build folders
# Add exclusions for:
# - C:\Users\%USERNAME%\Desktop\Video_Editor\build
# - C:\vcpkg
# - C:\Users\%USERNAME%\.vcpkg
```

### 2. Memory Configuration 🧠
- **VM Settings**: Allocate at least 8GB RAM to VM
- **Virtual Memory**: Set Windows paging file to system managed
- **Build Parallelism**: Limit parallel builds if memory constrained:
```powershell
# Build with limited parallelism
cmake --build build/qt-debug --config Debug --parallel 4
```

### 3. GPU Acceleration (If Available) 🚀
```powershell
# Check DirectX support
dxdiag

# Test hardware acceleration
cd build\qt-debug\tests\Debug
.\gpu_test_suite.exe
```

---

## Phase 7: Validation & Troubleshooting

### 1. Complete Build Test 🏁
```powershell
# Clean build test
Remove-Item -Recurse -Force build\qt-debug -ErrorAction SilentlyContinue
cmake --preset qt-debug
cmake --build build/qt-debug --config Debug

# Should complete without errors
```

### 2. Run the Video Editor 🎬
```powershell
cd build\qt-debug\src\tools\video_editor\Debug
.\video_editor.exe

# Should open the Qt GUI without errors
# Test by importing a video file
```

### 3. Performance Validation 📊
```powershell
# Run performance tests
cd build\qt-debug\tests\Debug
.\performance_optimizer_validation.exe

# Check profiling output
type ..\..\..\profiling.json
```

---

## Common Issues & Solutions

### Issue 1: vcpkg Dependencies Fail
**Solution:**
```powershell
# Clear vcpkg cache and reinstall
.\vcpkg remove --triplet x64-windows
.\vcpkg install --triplet x64-windows --recurse
```

### Issue 2: Qt Platform Plugin Errors
**Solution:**
```powershell
# Ensure Qt plugins are in the right location
# The CMakeLists.txt should handle this automatically
# If not, manually copy:
copy "build\qt-debug\vcpkg_installed\x64-windows\debug\Qt6\plugins\platforms\*.dll" "build\qt-debug\src\tools\video_editor\Debug\platforms\"
```

### Issue 3: DirectX/GPU Errors
**Solution:**
```powershell
# Install latest DirectX runtime
# Download from Microsoft
# Update GPU drivers in VM
```

### Issue 4: Build Fails with "Access Denied"
**Solution:**
```powershell
# Run as Administrator
# Disable antivirus real-time scanning for build folders
# Check file permissions on project directory
```

### Issue 5: CMake Can't Find Dependencies
**Solution:**
```powershell
# Verify vcpkg integration
.\vcpkg integrate install

# Check CMake can find vcpkg
cmake -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ..
```

---

## Automated Setup Script

Create `setup_vm.ps1` for automated installation:

```powershell
# Windows VM Setup Script for Video Editor Development
param(
    [string]$ProjectPath = "C:\Users\$env:USERNAME\Desktop\Video_Editor"
)

Write-Host "Setting up Windows VM for Video Editor development..." -ForegroundColor Green

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "Please run as Administrator" -ForegroundColor Red
    exit 1
}

# Step 1: Enable long paths
git config --global core.longpaths true

# Step 2: Set execution policy
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope LocalMachine -Force

# Step 3: Clone repository if not exists
if (!(Test-Path $ProjectPath)) {
    Write-Host "Cloning repository..." -ForegroundColor Yellow
    git clone https://github.com/klop51/Video_Editor.git $ProjectPath
}

# Step 4: Setup vcpkg if not exists
if (!(Test-Path "C:\vcpkg")) {
    Write-Host "Setting up vcpkg..." -ForegroundColor Yellow
    cd C:\
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    .\bootstrap-vcpkg.bat
    .\vcpkg integrate install
}

# Step 5: Configure and build
cd $ProjectPath
Write-Host "Configuring project..." -ForegroundColor Yellow
cmake --preset qt-debug

Write-Host "Building project..." -ForegroundColor Yellow
cmake --build build/qt-debug --config Debug

Write-Host "Setup complete! Run tests to validate installation." -ForegroundColor Green
```

---

## Quick Start Commands

Once VM is set up, use these for daily development:

```powershell
# Daily development workflow
cd C:\Users\%USERNAME%\Desktop\Video_Editor

# Pull latest changes
git pull

# Clean build
cmake --build build/qt-debug --config Debug --target clean
cmake --build build/qt-debug --config Debug

# Run tests
cd build\qt-debug\tests\Debug && .\unit_tests.exe

# Run application
cd ..\..\..\src\tools\video_editor\Debug && .\video_editor.exe
```

---

## Success Criteria ✅

Your VM setup is complete when:

1. ✅ Visual Studio 2022 builds the project without errors
2. ✅ All unit tests pass
3. ✅ GPU validation tests complete successfully  
4. ✅ Video editor GUI opens and can import video files
5. ✅ No missing DLL or dependency errors
6. ✅ Build completes in under 10 minutes (performance baseline)

---

*This guide ensures your Windows VM can build, test, and run the professional video editor with all dependencies properly configured.*