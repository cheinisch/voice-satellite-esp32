# Generic ESP32-S3

Generisches Testprofil für einen ESP32-S3 mit:

- digitalem I2S-Mikrofon (z. B. INMP441/ICS-43434),
- I2S-Verstärker/Lautsprecher (z. B. MAX98357A),
- BOOT-Taste als Push-to-talk.

Die Pins können in `include/local_config.h` überschrieben werden. Dieses Profil hat bewusst kein Display und keinen Touch.
