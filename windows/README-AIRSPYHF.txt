dumphfdl 1.7.0 - Windows x64 - Airspy HF+ / SoapySDR

This portable build adds Airspy HF+ and Airspy HF+ Discovery support through SoapySDR / SoapyAirspyHF while retaining RTL-SDR support.

Keep the complete extracted directory together.

Test detection:
powershell -ExecutionPolicy Bypass -File .\find-airspyhf.ps1

First HFDL test:
powershell -ExecutionPolicy Bypass -File .\receive-airspyhf-example.ps1

Another frequency:
powershell -ExecutionPolicy Bypass -File .\receive-airspyhf-example.ps1 -Frequency 8834

Direct:
powershell -ExecutionPolicy Bypass -File .\start-dumphfdl.ps1 --soapysdr driver=airspyhf --sample-rate 768000 10081

If multiple Airspy HF devices are connected, use:
.\SoapySDRUtil.exe --find="driver=airspyhf"

and pass:
--soapysdr "driver=airspyhf,serial=YOUR_SERIAL"

Automated CI verifies compilation, portable DLL packaging, dumphfdl startup and Soapy module loading. Live RF reception still requires testing on real Airspy HF+ hardware.
