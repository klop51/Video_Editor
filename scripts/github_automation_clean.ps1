#!/usr/bin/env pwsh

param(
    [string]$Action = "monitor",
    [string]$Owner = "",
    [string]$Repo = "",
    [switch]$Continuous,
    [int]$CheckIntervalSeconds = 30,
    [switch]$Help
)

# Color scheme
$Global:ColorScheme = @{
    "Header" = "Cyan"
    "Info" = "White"
    "Success" = "Green"  
    "Warning" = "Yellow"
    "Error" = "Red"
    "Highlight" = "Magenta"
}

function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    
    if ($Global:ColorScheme.ContainsKey($Color)) {
        Write-Host $Message -ForegroundColor $Global:ColorScheme[$Color]
    } else {
        Write-Host $Message -ForegroundColor $Color
    }
}

function Test-GitHubCLI {
    try {
        $null = Get-Command gh -ErrorAction Stop
        gh auth status 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            return $true
        } else {
            Write-ColorOutput "[ERROR] GitHub CLI authentication failed. Run 'gh auth login'" "Error"
            return $false
        }
    } catch {
        Write-ColorOutput "[ERROR] GitHub CLI not found. Install it from: https://cli.github.com/" "Error"
        return $false
    }
}

function Get-CIStatus {
    param([string]$Owner, [string]$Repo)
    
    Write-ColorOutput "Fetching CI status for $Owner/$Repo..." "Info"
    
    try {
        $runs = gh run list --repo "$Owner/$Repo" --limit 10 --json status,conclusion,workflowName,createdAt,url | ConvertFrom-Json
        
        if ($runs.Count -eq 0) {
            Write-ColorOutput "[INFO] No recent workflow runs found" "Warning"
            return
        }
        
        Write-ColorOutput "`nRecent Workflow Runs:" "Header"
        Write-ColorOutput ("=" * 70) "Header"
        
        foreach ($run in $runs) {
            $status = $run.status
            $conclusion = $run.conclusion
            $workflow = $run.workflowName
            $created = [DateTime]::Parse($run.createdAt).ToString("MM/dd HH:mm")
            
            $statusColor = switch ($conclusion) {
                "success" { "Success" }
                "failure" { "Error" } 
                "cancelled" { "Warning" }
                default { "Info" }
            }
            
            $statusText = if ($status -eq "completed") { $conclusion } else { $status }
            Write-ColorOutput "[$statusText] $workflow ($created)" $statusColor
        }
    } catch {
        Write-ColorOutput "[ERROR] Failed to fetch CI status: $($_.Exception.Message)" "Error"
    }
}

function Get-FailureLogs {
    param([string]$Owner, [string]$Repo)
    
    Write-ColorOutput "Fetching failed workflow runs..." "Info"
    
    try {
        $runs = gh run list --repo "$Owner/$Repo" --limit 20 --json databaseId,status,conclusion,workflowName,createdAt,url | ConvertFrom-Json
        
        if ($runs.Count -eq 0) {
            Write-ColorOutput "[INFO] No workflow runs found" "Warning"
            return
        }
        
        # Show interactive menu
        Show-InteractiveLogMenu -Runs $runs -Owner $Owner -Repo $Repo
        
    } catch {
        Write-ColorOutput "[ERROR] Failed to fetch logs: $($_.Exception.Message)" "Error"
    }
}

