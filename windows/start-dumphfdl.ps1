$ErrorActionPreference = "Stop"
$env:SOAPY_SDR_PLUGIN_PATH = Join-Path $PSScriptRoot "SoapySDR\modules0.8"

$exe = Join-Path $PSScriptRoot "dumphfdl.exe"
if ($args.Count -eq 0) {
    & $exe --help
} else {
    & $exe @args
}
exit $LASTEXITCODE
