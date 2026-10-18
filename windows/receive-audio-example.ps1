param(
    [string]$Device = "default",
    [int]$SampleRate = 48000,
    [string]$Frequency = "10081"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $PSScriptRoot "dumphfdl.exe"
$systemTable = Join-Path $PSScriptRoot "systable.conf"

& $exe --utc --system-table $systemTable --audio-device $Device --sample-rate $SampleRate $Frequency
exit $LASTEXITCODE
