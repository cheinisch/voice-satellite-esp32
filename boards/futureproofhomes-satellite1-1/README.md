# FutureProofHomes Satellite1.1

Experimentelles Boardprofil für den **FutureProofHomes Satellite1.1 Dev Kit / Smart Speaker**.

## Hardwarebasis

- ESP32-S3 N16R8 (16 MB Flash, 8 MB PSRAM)
- XMOS XU316 Audio Processor
- vier Mikrofone (XMOS-Frontend)
- 25-W-Lautsprecherpfad / Line-Out
- 24-LED-Ring
- Action-/Volume-/Mute-Tasten
- optionale Sensorik/mmWave

## Stand in Jarvis 0.1.x

Implementiert:

- eigenes PlatformIO-Target `satellite1-1`
- XMOS Reset und Firmware-Versionsprobe über SPI
- 48 kHz / 32-bit / Stereo I2S am Satellite1.1
- Mikrofonpfad: 48 kHz Stereo -> 16 kHz PCM16 Mono für `jarvis.voice.v1`
- TTS-Pfad: 16 kHz PCM16 Mono -> 48 kHz Stereo/32-bit I2S
- direkte Action-Taste (GPIO0) als Push-to-talk
- Status-LED GPIO45
- 16 MB Flash / 8 MB PSRAM

Noch experimentell / nicht vollständig implementiert:

- TAS2780-Verstärker- und PCM5122-Line-Out-Initialisierung
- 24-LED-Ring
- Volume-/Hardware-Mute-Tasten über XMOS GPIO-Service
- automatische XMOS-Firmwareinstallation
- Umweltsensoren/mmWave

**Wichtig:** Für den ersten Jarvis-Test sollte der XMOS zuvor einmal mit der offiziellen FutureProofHomes-Firmware provisioniert worden sein. Jarvis überschreibt die XMOS-Firmware nicht.

Quellen für Pinout/Audioformat:

- https://github.com/FutureProofHomes/Satellite1-ESPHome
- https://docs.futureproofhomes.net/satellite1-introduction/
