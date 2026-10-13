[CmdletBinding()]
param(
    [Parameter(ValueFromPipeline = $true)]
    [AllowEmptyString()]
    [string]$InputObject,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

begin {
    function Find-NamedPropertyValue {
        param(
            [Parameter(Mandatory = $true)] $Object,
            [Parameter(Mandatory = $true)] [string]$Name
        )

        if ($null -eq $Object) {
            return $null
        }

        if ($Object -is [System.Management.Automation.PSCustomObject]) {
            $direct = $Object.PSObject.Properties[$Name]
            if ($null -ne $direct) {
                return $direct.Value
            }

            foreach ($property in $Object.PSObject.Properties) {
                $value = $property.Value
                if ($null -eq $value -or $value -is [string] -or $value.GetType().IsPrimitive) {
                    continue
                }
                $found = Find-NamedPropertyValue -Object $value -Name $Name
                if ($null -ne $found) {
                    return $found
                }
            }
        }
        elseif ($Object -is [System.Collections.IDictionary]) {
            if ($Object.Contains($Name)) {
                return $Object[$Name]
            }
            foreach ($value in $Object.Values) {
                $found = Find-NamedPropertyValue -Object $value -Name $Name
                if ($null -ne $found) {
                    return $found
                }
            }
        }
        elseif ($Object -is [System.Collections.IEnumerable] -and -not ($Object -is [string])) {
            foreach ($value in $Object) {
                $found = Find-NamedPropertyValue -Object $value -Name $Name
                if ($null -ne $found) {
                    return $found
                }
            }
        }

        return $null
    }

    function Convert-ToPchfdlDms {
        param(
            [Parameter(Mandatory = $true)] [double]$Value,
            [Parameter(Mandatory = $true)] [double]$Maximum,
            [Parameter(Mandatory = $true)] [string]$PositiveHemisphere,
            [Parameter(Mandatory = $true)] [string]$NegativeHemisphere
        )

        if ([double]::IsNaN($Value) -or [double]::IsInfinity($Value)) {
            return $null
        }

        $absolute = [math]::Abs($Value)
        if ($absolute -gt $Maximum) {
            return $null
        }

        $degrees = [int][math]::Floor($absolute)
        $minutesFloat = ($absolute - $degrees) * 60.0
        $minutes = [int][math]::Floor($minutesFloat)
        $seconds = [int][math]::Round(
            ($minutesFloat - $minutes) * 60.0,
            0,
            [System.MidpointRounding]::AwayFromZero
        )

        if ($seconds -ge 60) {
            $seconds = 0
            $minutes++
        }
        if ($minutes -ge 60) {
            $minutes = 0
            $degrees++
        }

        if ($degrees -gt $Maximum) {
            $degrees = [int]$Maximum
            $minutes = 0
            $seconds = 0
        }

        [pscustomobject]@{
            Degrees = $degrees
            Minutes = $minutes
            Seconds = $seconds
            Hemisphere = if ($Value -lt 0) { $NegativeHemisphere } else { $PositiveHemisphere }
        }
    }

    function Convert-OneDumphfdlJsonLine {
        param([string]$Line)

        if ([string]::IsNullOrWhiteSpace($Line)) {
            return $null
        }

        try {
            $record = $Line | ConvertFrom-Json -ErrorAction Stop
        }
        catch {
            return $null
        }

        $hfnpdu = Find-NamedPropertyValue -Object $record -Name "hfnpdu"
        if ($null -eq $hfnpdu -or $null -eq $hfnpdu.type) {
            return $null
        }

        $typeId = $null
        if ($null -ne $hfnpdu.type.id) {
            try { $typeId = [int]$hfnpdu.type.id } catch { $typeId = $null }
        }

        $typeName = [string]$hfnpdu.type.name
        $tag = $null
        $time = $null

        if ($typeId -eq 0xD1 -or $typeName -match "(?i)^performance data$") {
            $tag = "[HFNPDU PERFORMANCE]"
            $time = $hfnpdu.time
        }
        elseif ($typeId -eq 0xD5 -or $typeName -match "(?i)^frequency data$") {
            $tag = "[HFNPDU FREQUENCY DATA]"
            $time = $hfnpdu.utc_time
        }
        else {
            return $null
        }

        if ($null -eq $time -or $null -eq $hfnpdu.pos) {
            return $null
        }

        $flightId = ([string]$hfnpdu.flight_id).Trim()
        if ([string]::IsNullOrWhiteSpace($flightId)) {
            return $null
        }

        try {
            $lat = Convert-ToPchfdlDms -Value ([double]$hfnpdu.pos.lat) -Maximum 90 -PositiveHemisphere "N" -NegativeHemisphere "S"
            $lon = Convert-ToPchfdlDms -Value ([double]$hfnpdu.pos.lon) -Maximum 180 -PositiveHemisphere "E" -NegativeHemisphere "W"
            $hour = [int]$time.hour
            $minute = [int]$time.min
            $second = [int]$time.sec
        }
        catch {
            return $null
        }

        if ($null -eq $lat -or $null -eq $lon -or
            $hour -lt 0 -or $hour -gt 23 -or
            $minute -lt 0 -or $minute -gt 59 -or
            $second -lt 0 -or $second -gt 59) {
            return $null
        }

        $positionLine = "{0:00}:{1:00}:{2:00}  UTC  Flight ID = {3}  LAT {4} {5} {6}  {7}  LON {8} {9} {10}  {11}  " -f
            $hour, $minute, $second, $flightId,
            $lat.Degrees, $lat.Minutes, $lat.Seconds, $lat.Hemisphere,
            $lon.Degrees, $lon.Minutes, $lon.Seconds, $lon.Hemisphere

        return "$tag`r`n$positionLine`r`n`r`n"
    }

    $fullOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
    $outputDirectory = Split-Path -Parent $fullOutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory) -and -not (Test-Path $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
}

process {
    $block = Convert-OneDumphfdlJsonLine -Line $InputObject
    if ($null -ne $block) {
        [System.IO.File]::AppendAllText(
            $fullOutputPath,
            $block,
            [System.Text.Encoding]::ASCII
        )
        Write-Output ($block.TrimEnd())
    }
}
