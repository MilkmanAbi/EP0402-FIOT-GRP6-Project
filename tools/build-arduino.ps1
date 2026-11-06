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
$secretsPath=Join-Path $repoRoot 'arduino\ParcelGuardian\Secrets.h'
if(-not (Test-Path -LiteralPath $secretsPath)){
    Copy-Item -LiteralPath (Join-Path $repoRoot 'arduino\ParcelGuardian\Secrets.example.h') -Destination $secretsPath
    Write-Host 'Created placeholder Secrets.h. Set pairing values before upload.'
}
Run-Checked $arduinoExe @('compile','--fqbn','arduino:avr:uno','--warnings','all','--output-dir',(Join-Path $repoRoot 'build\arduino'),(Join-Path $repoRoot 'arduino\ParcelGuardian'))
foreach($name in @('AdxlDropTest','EspAtTest','GpsRawTest','SdCardTest')){
    Run-Checked $arduinoExe @('compile','--fqbn','arduino:avr:uno','--warnings','all',(Join-Path $repoRoot "arduino\tools\$name"))
}
if($NativeTests){Run-Checked (Join-Path $repoRoot 'tests\arduino\run.cmd') @()}
