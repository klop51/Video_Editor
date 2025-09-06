# GitHub CI Automation Script
# Monitors CI status, downloads artifacts, and provides notifications

param(
    [string]$Action = "monitor",
    [string]$Owner = "klop51", 
    [string]$Repo = "Video_Editor",
    [int]$CheckIntervalSeconds = 30,
    [switch]$DownloadArtifacts,
    [switch]$ShowLogs,
    [switch]$Continuous
)

# Colors for output
$Colors = @{
    Success = "Green"
    Warning = "Yellow" 
    Error = "Red"
    Info = "Cyan"
    Header = "Magenta"
}

function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Colors[$Color]
}

function Test-GitHubCLI {
    try {
        $version = gh --version 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-ColorOutput "[OK] GitHub CLI available: $($version[0])" "Success"
            return $true
        }
    }
    catch {
        Write-ColorOutput "X GitHub CLI not found. Install from: https://cli.github.com/" "Error"
        return $false
    }
    return $false
}

function Get-CIStatus {
    param([string]$Owner, [string]$Repo)
    
    try {
        $runs = gh run list --repo "$Owner/$Repo" --limit 5 --json conclusion,status,workflowName,createdAt,headBranch,event,url | ConvertFrom-Json
        
        Write-ColorOutput "`n[CI] Recent CI Runs for $Owner/$Repo" "Header"
        Write-ColorOutput "=" * 60 "Header"
        
        foreach ($run in $runs) {
            $status = $run.conclusion
            if ([string]::IsNullOrEmpty($status)) { $status = $run.status }
            
            $color = switch ($status) {
                "success" { "Success" }
                "failure" { "Error" }
                "cancelled" { "Warning" }
                "in_progress" { "Info" }
                "queued" { "Info" }
                default { "Warning" }
            }
            
            $icon = switch ($status) {
                "success" { "[OK]" }
                "failure" { "[FAIL]" }
                "cancelled" { "[CANCEL]" }
                "in_progress" { "[RUNNING]" }
                "queued" { "[QUEUED]" }
                default { "[UNKNOWN]" }
            }
            
            $time = [DateTime]::Parse($run.createdAt).ToString('MM/dd HH:mm')
            Write-ColorOutput "$icon $($run.workflowName) [$status] - $($run.headBranch) ($time)" $color
        }
        
        return $runs[0] # Return latest run
    }
    catch {
        Write-ColorOutput "❌ Failed to get CI status: $_" "Error"
        return $null
    }
}

function Get-FailureLogs {
    param([string]$Owner, [string]$Repo)
    
    Write-ColorOutput "`n[LOGS] Getting latest failure logs..." "Info"
    try {
        gh run view --repo "$Owner/$Repo" --log
    }
    catch {
        Write-ColorOutput "❌ Failed to get logs: $_" "Error"
    }
}

function Get-Artifacts {
    param([string]$Owner, [string]$Repo)
    
    $artifactDir = "ci_artifacts_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
    
    Write-ColorOutput "`n[DOWNLOAD] Downloading CI artifacts to $artifactDir..." "Info"
    try {
        New-Item -ItemType Directory -Path $artifactDir -Force | Out-Null
        gh run download --repo "$Owner/$Repo" --dir $artifactDir
        Write-ColorOutput "[SUCCESS] Artifacts downloaded to: $artifactDir" "Success"
        
        # List downloaded files
        $files = Get-ChildItem -Path $artifactDir -Recurse -File
        if ($files.Count -gt 0) {
            Write-ColorOutput "`n📁 Downloaded files:" "Info"
            foreach ($file in $files) {
                Write-ColorOutput "  - $($file.FullName)" "Info"
            }
        }
    }
    catch {
        Write-ColorOutput "❌ Failed to download artifacts: $_" "Error"
    }
}

