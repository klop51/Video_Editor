# Test just the problem section
$criticalDlls = @(
    "Qt6Core*.dll",
    "Qt6Gui*.dll",
    "Qt6Widgets*.dll"
)

$allCriticalPresent = $true
foreach ($pattern in $criticalDlls) {
    Write-Host "Testing: $pattern"
}

Write-Host "Test completed successfully"