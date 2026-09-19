param(
    [int]$SampleRate = 250000,
    [string[]]$Frequency = @("10081")
)

$ErrorActionPreference = "Stop"
$env:SOAPY_SDR_PLUGIN_PATH = Join-Path $PSScriptRoot "SoapySDR\modules0.8"

$exe = Join-Path $PSScriptRoot "dumphfdl.exe"
$systemTable = Join-Path $PSScriptRoot "systable.conf"
& $exe --system-table $systemTable --soapysdr "driver=rtlsdr" --sample-rate $SampleRate @Frequency
exit $LASTEXITCODE
