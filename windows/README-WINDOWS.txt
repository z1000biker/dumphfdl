dumphfdl 1.7.0 για Windows x64
================================

Το πακέτο είναι φορητό: δεν χρειάζεται MSYS2, WSL ή εγκατάσταση βιβλιοθηκών.
Περιλαμβάνει SoapySDR και SoapyRTLSDR για RTL-SDR Blog V4.

1. Αποσυμπίεσε ολόκληρο το ZIP σε φάκελο της επιλογής σου.
2. Άνοιξε PowerShell μέσα στον φάκελο.
3. Έλεγξε ότι αναγνωρίζεται το SDR:

   powershell -ExecutionPolicy Bypass -File .\find-rtlsdr.ps1

4. Ξεκίνα δοκιμαστική λήψη στα 10081 kHz:

   powershell -ExecutionPolicy Bypass -File .\receive-example.ps1

   Η συχνότητα είναι σε kHz. Για άλλη συχνότητα:

   powershell -ExecutionPolicy Bypass -File .\receive-example.ps1 -Frequency 8834

   Για περισσότερα κανάλια που χωρούν στα 250 kS/s:

   powershell -ExecutionPolicy Bypass -File .\receive-example.ps1 -Frequency 10063,10081,10084

Πλήρης χειροκίνητη εκτέλεση:

   powershell -ExecutionPolicy Bypass -File .\start-dumphfdl.ps1 --soapysdr driver=rtlsdr --sample-rate 250000 10081

Το RTL-SDR πρέπει να χρησιμοποιεί οδηγό WinUSB. Αν ήδη λειτουργεί στο SDR++,
συνήθως δεν χρειάζεται αλλαγή. Μην αντιγράψεις μόνο το dumphfdl.exe: τα DLL και
ο φάκελος SoapySDR πρέπει να παραμείνουν μαζί του.

Build: dumphfdl commit bb7234b (version 1.7.0), Windows x86-64, SoapySDR 0.8.1,
SoapyRTLSDR 0.3.3, libacars 2.2.1, liquid-dsp 1.8.3.

Το πλήρες τροποποιημένο source και οι άδειες βρίσκονται στο source-code.zip.