function Show-InteractiveLogMenu {
    param($Runs, [string]$Owner, [string]$Repo)
    
    $currentIndex = 0
    $selectedIndices = @()
    $filterText = ""
    $filteredRuns = $Runs
    
    function Draw-FullMenu {
        Clear-Host
        Write-ColorOutput "GitHub CI Log Selection" "Header"
        Write-ColorOutput ("=" * 50) "Header"
        Write-ColorOutput "Controls:" "Info"
        Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
        Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
        Write-ColorOutput ("=" * 50) "Header"
        Write-ColorOutput ""
        
        # Apply filter
        if ($filterText) {
            Write-ColorOutput "Filter: $filterText" "Highlight"
            $script:filteredRuns = $Runs | Where-Object { 
                $_.workflowName -like "*$filterText*" -or 
                $_.conclusion -like "*$filterText*" -or 
                $_.status -like "*$filterText*" 
            }
        } else {
            $script:filteredRuns = $Runs
        }
        
        # Draw log list
        Draw-LogList
        
        # Status line
        Write-ColorOutput ""
        if ($selectedIndices.Count -gt 0) {
            Write-ColorOutput "Selected: $($selectedIndices.Count) runs" "Highlight"
        }
    }
    
    function Draw-LogList {
        if ($filteredRuns.Count -eq 0) {
            Write-ColorOutput "No runs match filter '$filterText'" "Warning"
            return
        }
        
        for ($i = 0; $i -lt $filteredRuns.Count; $i++) {
            $run = $filteredRuns[$i]
            $originalIndex = [Array]::IndexOf($Runs, $run)
            $isSelected = $selectedIndices -contains $originalIndex
            
            $prefix = if ($i -eq $currentIndex) {
                if ($isSelected) { "> [X] " } else { ">     " }
            } else {
                if ($isSelected) { "  [X] " } else { "      " }
            }
            
            $status = if ($run.status -eq "completed") { $run.conclusion } else { $run.status }
            $created = [DateTime]::Parse($run.createdAt).ToString("MM/dd HH:mm")
            
            $statusColor = switch ($run.conclusion) {
                "success" { "Success" }
                "failure" { "Error" }
                "cancelled" { "Warning" }
                default { "Info" }
            }
            
            if ($i -eq $currentIndex) {
                Write-Host $prefix -NoNewline -ForegroundColor Yellow
            } else {
                Write-Host $prefix -NoNewline
            }
            Write-ColorOutput "[$status] $($run.workflowName) ($created)" $statusColor
        }
    }
    
    # Draw initial menu
    Draw-FullMenu
    
    while ($true) {
        $key = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        $oldIndex = $currentIndex
        $redrawNeeded = $false
        
        switch ($key.VirtualKeyCode) {
            38 { # Up arrow
                if ($filteredRuns.Count -gt 0) {
                    $currentIndex = ($currentIndex - 1 + $filteredRuns.Count) % $filteredRuns.Count
                    # Only redraw if we actually moved
                    if ($currentIndex -ne $oldIndex) {
                        Draw-FullMenu
                    }
                }
            }
            40 { # Down arrow
                if ($filteredRuns.Count -gt 0) {
                    $currentIndex = ($currentIndex + 1) % $filteredRuns.Count
                    # Only redraw if we actually moved
                    if ($currentIndex -ne $oldIndex) {
                        Draw-FullMenu
                    }
                }
            }
            32 { # Spacebar
                if ($filteredRuns.Count -gt 0) {
                    $originalIndex = [Array]::IndexOf($Runs, $filteredRuns[$currentIndex])
                    if ($selectedIndices -contains $originalIndex) {
                        $selectedIndices = $selectedIndices | Where-Object { $_ -ne $originalIndex }
                    } else {
                        $selectedIndices += $originalIndex
                    }
                    Draw-FullMenu
                }
            }
            13 { # Enter
                if ($selectedIndices.Count -gt 0) {
                    Write-ColorOutput ""
                    Write-ColorOutput "Downloading selected logs..." "Info"
                    foreach ($index in $selectedIndices) {
                        $run = $Runs[$index]
                        Write-ColorOutput "Downloading logs for: $($run.workflowName)" "Info"
                        try {
                            gh run view $run.databaseId --repo "$Owner/$Repo" --log
                        } catch {
                            Write-ColorOutput "[ERROR] Failed to download logs for run $($run.databaseId): $($_.Exception.Message)" "Error"
                        }
                    }
                    Write-ColorOutput ""
                    Write-ColorOutput "Press any key to continue..." "Info"
                    $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown") | Out-Null
                    return
                } else {
                    Write-ColorOutput "No runs selected" "Warning"
                    Start-Sleep -Milliseconds 500
                    Draw-FullMenu
                }
            }
            8 { # Backspace
                if ($filterText.Length -gt 0) {
                    $filterText = $filterText.Substring(0, $filterText.Length - 1)
                    $currentIndex = 0
                    Draw-FullMenu
                }
            }
            27 { # Escape
                return
            }
            81 { # Q key
                if ($key.Character -eq 'Q' -or $key.Character -eq 'q') {
                    return
                }
            }
            default {
                if ($key.Character -match '[a-zA-Z0-9 ]') {
                    $filterText += $key.Character
                    $currentIndex = 0
                    Draw-FullMenu
                }
            }
        }
    }
}

