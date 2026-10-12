$ErrorActionPreference = "Stop"
$env:PATH = "$PSScriptRoot;$env:PATH"
$env:SOAPY_SDR_PLUGIN_PATH = Join-Path $PSScriptRoot "SoapySDR\modules0.8"
& (Join-Path $PSScriptRoot "SoapySDRUtil.exe") --find="driver=airspyhf"
exit $LASTEXITCODE
