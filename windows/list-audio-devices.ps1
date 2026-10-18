$ErrorActionPreference = "Stop"
$exe = Join-Path $PSScriptRoot "dumphfdl.exe"
& $exe --list-audio-devices
exit $LASTEXITCODE