function Get-Artifacts {
    param([string]$Owner, [string]$Repo)
    
    Write-ColorOutput "Downloading latest CI artifacts for $Owner/$Repo..." "Info"
    
    try {
        gh run download --repo "$Owner/$Repo" --dir ".ci_artifacts"
        Write-ColorOutput "[SUCCESS] Artifacts downloaded to .ci_artifacts/" "Success"
    } catch {
        Write-ColorOutput "[ERROR] Failed to download artifacts: $($_.Exception.Message)" "Error"
    }
}

function Start-ContinuousMonitor {
    param([string]$Owner, [string]$Repo, [int]$IntervalSeconds)
    
    Write-ColorOutput "Starting continuous monitoring (every $IntervalSeconds seconds)" "Info"
    Write-ColorOutput "Press Ctrl+C to stop" "Info"
    
    while ($true) {
        Clear-Host
        Get-CIStatus -Owner $Owner -Repo $Repo
        Start-Sleep -Seconds $IntervalSeconds
    }
}

function Show-Help {
    Write-ColorOutput "GitHub CI Automation Tool" "Header"
    Write-ColorOutput "USAGE: .\github_automation.ps1 [ACTION]" "Info"
    Write-ColorOutput "ACTIONS:" "Info"
    Write-ColorOutput "  monitor     Show current CI status (default)" "Info"
    Write-ColorOutput "  logs        Interactive log selection and download" "Info"
    Write-ColorOutput "  artifacts   Download CI artifacts" "Info"
    Write-ColorOutput "  help        Show this help message" "Info"
    Write-ColorOutput ""
    Write-ColorOutput "OPTIONS:" "Info"
    Write-ColorOutput "  -Owner      GitHub owner/organization (auto-detected if not provided)" "Info"
    Write-ColorOutput "  -Repo       GitHub repository (auto-detected if not provided)" "Info"
    Write-ColorOutput "  -Continuous Start continuous monitoring" "Info"
    Write-ColorOutput "  -CheckIntervalSeconds Monitoring interval (default: 30)" "Info"
    Write-ColorOutput ""
    Write-ColorOutput "EXAMPLES:" "Info"
    Write-ColorOutput "  .\github_automation.ps1 monitor" "Info"
    Write-ColorOutput "  .\github_automation.ps1 logs -Owner user -Repo project" "Info"
    Write-ColorOutput "  .\github_automation.ps1 monitor -Continuous" "Info"
}

# Main execution
if ($Help) {
    Show-Help
    exit 0
}

if (-not (Test-GitHubCLI)) {
    exit 1
}

# Auto-detect repository if not provided
if (-not $Owner -or -not $Repo) {
    try {
        $remoteUrl = git remote get-url origin
        $githubPattern = "github\.com[:/]([^/]+)/([^/]+?)(?:\.git)?$"
        if ($remoteUrl -match $githubPattern) {
            if (-not $Owner) { $Owner = $matches[1] }
            if (-not $Repo) { $Repo = $matches[2] }
            Write-ColorOutput "[INFO] Auto-detected repository: $Owner/$Repo" "Info"
        } else {
            Write-ColorOutput "[ERROR] Could not detect GitHub repository. Please specify -Owner and -Repo" "Error"
            exit 1
        }
    } catch {
        Write-ColorOutput "[ERROR] Could not detect repository from git remote" "Error"
        exit 1
    }
}

# Execute main action
switch ($Action.ToLower()) {
    "monitor" { 
        Get-CIStatus -Owner $Owner -Repo $Repo
    }
    "logs" {
        Get-FailureLogs -Owner $Owner -Repo $Repo
    }
    "artifacts" {
        Get-Artifacts -Owner $Owner -Repo $Repo  
    }
    "help" {
        Show-Help
    }
    default { 
        Write-ColorOutput "[ERROR] Unknown action: $Action" "Error"
        Show-Help
        exit 1
    }
}

if ($Continuous) {
    Start-ContinuousMonitor -Owner $Owner -Repo $Repo -IntervalSeconds $CheckIntervalSeconds
}
