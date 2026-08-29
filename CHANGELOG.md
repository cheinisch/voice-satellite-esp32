
## Unreleased - Waveshare audio bring-up

### TTS bring-up

- Waveshare ES8311/NS4150B-Ausgabe in den Voice-Roundtrip eingebunden
- `JARVIS_AUTO_TTS` (Default 1) für normale Sprachrunden
- `spk` als lokaler 1-kHz-Speaker-Test ohne Core
- `tts` als kompletter STT→Assistant→TTS-Roundtrip-Test
- `stt` bleibt isolierter STT-Test mit `auto_tts=false`
- binäre PCM16- und PCM16-WAV-TTS-Daten werden erkannt
- WAV-Metadaten (Rate/Kanäle/16 Bit) werden ausgewertet
- einfache Streaming-Samplerate-Konvertierung auf 16 kHz
- PA wird zwischen Ausgaben gemutet und bei Playback wieder aktiviert

- Waveshare microphone path is initialized before and independently from ES8311 speaker output.
- ES8311 is initialized lazily on first TTS playback and can no longer block STT/microphone bring-up.
- Added explicit Waveshare hardware initialization stage logs.
- Disabled client `hello` by default because the current Jarvis voice core already sends `ready` and rejects `hello`.
- Added normal handling for the core `reset` session event and de-duplicated repeated `ready` events.
- Network/protocol diagnostics remain available even when board initialization is incomplete.
# Changelog

## Unreleased

- Waveshare V2: ES7210 microphone codec now uses a board-specific initialization sequence matching the official Waveshare 16 kHz / 16-bit / stereo reference profile instead of the generic arduino-audio-driver ES7210 wrapper.
- Added boot-time I2C probes for ES8311 (0x18) and ES7210 (0x40), ES7210 register readback, and richer zero-RX diagnostics.
- Waveshare shared I2C bus reduced to 100 kHz to match the vendor Arduino reference during hardware bring-up.

### Added
- Serieller `mic`-Hardwaretest: misst zwei Sekunden I2S-Eingang ohne Jarvis/STT und zeigt Samples, Min/Max, mittleren Absolutpegel und Non-Zero-Anteil.
- Bearer-Token-Unterstützung für den Voice-WebSocket (`JARVIS_CORE_TOKEN`).
- Token wird als `Authorization: Bearer` beim WebSocket-Upgrade gesendet und nicht geloggt.
- Serielle STT-Testkonsole mit `stt`, `stop`, `status` und `help`.
- STT-Transkript wird für Testläufe zusätzlich als `STT Ergebnis` ausgegeben.

### Changed
- ESP32-Voice-Uplink an den bereits produktiv verwendeten `jarvis.voice.v1`-Vertrag des Linux-/ReSpeaker-Satelliten angepasst: `session.start` -> binäre WAV-Nachricht -> `audio.commit`.
- Push-to-talk-Aufnahmen werden in 0.1.x lokal gepuffert und als PCM16/16-kHz/Mono-WAV übertragen.
- Native Server-Events `session.started`, `transcript.partial`, `transcript.final` und `assistant.final` werden ausgewertet.

### Fixed
- Waveshare-I2S an das offizielle V2-Arduino-Beispiel angeglichen: automatischer I2S-Port statt erzwungenem I2S1, expliziter Stream-Timeout und gedrosselte RX-Diagnose bei 0-Byte-Reads.
- Waveshare runtime I2C fix: the board now owns the shared `Wire` bus exclusively; ES8311/ES7210 reuse the initialized bus instead of reinitializing it.
- Correct `arduino-audio-driver` v0.2.0 shared-Wire registration so codec default addresses (ES8311 `0x18`, ES7210 `0x40`) are retained.
- CST816 touch polling now uses the active-low interrupt instead of continuously reading I2C every ~35 ms, preventing serial error floods when I2C is unavailable.
- Waveshare 1.85C audio driver updated for `arduino-audio-driver` v0.2.0 (`DriverPins` API, default ES8311/ES7210 constructors, correct I2C port argument).
- Use the `const char*` WebSocketsClient `sendTXT` overload so JSON messages compile with arduinoWebSockets 2.7.3 while keeping `sendJson(const String&)` const-correct.
- PlatformIO board isolation: the Generic ESP32-S3 build ignores the Waveshare board library and the Waveshare build ignores the Generic board library.

## 0.1.0 Build 4
- FutureProofHomes Satellite1.1 als drittes ESP32-S3-Boardprofil (`satellite1-1`) ergänzt.
- XMOS SPI-Versionsprobe, Satellite1.1 48-kHz-I2S-Audiopfad und Action-Taste vorbereitet.
- CI baut jetzt Generic, Waveshare und Satellite1.1.
- Release-Workflow erzeugt zusätzlich Satellite1.1 Firmwarepakete.

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
