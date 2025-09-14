# Find where the missing brace occurs
$content = Get-Content 'c:\Users\braul\Desktop\Video_Editor\scripts\deploy-dlls.ps1'
$openBraces = 0
$lineNumber = 0
$lastBalanceChange = 0

foreach ($line in $content) {
    $lineNumber++
    $prevBraces = $openBraces
    $openBraces += ($line -split '\{').Count - 1
    $openBraces -= ($line -split '\}').Count - 1
    
    # Track when balance changes
    if ($openBraces -ne $prevBraces) {
        $lastBalanceChange = $lineNumber
        Write-Host "Line $lineNumber (Balance: $openBraces): $line"
    }
    
    if ($lineNumber -eq 272) {
        Write-Host "`nAt line 272, brace balance is: $openBraces"
        Write-Host "Last balance change was at line: $lastBalanceChange"
        break
    }
}

Write-Host "`nSearching backwards from line 272 for the unclosed brace..."