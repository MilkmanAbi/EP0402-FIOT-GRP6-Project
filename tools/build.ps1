param([switch]$NativeTests)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
$arduinoCommand=Get-Command arduino-cli -ErrorAction SilentlyContinue
$arduinoExe=if($arduinoCommand){$arduinoCommand.Source}else{'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'}
if(-not (Test-Path -LiteralPath $arduinoExe)){throw 'Install Arduino CLI and put it on PATH.'}
function Run-Checked([string]$Executable,[string[]]$Arguments){
    & $Executable @Arguments
    if($LASTEXITCODE -ne 0){throw "$Executable failed with exit code $LASTEXITCODE"}
}
$secretsPath=Join-Path $repoRoot 'firmware\ParcelGuardian\Secrets.h'
if(-not (Test-Path -LiteralPath $secretsPath)){
    Copy-Item -LiteralPath (Join-Path $repoRoot 'firmware\ParcelGuardian\Secrets.example.h') -Destination $secretsPath
    Write-Host 'Created placeholder Secrets.h. Set real pairing values before upload.'
}
Run-Checked $arduinoExe @('compile','--fqbn','arduino:avr:uno','--warnings','all','--output-dir',(Join-Path $repoRoot 'build\firmware'),(Join-Path $repoRoot 'firmware\ParcelGuardian'))
foreach($name in @('AdxlDropTest','EspAtTest','GpsRawTest','SdCardTest')){
    Run-Checked $arduinoExe @('compile','--fqbn','arduino:avr:uno','--warnings','all',(Join-Path $repoRoot "firmware\tools\$name"))
}
if($NativeTests){Run-Checked (Join-Path $repoRoot 'tests\firmware\run.cmd') @()}
Push-Location (Join-Path $repoRoot 'app')
try{
    Run-Checked 'flutter.bat' @('pub','get')
    Run-Checked 'flutter.bat' @('analyze')
    Run-Checked 'flutter.bat' @('test')
    Run-Checked 'flutter.bat' @('build','apk','--release')
}finally{Pop-Location}
