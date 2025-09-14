# Find the exact line where brace imbalance occurs
$content = Get-Content 'c:\Users\braul\Desktop\Video_Editor\scripts\deploy-dlls.ps1'
$openBraces = 0
$lineNumber = 0

foreach ($line in $content) {
    $lineNumber++
    $openCount = ($line -split '\{').Count - 1
    $closeCount = ($line -split '\}').Count - 1
    $openBraces += $openCount - $closeCount
    
    # Show lines between 260 and 275 with their brace changes
    if ($lineNumber -ge 260 -and $lineNumber -le 275) {
        $change = $openCount - $closeCount
        Write-Host "Line $lineNumber (Change: $change, Balance: $openBraces): $line"
    }
    
    if ($lineNumber -eq 272) {
        break
    }
}