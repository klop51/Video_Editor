Param(
    [string]$Root = (Resolve-Path "$PSScriptRoot/.."),
    [string[]]$Include = @('src/core','src/timeline','src/commands','src/decode'),
    [switch]$FailOnViolation
)

$violations = @()
foreach($dir in $Include){
    $path = Join-Path $Root $dir
    if(Test-Path $path){
        Get-ChildItem -Path $path -Recurse -Include *.hpp,*.h,*.cpp | ForEach-Object {
            $content = Get-Content $_.FullName -Raw
            if($content -match "\bthrow\b"){
                # allow comments containing 'throw' only if explicitly annotated
                $lines = $content -split "`n"
                for($i=0;$i -lt $lines.Count;$i++){
                    if($lines[$i] -match "\bthrow\b" -and $lines[$i] -notmatch "VE_ALLOW_THROW"){
                        $violations += @{ File=$_.FullName; Line=$i+1; Text=$lines[$i] }
                    }
                }
            }
        }
    }
}

if($violations.Count -gt 0){
    Write-Host "No-throw enforcement violations:" -ForegroundColor Red
    foreach($v in $violations){
        Write-Host "${($v.File)}:${($v.Line)}: $($v.Text.Trim())"
    }
    if($FailOnViolation){ exit 1 }
}else{
    Write-Host "No-throw enforcement passed." -ForegroundColor Green
}
