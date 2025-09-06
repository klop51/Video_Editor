# GitHub CI Notification System
# Monitors CI and sends Windows notifications when status changes

param(
    [string]$Owner = "klop51",
    [string]$Repo = "Video_Editor", 
    [int]$CheckInterval = 30,
    [switch]$OnlyFailures
)

# Import required assembly for Windows notifications
Add-Type -AssemblyName System.Windows.Forms

function Send-Notification {
    param(
        [string]$Title,
        [string]$Message,
        [string]$Icon = "Info"
    )
    
    # Try Windows 10/11 toast notification first
    try {
        $ToastXml = [Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime]::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType, Windows.UI.Notifications, ContentType = WindowsRuntime]::ToastText02)
        
        $ToastXml.SelectSingleNode("//text[@id='1']").AppendChild($ToastXml.CreateTextNode($Title)) | Out-Null
        $ToastXml.SelectSingleNode("//text[@id='2']").AppendChild($ToastXml.CreateTextNode($Message)) | Out-Null
        
        $AppId = "GitHub CI Monitor"
        [Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime]::CreateToastNotifier($AppId).Show($ToastXml)
    }
    catch {
        # Fallback to balloon tip
        try {
            $balloon = New-Object System.Windows.Forms.NotifyIcon
            $balloon.Icon = [System.Drawing.SystemIcons]::Information
            $balloon.BalloonTipIcon = $Icon
            $balloon.BalloonTipText = $Message
            $balloon.BalloonTipTitle = $Title
            $balloon.Visible = $true
            $balloon.ShowBalloonTip(5000)
            Start-Sleep -Seconds 1
            $balloon.Dispose()
        }
        catch {
            Write-Host "NOTIFICATION: $Title - $Message" -ForegroundColor Yellow
        }
    }
}

function Get-LatestRunStatus {
    param([string]$Owner, [string]$Repo)
    
    try {
        $jsonOutput = gh run list --repo "$Owner/$Repo" --limit 1 --json conclusion,status,workflowName,headBranch
        $run = ($jsonOutput | ConvertFrom-Json)[0]
        
        $status = if ($run.conclusion) { $run.conclusion } else { $run.status }
        
        return @{
            Status = $status
            Workflow = $run.workflowName
            Branch = $run.headBranch
            IsCompleted = ($null -ne $run.conclusion)
        }
    }
    catch {
        Write-Host "[ERROR] Failed to get CI status: $_" -ForegroundColor Red
        return $null
    }
}

Write-Host "[MONITOR] Starting GitHub CI notification monitor..." -ForegroundColor Cyan
Write-Host "Repository: $Owner/$Repo" -ForegroundColor White
Write-Host "Check interval: $CheckInterval seconds" -ForegroundColor White
Write-Host "Monitoring for changes... (Ctrl+C to stop)" -ForegroundColor Yellow

$lastStatus = $null
$notifiedRuns = @{}

while ($true) {
    $currentRun = Get-LatestRunStatus -Owner $Owner -Repo $Repo
    
    if ($currentRun) {
        $runKey = "$($currentRun.Workflow)-$($currentRun.Branch)"
        $status = $currentRun.Status
        
        # Check if this is a new status we haven't notified about
        if ($currentRun.IsCompleted -and $notifiedRuns[$runKey] -ne $status) {
            $shouldNotify = $true
            
            # If OnlyFailures is set, only notify on failures
            if ($OnlyFailures -and $status -ne "failure") {
                $shouldNotify = $false
            }
            
            if ($shouldNotify) {
                $icon = switch ($status) {
                    "success" { "Info" }
                    "failure" { "Error" }
                    "cancelled" { "Warning" }
                    default { "Info" }
                }
                
                $emoji = switch ($status) {
                    "success" { "✅" }
                    "failure" { "❌" }
                    "cancelled" { "🚫" }
                    default { "ℹ️" }
                }
                
                $title = "GitHub CI: $($currentRun.Workflow)"
                $message = "$emoji Build $status on $($currentRun.Branch)"
                
                Send-Notification -Title $title -Message $message -Icon $icon
                Write-Host "[$(Get-Date -Format 'HH:mm:ss')] NOTIFICATION: $title - $message" -ForegroundColor Green
                
                $notifiedRuns[$runKey] = $status
            }
        }
        
        # Show current status (but don't spam)
        if ($lastStatus -ne $status) {
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Current: $($currentRun.Workflow) [$status] on $($currentRun.Branch)" -ForegroundColor Cyan
            $lastStatus = $status
        }
    }
    
    Start-Sleep -Seconds $CheckInterval
}
