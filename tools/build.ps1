param([switch]$NativeTests)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'build-arduino.ps1') -NativeTests:$NativeTests
function Run-Checked([string[]]$Arguments){
    & flutter.bat @Arguments
    if($LASTEXITCODE -ne 0){throw "Flutter failed with exit code $LASTEXITCODE"}
}
Push-Location (Join-Path $repoRoot 'app')
try{
    Run-Checked @('pub','get')
    Run-Checked @('analyze')
    Run-Checked @('test')
    Run-Checked @('build','apk','--release')
}finally{Pop-Location}
