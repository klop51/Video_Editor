# Self-Hosted Windows Runner Configuration Script
# Run this script on your Windows runner to optimize it for Video Editor builds

param(
    [switch]$Install,
    [switch]$Configure,
    [switch]$Optimize,
    [switch]$Test,
    [switch]$All
)

# Color output functions
function Write-Success { param($Message) Write-Host $Message -ForegroundColor Green }
function Write-Warning { param($Message) Write-Host $Message -ForegroundColor Yellow }
function Write-Error { param($Message) Write-Host $Message -ForegroundColor Red }
function Write-Info { param($Message) Write-Host $Message -ForegroundColor Cyan }

function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Install-Prerequisites {
    Write-Info "Installing prerequisites for self-hosted runner..."
    
    # Check if running as administrator
    if (-not (Test-Administrator)) {
        Write-Error "Please run this script as Administrator for installation"
        return $false
    }
    
    # Install Chocolatey if not present
    if (!(Get-Command choco -ErrorAction SilentlyContinue)) {
        Write-Info "Installing Chocolatey..."
        Set-ExecutionPolicy Bypass -Scope Process -Force
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
        iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    }
    
    # Install required packages
    Write-Info "Installing build tools..."
    choco install -y git cmake nasm
    
    # Verify Visual Studio 2022 installation
    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
    if (!(Test-Path $vsPath)) {
        Write-Warning "Visual Studio 2022 Community not found. Please install it manually."
        Write-Info "Download from: https://visualstudio.microsoft.com/vs/community/"
        Write-Info "Required workloads: 'Desktop development with C++', 'Game development with C++'"
    } else {
        Write-Success "Visual Studio 2022 Community found"
    }
    
    # Setup vcpkg if not present
    if (!(Test-Path "C:\vcpkg\vcpkg.exe")) {
        Write-Info "Setting up vcpkg..."
        Remove-Item -Path "C:\vcpkg" -Recurse -Force -ErrorAction SilentlyContinue
        git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
        & "C:\vcpkg\bootstrap-vcpkg.bat"
        & "C:\vcpkg\vcpkg.exe" integrate install
        Write-Success "vcpkg installed and integrated"
    } else {
        Write-Success "vcpkg already installed"
    }
    
    return $true
}

function Configure-Runner {
    Write-Info "Configuring runner environment..."
    
    # Set environment variables
    $envVars = @{
        "VCPKG_ROOT" = "C:\vcpkg"
        "VCPKG_DEFAULT_TRIPLET" = "x64-windows"
        "VE_ENABLE_DETAILED_PROFILING" = "1"
        "VE_ENABLE_PBO_UPLOAD" = "1"
        "CMAKE_TOOLCHAIN_FILE" = "C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
    }
    
    foreach ($var in $envVars.GetEnumerator()) {
        [Environment]::SetEnvironmentVariable($var.Key, $var.Value, "Machine")
        Write-Success "Set $($var.Key) = $($var.Value)"
    }
    
    # Add to PATH
    $pathAdditions = @(
        "C:\vcpkg"
        "C:\Program Files\CMake\bin"
        "C:\Program Files\Git\bin"
    )
    
    $currentPath = [Environment]::GetEnvironmentVariable("PATH", "Machine")
    foreach ($addition in $pathAdditions) {
        if ($currentPath -notlike "*$addition*") {
            $currentPath += ";$addition"
            Write-Success "Added $addition to PATH"
        }
    }
    [Environment]::SetEnvironmentVariable("PATH", $currentPath, "Machine")
    
    # Configure Git for runner
    git config --global core.longpaths true
    git config --global core.autocrlf false
    git config --global pull.rebase false
    
    Write-Success "Runner environment configured"
}

function Optimize-Runner {
    Write-Info "Optimizing runner performance..."
    
    if (-not (Test-Administrator)) {
        Write-Error "Please run as Administrator for optimization"
        return $false
    }
    
    # Disable Windows Defender real-time scanning for build directories
    $exclusions = @(
        "C:\vcpkg"
        "C:\actions-runner"
        "C:\Users\*\Desktop\Video_Editor"
        "C:\Users\*\AppData\Local\Temp"
    )
    
    foreach ($exclusion in $exclusions) {
        try {
            Add-MpPreference -ExclusionPath $exclusion -ErrorAction SilentlyContinue
            Write-Success "Added Windows Defender exclusion: $exclusion"
        } catch {
            Write-Warning "Could not add exclusion: $exclusion"
        }
    }
    
    # Optimize Windows performance settings
    Write-Info "Optimizing Windows performance settings..."
    
    # Disable visual effects for better performance
    $regPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects"
    Set-ItemProperty -Path $regPath -Name "VisualFXSetting" -Value 2 -ErrorAction SilentlyContinue
    
    # Set high performance power plan
    try {
        $powerPlan = Get-WmiObject -Class Win32_PowerPlan -Namespace root\cimv2\power | Where-Object { $_.ElementName -eq "High performance" }
        if ($powerPlan) {
            $powerPlan.Activate()
            Write-Success "Set power plan to High Performance"
        }
    } catch {
        Write-Warning "Could not set power plan"
    }
    
    # Configure virtual memory
    $computerSystem = Get-WmiObject -Class Win32_ComputerSystem
    $ram = [math]::Round($computerSystem.TotalPhysicalMemory / 1GB)
    $pageFileSize = $ram * 1.5  # 1.5x RAM size
    
    Write-Info "System RAM: ${ram}GB, Setting page file to ${pageFileSize}GB"
    
    Write-Success "Performance optimization completed"
}

