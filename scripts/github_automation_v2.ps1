# GitHub CI Automation Script
param(
    [string]$Action = "monitor",
    [string]$Owner = "klop51", 
    [string]$Repo = "Video_Editor",
    [int]$CheckIntervalSeconds = 30,
    [switch]$DownloadArtifacts,
    [switch]$ShowLogs,
    [switch]$Continuous
)

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
            Write-ColorOutput "[OK] GitHub CLI available" "Success"
            return $true
        } else {
            Write-ColorOutput "[ERROR] GitHub CLI not found" "Error"
            return $false
        }
    }
    catch {
        Write-ColorOutput "[ERROR] GitHub CLI not found. Install from: https://cli.github.com/" "Error"
        return $false
    }
}

function Get-CIStatus {
    param([string]$Owner, [string]$Repo)
    
    try {
        $jsonOutput = gh run list --repo "$Owner/$Repo" --limit 5 --json conclusion,status,workflowName,createdAt,headBranch
        $runs = $jsonOutput | ConvertFrom-Json
        
        Write-ColorOutput "`n[CI] Recent CI Runs for $Owner/$Repo" "Header"
        Write-ColorOutput "============================================" "Header"
        
        foreach ($run in $runs) {
            $status = if ($run.conclusion) { $run.conclusion } else { $run.status }
            
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
            
            $time = [DateTime]::Parse($run.createdAt).ToString("MM/dd HH:mm")
            Write-ColorOutput "$icon $($run.workflowName) [$status] - $($run.headBranch) ($time)" $color
        }
        
        return $runs[0]
    }
    catch {
        Write-ColorOutput "[ERROR] Failed to get CI status: $_" "Error"
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
        Write-ColorOutput "[ERROR] Failed to get logs: $_" "Error"
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
        
        $files = Get-ChildItem -Path $artifactDir -Recurse -File
        if ($files.Count -gt 0) {
            Write-ColorOutput "`n[FILES] Downloaded files:" "Info"
            foreach ($file in $files) {
                Write-ColorOutput "  - $($file.FullName)" "Info"
            }
        }
    }
    catch {
        Write-ColorOutput "[ERROR] Failed to download artifacts: $_" "Error"
    }
}

function Start-ContinuousMonitor {
    param([string]$Owner, [string]$Repo, [int]$IntervalSeconds)
    
    Write-ColorOutput "`n[MONITOR] Starting continuous monitoring..." "Header"
    Write-ColorOutput "Checking every $IntervalSeconds seconds (Ctrl+C to stop)`n" "Info"
    
    $lastStatus = $null
    
    while ($true) {
        Clear-Host
        Write-ColorOutput "[MONITOR] GitHub CI Monitor - $(Get-Date)" "Header"
        
        $currentRun = Get-CIStatus -Owner $Owner -Repo $Repo
        
        if ($currentRun) {
            $currentStatus = if ($currentRun.conclusion) { $currentRun.conclusion } else { $currentRun.status }
            
            if ($lastStatus -and $lastStatus -ne $currentStatus) {
                Write-ColorOutput "`n[ALERT] Status changed from '$lastStatus' to '$currentStatus'!" "Warning"
                
                if ($currentStatus -eq "failure") {
                    Get-Artifacts -Owner $Owner -Repo $Repo
                    Get-FailureLogs -Owner $Owner -Repo $Repo
                }
            }
            
            $lastStatus = $currentStatus
        }
        
        Write-ColorOutput "`n[WAIT] Next check in $IntervalSeconds seconds..." "Info"
        Start-Sleep -Seconds $IntervalSeconds
    }
}

function Show-Help {
    Write-ColorOutput "GitHub CI Automation Tool" "Header"
    Write-ColorOutput "USAGE: .\github_automation.ps1 [ACTION]" "Info"
    Write-ColorOutput "ACTIONS:" "Info"
    Write-ColorOutput "  monitor     Show current CI status (default)" "Info"
    Write-ColorOutput "  logs        Show failure logs" "Info"
    Write-ColorOutput "  artifacts   Download CI artifacts" "Info"
    Write-ColorOutput "  continuous  Start continuous monitoring" "Info"
    Write-ColorOutput "  help        Show this help" "Info"
}

# Main execution
Write-ColorOutput "[START] GitHub CI Automation Tool" "Header"

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
        Write-ColorOutput "[ERROR] Unknown action: $Action" "Error"
        Show-Help
        exit 1
    }
}

if ($Continuous) {
    Start-ContinuousMonitor -Owner $Owner -Repo $Repo -IntervalSeconds $CheckIntervalSeconds
}
