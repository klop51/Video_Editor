# Test if the issue is line-number specific
param(
    [string]$TargetBinary = "test",
    [string]$VcpkgInstalledDir = "test"
)

Write-Host "Testing array definition and foreach..."

# Test the exact same structure but earlier in the file
$criticalDlls = @(
    "Qt6Core*.dll",
    "Qt6Gui*.dll", 
    "Qt6Widgets*.dll"
)

$allCriticalPresent = $true
foreach ($pattern in $criticalDlls) {
    Write-Host "Pattern: $pattern"
}

Write-Host "Test completed successfully"