dumphfdl -> PC-HFDL / Contrail compatibility bridge (Windows)
================================================================

Purpose
-------
These scripts create a PC-HFDL-style text stream from dumphfdl's decoded JSON
output so software which already understands the traditional PC-HFDL position
records can be tested without changing the HFDL decoder or SDR support.

The bridge currently emits the two native HFDL position record types seen in
PC-HFDL logs:

    [HFNPDU PERFORMANCE]
    HH:MM:SS  UTC  Flight ID = ......  LAT D M S  N/S  LON D M S  E/W

and:

    [HFNPDU FREQUENCY DATA]
    HH:MM:SS  UTC  Flight ID = ......  LAT D M S  N/S  LON D M S  E/W

This is intentionally a compatibility subset, not a byte-for-byte reproduction
of every PC-HFDL screen/log line. It targets the position records that plotting
software normally consumes. ACARS text, preamble diagnostics, ground-station
status lines and CRC-failure lines are not synthesized by this bridge.

Live use
--------
Run PowerShell in the extracted release directory.

Airspy HF+ / Discovery example on 8942 kHz:

    powershell -ExecutionPolicy Bypass -File .\receive-contrail.ps1 `
      -OutputPath .\pchfdl-contrail.txt `
      -DumphfdlArgs '--soapysdr','driver=airspyhf','--sample-rate','768000','8942'

The output file is appended continuously. Point Contrail (or another PC-HFDL
log consumer) at pchfdl-contrail.txt if it allows selecting a PC-HFDL log file.

Multiple HFDL frequencies may be added at the end of -DumphfdlArgs as long as
they fit inside the configured SDR bandwidth.

Offline conversion
------------------
If you already have one-JSON-object-per-line dumphfdl output:

    Get-Content .\dumphfdl.json |
      powershell -ExecutionPolicy Bypass -File .\Convert-DumphfdlJsonToPchfdl.ps1 `
      -OutputPath .\pchfdl-contrail.txt

Notes
-----
- The original dumphfdl executable and AirspyHF / RTL-SDR support are unchanged.
- The converter ignores non-position HFDL messages.
- Latitude/longitude are converted from dumphfdl decimal degrees to the integer
  degree/minute/second layout used by PC-HFDL.
- The file is ASCII and uses CRLF line endings.
- Real Contrail compatibility still needs confirmation with the actual Contrail
  parser. The generated records match the PC-HFDL position-line syntax supplied
  for testing, but no claim is made yet about unsupported PC-HFDL log sections.
