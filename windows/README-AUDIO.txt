dumphfdl 1.7.0 - Windows conventional receiver / USB audio input
=====================================================================

This Windows build can decode HFDL audio from a conventional HF receiver,
without using an SDR as the input device.

Typical connection
------------------
HF receiver headphone/line output
        |
        v
USB audio adapter / PC recording input
        |
        v
dumphfdl --audio-device ...

The receiver must be in USB mode and tuned to the HFDL assigned frequency.
dumphfdl treats the real mono audio as a USB-demodulated signal and uses the
existing HFDL +1440 Hz carrier offset in its channelizer.

1. List recording devices
-------------------------

PowerShell:

    powershell -ExecutionPolicy Bypass -File .\list-audio-devices.ps1

or directly:

    .\dumphfdl.exe --list-audio-devices

Example output:

    Windows recording devices (2):
      0: Microphone (USB Audio Device)
      1: Microphone (Realtek Audio)

Using the numeric index is recommended because old Windows WinMM device names
may be shortened.

2. Receive one HFDL channel
---------------------------

Tune the receiver to the HFDL channel frequency in USB mode. For example,
for 10081 kHz:

    powershell -ExecutionPolicy Bypass -File .\receive-audio-example.ps1 -Device 0 -Frequency 10081

Direct command:

    .\dumphfdl.exe --utc --system-table .\systable.conf --audio-device 0 --sample-rate 48000 10081

The default audio rate is 48000 Hz. If the USB audio device does not support
48000 Hz, specify a supported rate, for example:

    .\dumphfdl.exe --audio-device 0 --sample-rate 44100 10081

3. Audio level
--------------

Avoid clipping the PC recording input. Start with moderate receiver volume and
watch the Windows input level meter. A clean HFDL signal is more important than
maximum volume.

Important limitations
---------------------
- Windows audio mode currently decodes one HFDL channel at a time.
- Input is mono 16-bit PCM through the native Windows WinMM recording API.
- The receiver must provide demodulated USB audio. LSB is not supported for HFDL.
- This is separate from the existing SoapySDR and raw I/Q input paths; those
  paths are unchanged.
- Live decoding still requires validation with a real receiver/audio adapter.