function Start-ContinuousMonitor {
    param([string]$Owner, [string]$Repo, [int]$IntervalSeconds)
    
    Write-ColorOutput "`n🔄 Starting continuous monitoring (Ctrl+C to stop)..." "Header"
    Write-ColorOutput "Checking every $IntervalSeconds seconds`n" "Info"
    
    $lastStatus = $null
    
    while ($true) {
        Clear-Host
        Write-ColorOutput "🔍 GitHub CI Monitor - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "Header"
        
        $currentRun = Get-CIStatus -Owner $Owner -Repo $Repo
        
        if ($currentRun) {
            $currentStatus = $currentRun.conclusion
            if ([string]::IsNullOrEmpty($currentStatus)) { $currentStatus = $currentRun.status }
            
            # Notify on status change
            if ($lastStatus -and $lastStatus -ne $currentStatus) {
                Write-ColorOutput "`n🔔 Status changed from '$lastStatus' to '$currentStatus'!" "Warning"
                
                # Auto-download artifacts on failure
                if ($currentStatus -eq "failure") {
                    Get-Artifacts -Owner $Owner -Repo $Repo
                    Get-FailureLogs -Owner $Owner -Repo $Repo
                }
            }
            
            $lastStatus = $currentStatus
        }
        
        Write-ColorOutput "`n⏰ Next check in $IntervalSeconds seconds... (Ctrl+C to stop)" "Info"
        Start-Sleep -Seconds $IntervalSeconds
    }
}

function Show-Help {
    Write-ColorOutput "GitHub CI Automation Tool" "Info"
    Write-ColorOutput "" "Info"
    Write-ColorOutput "USAGE:" "Info"
    Write-ColorOutput "    .\github_automation.ps1 [ACTION] [OPTIONS]" "Info"
    Write-ColorOutput "" "Info"
    Write-ColorOutput "ACTIONS:" "Info"
    Write-ColorOutput "    monitor     Show current CI status (default)" "Info"
    Write-ColorOutput "    logs        Download and show failure logs" "Info"
    Write-ColorOutput "    artifacts   Download CI artifacts" "Info"
    Write-ColorOutput "    continuous  Start continuous monitoring" "Info"
    Write-ColorOutput "    help        Show this help" "Info"
    Write-ColorOutput "" "Info"
    Write-ColorOutput "OPTIONS:" "Info"
    Write-ColorOutput "    -Owner              GitHub owner/org (default: klop51)" "Info"
    Write-ColorOutput "    -Repo               Repository name (default: Video_Editor)" "Info"
    Write-ColorOutput "    -CheckIntervalSeconds    Seconds between checks in continuous mode (default: 30)" "Info"
    Write-ColorOutput "    -DownloadArtifacts  Also download artifacts" "Info"
    Write-ColorOutput "    -ShowLogs           Also show failure logs" "Info"
    Write-ColorOutput "    -Continuous         Start continuous monitoring" "Info"
}

# Main execution
Write-ColorOutput "🚀 GitHub CI Automation Tool" "Header"

if (-not (Test-GitHubCLI)) {
    exit 1
}

switch ($Action.ToLower()) {
    "monitor" { 
        Get-CIStatus -Owner $Owner -Repo $Repo
        if ($ShowLogs) { Get-FailureLogs -Owner $Owner -Repo $Repo }
        if ($DownloadArtifacts) { Get-Artifacts -Owner $Owner -Repo $Repo }
    }
    "logs" { Get-FailureLogs -Owner $Owner -Repo $Repo }
    "artifacts" { Get-Artifacts -Owner $Owner -Repo $Repo }
    "continuous" { Start-ContinuousMonitor -Owner $Owner -Repo $Repo -IntervalSeconds $CheckIntervalSeconds }
    "help" { Show-Help }
    default { 
        Write-ColorOutput "❌ Unknown action: $Action" "Error"
        Show-Help
        exit 1
    }
}

if ($Continuous) {
    Start-ContinuousMonitor -Owner $Owner -Repo $Repo -IntervalSeconds $CheckIntervalSeconds
}
