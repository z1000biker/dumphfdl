[CmdletBinding()]
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot "pchfdl-contrail.txt"),

    [Parameter(Mandatory = $true)]
    [string[]]$DumphfdlArgs
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $PSScriptRoot "dumphfdl.exe"
$converter = Join-Path $PSScriptRoot "Convert-DumphfdlJsonToPchfdl.ps1"

if (-not (Test-Path $exe)) {
    throw "dumphfdl.exe was not found next to this script."
}
if (-not (Test-Path $converter)) {
    throw "Convert-DumphfdlJsonToPchfdl.ps1 was not found next to this script."
}

if ($DumphfdlArgs -contains "--output") {
    throw "Do not include --output in -DumphfdlArgs. This launcher adds the JSON output needed by the PC-HFDL compatibility converter."
}

$env:PATH = "$PSScriptRoot;$env:PATH"
$pluginPath = Join-Path $PSScriptRoot "SoapySDR\modules0.8"
if (Test-Path $pluginPath) {
    $env:SOAPY_SDR_PLUGIN_PATH = $pluginPath
}

$fullOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
Write-Host "Writing PC-HFDL-compatible position records to: $fullOutputPath"
Write-Host "Press Ctrl+C to stop."
Write-Host ""

$oldPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    & $exe --output "decoded:json:file:path=-" @DumphfdlArgs |
        & $converter -OutputPath $fullOutputPath
}
finally {
    $ErrorActionPreference = $oldPreference
}
