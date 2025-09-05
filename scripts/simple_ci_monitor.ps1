# Simple CI Monitor - No Jan API dependency
# Just detects failures and creates alerts

param(
    [int]$PollMinutes = 2,
    [string]$Repository = "klop51/Video_Editor"
)

Write-Host "Simple CI Monitor - Linux Focus" -ForegroundColor Green
Write-Host "Repository: $Repository" -ForegroundColor Cyan
Write-Host "Poll Interval: $PollMinutes minutes" -ForegroundColor Cyan
Write-Host ""

$lastRunNumber = $null
$startTime = Get-Date

while ($true) {
    try {
        # Get latest run
        $runJson = gh run list --repo $Repository --limit=1 --json number,status,conclusion,displayTitle,createdAt,updatedAt
        if (-not $runJson) {
            Write-Host "$(Get-Date -Format 'HH:mm:ss') No runs found" -ForegroundColor Yellow
            Start-Sleep (60 * $PollMinutes)
            continue
        }

        $runs = $runJson | ConvertFrom-Json
        $latestRun = $runs[0]
        $runNumber = $latestRun.number
        $status = $latestRun.status
        $conclusion = $latestRun.conclusion

        # Check if this is a new run
        if ($runNumber -ne $lastRunNumber) {
            Write-Host "$(Get-Date -Format 'HH:mm:ss') NEW RUN #$runNumber - $status" -ForegroundColor Yellow
            $lastRunNumber = $runNumber
        }

        # Check for completion
        if ($status -eq "completed") {
            $timestamp = Get-Date -Format 'HH:mm:ss'
            
            if ($conclusion -eq "success") {
                Write-Host "[$timestamp] RUN #$runNumber - SUCCESS ✅" -ForegroundColor Green
            } 
            elseif ($conclusion -eq "failure") {
                Write-Host "[$timestamp] RUN #$runNumber - FAILED ❌" -ForegroundColor Red
                
                # Create failure alert
                $alertFile = "CI_FAILURE_#$runNumber.txt"
                $runUrl = "https://github.com/$Repository/actions/runs/$runNumber"
                
                $alertContent = @"
CI FAILURE DETECTED
==================
Run: #$runNumber  
Status: $conclusion
URL: $runUrl
Time: $(Get-Date)

LINUX BUILD FAILURE - Check logs for:
- Ubuntu package installation issues  
- vcpkg build timeouts (30min mark)
- Qt6/XCB configuration problems
- Missing dependencies

Action Required: Review logs and apply fixes
"@
                
                Set-Content -Path $alertFile -Value $alertContent
                Write-Host "Created alert file: $alertFile" -ForegroundColor Magenta
                
                # Try to open in VS Code
                try {
                    code $alertFile
                    Write-Host "Opened alert in VS Code" -ForegroundColor Green
                } catch {
                    Write-Host "Could not open in VS Code: $_" -ForegroundColor Yellow
                }
                
                # Reset for next run
                $lastRunNumber = $null
            }
        }
        elseif ($status -eq "in_progress") {
            # Check for timeouts (runs longer than 35 minutes are likely stuck)
            $runAge = (Get-Date) - [DateTime]$latestRun.createdAt
            if ($runAge.TotalMinutes -gt 35) {
                Write-Host "$(Get-Date -Format 'HH:mm:ss') WARNING: Run #$runNumber running for $([int]$runAge.TotalMinutes) minutes (likely timeout)" -ForegroundColor Red
            } else {
                Write-Host "$(Get-Date -Format 'HH:mm:ss') Run #$runNumber in progress ($([int]$runAge.TotalMinutes) min)" -ForegroundColor Cyan
            }
        }

    } catch {
        Write-Host "$(Get-Date -Format 'HH:mm:ss') Error: $_" -ForegroundColor Red
    }

    # Wait before next check
    Start-Sleep (60 * $PollMinutes)
}
