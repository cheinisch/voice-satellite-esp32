# Changelog

## 0.1.0 Build 4

- Fix: gemeinsame Header unter `include/` werden beim Kompilieren der Board-Libraries explizit in den Include-Pfad aufgenommen.
- Fix: der Push-Workflow committet `BUILD` erst, nachdem Generic- und Waveshare-Firmware erfolgreich gebaut wurden.
- Fix: Branch-Race-Schutz verhindert einen veralteten Build-Commit, wenn während des Builds ein neuer Push eingeht.

## 0.1.0 - initial ESP32 repository

- Neues eigenständiges Repository `jarvis-satellite-esp32`.
- Keine Linux-Satellite-Komponenten in diesem Repository.
- Gemeinsamer Voice-Satellite-Core für ESP32-S3.
- Board-Abstraktion über PlatformIO-Libraries unter `boards/`.
- Generisches ESP32-S3-Profil für externes I2S-Mikrofon und I2S-Lautsprecher.
- Erstes Display-Board: Waveshare ESP32-S3-Touch-LCD-1.85C **V2 / Rev2.0**.
- V1 wird bewusst nicht unterstützt.
- `jarvis.voice.v1` WebSocket-Protokoll, PCM16/16 kHz/Mono zum Core.
- TTS-Rückkanal über binäre PCM16-Frames.
- Firmware-/Build-Version im Handshake.
- Waveshare: ST77916, CST816, TCA9554, ES8311, ES7210, Touch-Trigger und Status-UI vorbereitet.

### Build-/Release-Automation

- `VERSION` und `BUILD` sind jetzt die alleinige Quelle für Firmware-Version und Buildnummer.
- PlatformIO übernimmt beide Werte automatisch über `scripts/platformio_version.py` in die Firmware.
- GitHub Actions erhöht `BUILD` bei normalen Pushes auf den Default-Branch automatisch; Pull Requests verändern keine Buildnummer.
- Manueller Workflow `Create firmware release` unterstützt `patch`, `minor`, `major` und eine explizite höhere SemVer-Version.
- Der Release-Workflow aktualisiert `VERSION` und `BUILD`, baut vor dem Push, erstellt Release-Commit und Tag und veröffentlicht danach automatisch das GitHub Release.
- Waveshare-Releases enthalten App-Binary, zusammengeführtes Factory-Binary, ZIP-Paket, Manifest und SHA-256-Prüfsummen.

## Unreleased

### Fixed
- PlatformIO board isolation: the Generic ESP32-S3 build now ignores the Waveshare board library and the Waveshare build ignores the Generic board library.
- Prevents the PlatformIO LDF from compiling Waveshare-only dependencies such as `AudioBoard.h` during the Generic target.
