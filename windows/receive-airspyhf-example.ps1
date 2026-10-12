param(
    [int]$SampleRate = 768000,
    [string[]]$Frequency = @("10081"),
    [string]$Device = "driver=airspyhf"
)
$ErrorActionPreference = "Stop"
$env:PATH = "$PSScriptRoot;$env:PATH"
$env:SOAPY_SDR_PLUGIN_PATH = Join-Path $PSScriptRoot "SoapySDR\modules0.8"
$exe = Join-Path $PSScriptRoot "dumphfdl.exe"
$systemTable = Join-Path $PSScriptRoot "systable.conf"
& $exe --system-table $systemTable --soapysdr $Device --sample-rate $SampleRate @Frequency
exit $LASTEXITCODE
