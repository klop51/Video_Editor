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
        $authCheck = gh auth status 2>&1
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
        $runs = gh run list --repo "$Owner/$Repo" --limit 10 --json databaseId,status,conclusion,workflowName,createdAt,url | ConvertFrom-Json
        
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
    
    # Show header once - never redraw this
    Clear-Host
    Write-ColorOutput "GitHub CI Log Selection" "Header"
    Write-ColorOutput ("=" * 50) "Header"
    Write-ColorOutput "Controls:" "Info"
    Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
    Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
    Write-ColorOutput ("=" * 50) "Header"
    Write-ColorOutput ""
    
    function Update-SingleLine($index, $run, $isCurrent, $isSelected) {
        try {
            $status = if ($run.status -eq "completed") { $run.conclusion } else { $run.status }
            $created = [DateTime]::Parse($run.createdAt).ToString("MM/dd HH:mm")
            $selMark = if ($isSelected) { "[X]" } else { "[ ]" }
            $curMark = if ($isCurrent) { "> " } else { "  " }
            $line = "$curMark$selMark [$status] $($run.workflowName) ($created)"
            
            # Calculate row position (header=7 lines + filter line + blank line + index)
            $row = 9 + $index
            $width = try { [System.Console]::WindowWidth } catch { 120 }
            
            [System.Console]::SetCursorPosition(0, $row)
            if ($isCurrent) {
                Write-Host ($line.PadRight($width-1)) -ForegroundColor Yellow -NoNewline
            } else {
                Write-Host ($line.PadRight($width-1)) -NoNewline
            }
        } catch {
            # If cursor positioning fails, do nothing (no flashing)
        }
    }
    
    # Apply initial filter
    if ($filterText) {
        $filteredRuns = $Runs | Where-Object { 
            $_.workflowName -like "*$filterText*" -or 
            $_.conclusion -like "*$filterText*" -or 
            $_.status -like "*$filterText*" 
        }
    } else {
        $filteredRuns = $Runs
    }
    
    # Show filter line
    Write-Host ("Filter: {0}" -f $filterText)
    Write-Host ""
    
    # Show initial menu - only once
    for ($i = 0; $i -lt $filteredRuns.Count; $i++) {
        $run = $filteredRuns[$i]
        $originalIndex = [Array]::IndexOf($Runs, $run)
        $isSelected = $selectedIndices -contains $originalIndex
        $isCurrent = ($i -eq $currentIndex)
        
        $status = if ($run.status -eq "completed") { $run.conclusion } else { $run.status }
        $created = [DateTime]::Parse($run.createdAt).ToString("MM/dd HH:mm")
        $selMark = if ($isSelected) { "[X]" } else { "[ ]" }
        $curMark = if ($isCurrent) { "> " } else { "  " }
        $line = "$curMark$selMark [$status] $($run.workflowName) ($created)"
        
        if ($isCurrent) {
            Write-Host $line -ForegroundColor Yellow
        } else {
            Write-Host $line
        }
    }

    # Main input loop - only update specific lines, no redrawing
    while ($true) {
        $key = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

        switch ($key.VirtualKeyCode) {
            38 { # Up arrow - only update the 2 lines that change
                if ($filteredRuns.Count -gt 0) {
                    $prevIndex = $currentIndex
                    $currentIndex = ($currentIndex - 1 + $filteredRuns.Count) % $filteredRuns.Count
                    
                    # Update only the previous line (remove highlight)
                    if ($prevIndex -ne $currentIndex) {
                        $run = $filteredRuns[$prevIndex]
                        $originalIndex = [Array]::IndexOf($Runs, $run)
                        $isSelected = $selectedIndices -contains $originalIndex
                        Update-SingleLine $prevIndex $run $false $isSelected
                        
                        # Update only the new current line (add highlight)
                        $run = $filteredRuns[$currentIndex]
                        $originalIndex = [Array]::IndexOf($Runs, $run)
                        $isSelected = $selectedIndices -contains $originalIndex
                        Update-SingleLine $currentIndex $run $true $isSelected
                    }
                }
            }
            40 { # Down arrow - only update the 2 lines that change
                if ($filteredRuns.Count -gt 0) {
                    $prevIndex = $currentIndex
                    $currentIndex = ($currentIndex + 1) % $filteredRuns.Count
                    
                    # Update only the previous line (remove highlight)
                    if ($prevIndex -ne $currentIndex) {
                        $run = $filteredRuns[$prevIndex]
                        $originalIndex = [Array]::IndexOf($Runs, $run)
                        $isSelected = $selectedIndices -contains $originalIndex
                        Update-SingleLine $prevIndex $run $false $isSelected
                        
                        # Update only the new current line (add highlight)
                        $run = $filteredRuns[$currentIndex]
                        $originalIndex = [Array]::IndexOf($Runs, $run)
                        $isSelected = $selectedIndices -contains $originalIndex
                        Update-SingleLine $currentIndex $run $true $isSelected
                    }
                }
            }
            32 { # Spacebar - only update the current line (toggle selection)
                if ($filteredRuns.Count -gt 0) {
                    $run = $filteredRuns[$currentIndex]
                    $originalIndex = [Array]::IndexOf($Runs, $run)
                    
                    if ($selectedIndices -contains $originalIndex) {
                        $selectedIndices = $selectedIndices | Where-Object { $_ -ne $originalIndex }
                    } else {
                        $selectedIndices += $originalIndex
                    }
                    
                    # Update only this one line
                    $isSelected = $selectedIndices -contains $originalIndex
                    Update-SingleLine $currentIndex $run $true $isSelected
                }
            }
            13 { # Enter - download selected
                if ($selectedIndices.Count -gt 0) {
                    Write-Host ""
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
                    Write-Host ""
                    Write-ColorOutput "Press any key to continue..." "Info"
                    $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown") | Out-Null
                    return
                } else {
                    # Just show message briefly without redrawing
                    $pos = try { [System.Console]::CursorTop } catch { 0 }
                    Write-Host ""
                    Write-ColorOutput "No runs selected" "Warning"
                    Start-Sleep -Milliseconds 1000
                    try { 
                        [System.Console]::SetCursorPosition(0, $pos)
                        Write-Host ("".PadRight([System.Console]::WindowWidth)) -NoNewline
                        [System.Console]::SetCursorPosition(0, $pos)
                    } catch { }
                }
            }
            8 { # Backspace - only for full redraws (filter changes)
                if ($filterText.Length -gt 0) {
                    # Filter changed - need full redraw, but this is rare
                    return Show-InteractiveLogMenu -Runs $Runs -Owner $Owner -Repo $Repo
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
                    # Filter changed - need full redraw, but this is rare
                    return Show-InteractiveLogMenu -Runs $Runs -Owner $Owner -Repo $Repo
                }
            }
        }
    }
}
} else {
            $filteredRuns = $Runs
        }
        
        # Display runs
        if ($filteredRuns.Count -eq 0) {
            Write-ColorOutput "No runs match filter '$filterText'" "Warning"
        } else {
            Write-ColorOutput "Please select runs using Up/Down arrows, Space to select, Enter to download:" "Info"
            Write-ColorOutput ""
            
            for ($i = 0; $i -lt $filteredRuns.Count; $i++) {
                $run = $filteredRuns[$i]
                $originalIndex = [Array]::IndexOf($Runs, $run)
                $isSelected = $selectedIndices -contains $originalIndex
                
                # Selection indicator
                if ($i -eq $currentIndex) {
                    Write-Host "> " -NoNewline -ForegroundColor Yellow
                } else {
                    Write-Host "  " -NoNewline
                }
                
                # Checkbox
                if ($isSelected) {
                    Write-Host "[X] " -NoNewline -ForegroundColor Green
                } else {
                    Write-Host "[ ] " -NoNewline
                }
                
                # Status and workflow info
                $status = if ($run.status -eq "completed") { $run.conclusion } else { $run.status }
                $created = [DateTime]::Parse($run.createdAt).ToString("MM/dd HH:mm")
                
                $statusColor = switch ($run.conclusion) {
                    "success" { "Success" }
                    "failure" { "Error" }
                    "cancelled" { "Warning" }
                    default { "Info" }
                }
                
                Write-ColorOutput "[$status] $($run.workflowName) ($created)" $statusColor
            }
        }
        
        Write-ColorOutput ""
        if ($selectedIndices.Count -gt 0) {
            Write-ColorOutput "Selected: $($selectedIndices.Count) runs" "Highlight"
        }
        
        # Get user input
        $key = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        
        # Handle navigation and selection
        switch ($key.VirtualKeyCode) {
            38 { # Up arrow
                if ($filteredRuns.Count -gt 0) {
                    $currentIndex = ($currentIndex - 1 + $filteredRuns.Count) % $filteredRuns.Count
                }
                # Clear screen and redraw
                Clear-Host
                Write-ColorOutput "GitHub CI Log Selection" "Header"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput "Controls:" "Info"
                Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
                Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput ""
            }
            40 { # Down arrow
                if ($filteredRuns.Count -gt 0) {
                    $currentIndex = ($currentIndex + 1) % $filteredRuns.Count
                }
                # Clear screen and redraw
                Clear-Host
                Write-ColorOutput "GitHub CI Log Selection" "Header"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput "Controls:" "Info"
                Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
                Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput ""
            }
            32 { # Spacebar
                if ($filteredRuns.Count -gt 0) {
                    $originalIndex = [Array]::IndexOf($Runs, $filteredRuns[$currentIndex])
                    if ($selectedIndices -contains $originalIndex) {
                        $selectedIndices = $selectedIndices | Where-Object { $_ -ne $originalIndex }
                    } else {
                        $selectedIndices += $originalIndex
                    }
                }
                # Clear screen and redraw
                Clear-Host
                Write-ColorOutput "GitHub CI Log Selection" "Header"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput "Controls:" "Info"
                Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
                Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput ""
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
                    Start-Sleep -Milliseconds 1000
                    # Clear screen and redraw
                    Clear-Host
                    Write-ColorOutput "GitHub CI Log Selection" "Header"
                    Write-ColorOutput ("=" * 50) "Header"
                    Write-ColorOutput "Controls:" "Info"
                    Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
                    Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
                    Write-ColorOutput ("=" * 50) "Header"
                    Write-ColorOutput ""
                }
            }
            8 { # Backspace
                if ($filterText.Length -gt 0) {
                    $filterText = $filterText.Substring(0, $filterText.Length - 1)
                    $currentIndex = 0
                }
                # Clear screen and redraw
                Clear-Host
                Write-ColorOutput "GitHub CI Log Selection" "Header"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput "Controls:" "Info"
                Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
                Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
                Write-ColorOutput ("=" * 50) "Header"
                Write-ColorOutput ""
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
                    # Clear screen and redraw
                    Clear-Host
                    Write-ColorOutput "GitHub CI Log Selection" "Header"
                    Write-ColorOutput ("=" * 50) "Header"
                    Write-ColorOutput "Controls:" "Info"
                    Write-ColorOutput "  Up/Down: Navigate | SPACE: Select/Deselect | ENTER: Download selected" "Info"
                    Write-ColorOutput "  Type: Filter | BACKSPACE: Clear filter | ESC/Q: Exit" "Info"
                    Write-ColorOutput ("=" * 50) "Header"
                    Write-ColorOutput ""
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