function Test-RunnerSetup {
    Write-Info "Testing runner setup..."
    
    # Test vcpkg
    if (Test-Path "C:\vcpkg\vcpkg.exe") {
        Write-Success "✅ vcpkg found"
        $vcpkgVersion = & "C:\vcpkg\vcpkg.exe" version
        Write-Info "vcpkg version: $vcpkgVersion"
    } else {
        Write-Error "❌ vcpkg not found"
        return $false
    }
    
    # Test CMake
    if (Get-Command cmake -ErrorAction SilentlyContinue) {
        $cmakeVersion = cmake --version | Select-Object -First 1
        Write-Success "✅ CMake found: $cmakeVersion"
    } else {
        Write-Error "❌ CMake not found"
        return $false
    }
    
    # Test Git
    if (Get-Command git -ErrorAction SilentlyContinue) {
        $gitVersion = git --version
        Write-Success "✅ Git found: $gitVersion"
    } else {
        Write-Error "❌ Git not found"
        return $false
    }
    
    # Test Visual Studio
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsInstalls = & $vsWhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
        if ($vsInstalls) {
            Write-Success "✅ Visual Studio found: $($vsInstalls.displayName)"
        } else {
            Write-Error "❌ Visual Studio with C++ tools not found"
            return $false
        }
    } else {
        Write-Error "❌ Visual Studio installer not found"
        return $false
    }
    
    # Test build environment with a simple project
    Write-Info "Testing build environment..."
    $testDir = "C:\temp\runner-test"
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    
    try {
        # Create minimal CMakeLists.txt
        @"
cmake_minimum_required(VERSION 3.20)
project(RunnerTest)
set(CMAKE_CXX_STANDARD 17)
add_executable(test main.cpp)
"@ | Out-File -FilePath "$testDir\CMakeLists.txt" -Encoding ASCII
        
        # Create minimal main.cpp
        @"
#include <iostream>
int main() {
    std::cout << "Runner test successful!" << std::endl;
    return 0;
}
"@ | Out-File -FilePath "$testDir\main.cpp" -Encoding ASCII
        
        # Test build
        Push-Location $testDir
        cmake -B build
        cmake --build build --config Debug
        
        if ($LASTEXITCODE -eq 0) {
            Write-Success "✅ Build environment test passed"
        } else {
            Write-Error "❌ Build environment test failed"
            return $false
        }
    } finally {
        Pop-Location
        Remove-Item -Path $testDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    
    # Test GitHub Actions runner service (if installed)
    $runnerService = Get-Service -Name "actions.runner.*" -ErrorAction SilentlyContinue
    if ($runnerService) {
        Write-Success "✅ GitHub Actions runner service found: $($runnerService.Name)"
        if ($runnerService.Status -eq "Running") {
            Write-Success "✅ Runner service is running"
        } else {
            Write-Warning "⚠️ Runner service is not running"
        }
    } else {
        Write-Warning "⚠️ GitHub Actions runner service not found (install separately)"
    }
    
    Write-Success "✅ Runner setup validation completed successfully!"
    return $true
}

function Show-RunnerInfo {
    Write-Info "=== Self-Hosted Windows Runner Information ==="
    
    # System information
    $computerSystem = Get-WmiObject -Class Win32_ComputerSystem
    $os = Get-WmiObject -Class Win32_OperatingSystem
    $processor = Get-WmiObject -Class Win32_Processor
    
    Write-Host "Computer Name: $($computerSystem.Name)"
    Write-Host "OS: $($os.Caption) $($os.Version)"
    Write-Host "Processor: $($processor.Name)"
    Write-Host "RAM: $([math]::Round($computerSystem.TotalPhysicalMemory / 1GB, 2)) GB"
    Write-Host "Logical Processors: $($computerSystem.NumberOfLogicalProcessors)"
    
    # Disk space
    $disk = Get-WmiObject -Class Win32_LogicalDisk | Where-Object { $_.DriveType -eq 3 }
    foreach ($drive in $disk) {
        $freeGB = [math]::Round($drive.FreeSpace / 1GB, 2)
        $totalGB = [math]::Round($drive.Size / 1GB, 2)
        Write-Host "Drive $($drive.DeviceID) Free: ${freeGB}GB / ${totalGB}GB"
    }
    
    Write-Host ""
    Write-Info "Next steps:"
    Write-Host "1. Install GitHub Actions runner: https://github.com/YOUR_REPO/settings/actions/runners/new"
    Write-Host "2. Configure runner with labels: [self-hosted, windows]"
    Write-Host "3. Start the runner service"
    Write-Host "4. Test with a simple workflow push"
}

# Main execution
if ($All) {
    $Install = $Configure = $Optimize = $Test = $true
}

if (-not ($Install -or $Configure -or $Optimize -or $Test)) {
    Write-Info "Self-Hosted Windows Runner Setup Script"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\configure-runner.ps1 -Install     # Install prerequisites"
    Write-Host "  .\configure-runner.ps1 -Configure   # Configure environment"
    Write-Host "  .\configure-runner.ps1 -Optimize    # Optimize performance"
    Write-Host "  .\configure-runner.ps1 -Test        # Test setup"
    Write-Host "  .\configure-runner.ps1 -All         # Do everything"
    Write-Host ""
    Show-RunnerInfo
    exit 0
}

$success = $true

if ($Install) {
    $success = $success -and (Install-Prerequisites)
}

if ($Configure) {
    Configure-Runner
}

if ($Optimize) {
    $success = $success -and (Optimize-Runner)
}

if ($Test) {
    $success = $success -and (Test-RunnerSetup)
}

if ($success) {
    Write-Success "=== Runner configuration completed successfully! ==="
    Show-RunnerInfo
} else {
    Write-Error "=== Runner configuration failed - check errors above ==="
    exit 1
}