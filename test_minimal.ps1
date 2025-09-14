# Minimal test for line 272 issue
param(
    [string]$TargetBinary = "test",
    [string]$VcpkgInstalledDir = "test"
)

Write-Host "Testing..."

# This is around line 10 - should work fine
$testArray = @(
    "item1",
    "item2"
)

$testVar = $true
foreach ($item in $testArray) {
    Write-Host "Item: $item"
}

Write-Host "Test completed - no errors should occur"