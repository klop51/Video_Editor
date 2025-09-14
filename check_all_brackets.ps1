# Check for bracket and parenthesis imbalance too
$content = Get-Content 'c:\Users\braul\Desktop\Video_Editor\scripts\deploy-dlls.ps1'
$openBraces = 0
$openParens = 0
$openBrackets = 0
$lineNumber = 0

foreach ($line in $content) {
    $lineNumber++
    $openBraces += ($line -split '\{').Count - 1
    $openBraces -= ($line -split '\}').Count - 1
    $openParens += ($line -split '\(').Count - 1
    $openParens -= ($line -split '\)').Count - 1
    $openBrackets += ($line -split '\[').Count - 1
    $openBrackets -= ($line -split '\]').Count - 1
    
    if ($lineNumber -eq 272) {
        Write-Host "Line 272 Balance:"
        Write-Host "  Braces: $openBraces"
        Write-Host "  Parentheses: $openParens"  
        Write-Host "  Brackets: $openBrackets"
        break
    }
}

if ($openBraces -ne 0 -or $openParens -ne 0 -or $openBrackets -ne 0) {
    Write-Host "IMBALANCE DETECTED!" -ForegroundColor Red
}