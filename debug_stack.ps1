# Debug script to capture stack information for video_editor.exe
# Run this when the application becomes unresponsive

Write-Host "=== Video Editor Debug Information ===" -ForegroundColor Green

# Find the video editor process
$processes = Get-Process -Name "video_editor" -ErrorAction SilentlyContinue

if ($processes) {
    foreach ($proc in $processes) {
        Write-Host "Process ID: $($proc.Id)" -ForegroundColor Yellow
        Write-Host "CPU Usage: $($proc.CPU)" -ForegroundColor Yellow
        Write-Host "Memory Usage: $([math]::Round($proc.WorkingSet64/1MB, 2)) MB" -ForegroundColor Yellow
        Write-Host "Thread Count: $($proc.Threads.Count)" -ForegroundColor Yellow
        $state = if ($proc.Responding) { 'Responding' } else { 'NOT RESPONDING' }
        $color = if ($proc.Responding) { 'Green' } else { 'Red' }
        Write-Host "Process State: $state" -ForegroundColor $color
        
        Write-Host "`nThread Information:" -ForegroundColor Cyan
        $threadInfo = $proc.Threads | Sort-Object TotalProcessorTime -Descending | Select-Object -First 5
        foreach ($thread in $threadInfo) {
            Write-Host "  Thread ID: $($thread.Id), CPU Time: $($thread.TotalProcessorTime), State: $($thread.ThreadState)" -ForegroundColor Gray
        }
        
        # Try to get more detailed info using WMI
        try {
            $wmiProc = Get-WmiObject -Class Win32_Process -Filter "ProcessId = $($proc.Id)"
            Write-Host "`nCommand Line: $($wmiProc.CommandLine)" -ForegroundColor Gray
            Write-Host "Start Time: $($wmiProc.CreationDate)" -ForegroundColor Gray
        } catch {
            Write-Host "Could not get WMI process info: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
} else {
    Write-Host "No video_editor.exe processes found" -ForegroundColor Red
}

Write-Host "`n=== System Resource Information ===" -ForegroundColor Green
Write-Host "Available Memory: $([math]::Round((Get-WmiObject -Class Win32_OperatingSystem).FreePhysicalMemory/1KB/1KB, 2)) MB" -ForegroundColor Gray
Write-Host "CPU Load: $(Get-WmiObject -Class Win32_Processor | Measure-Object LoadPercentage -Average | ForEach-Object {$_.Average})%" -ForegroundColor Gray

Write-Host "`nPress any key to continue..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
