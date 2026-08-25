# jarvis-satellite-esp32

Eigenständige Firmware für **ESP32-basierte Jarvis Voice Satellites**.

> Dieses Repository hat bewusst **nichts mit dem Linux-Satellite-Repository zu tun**. Linux-Satelliten werden separat entwickelt. Hier liegen ausschließlich Firmware, Hardwareprofile und Build-Dateien für ESP32/ESP32-S3.

## Architektur

```text
Mikrofon / Touch / Taste
        ↓
jarvis-satellite-esp32
        ↓  PCM16 16 kHz Mono / WebSocket
Jarvis Voice Core :8081
        ↓
STT → Assistant → TTS
        ↓  PCM16
ESP32 Satellite
        ↓
Lautsprecher / Display
```

Der ESP32 führt keine serverseitige STT-, TTS- oder LLM-Logik aus. Das Gerät ist Audio-/UI-Endpunkt.

## Unterstützte Hardware

### `waveshare-1_85c` – primäres Board

Waveshare **ESP32-S3-Touch-LCD-1.85C V2 / Rev2.0**:

- 360×360 ST77916 QSPI Display
- CST816 Touch
- ES7210 Dual-Mikrofon-ADC
- ES8311 Audio-Codec
- Lautsprecherverstärker
- 16 MB Flash
- 8 MB PSRAM
- microSD vorhanden (in 0.1.0 noch nicht benutzt)

**V1 ist nicht Bestandteil dieses Repositories.**

### `generic-esp32s3`

Testprofil für ESP32-S3 + externes digitales I2S-Mikrofon + I2S-Verstärker.

## Repository-Struktur

```text
jarvis-satellite-esp32/
├── include/                         gemeinsame Interfaces / Config
├── src/                             hardwareunabhängiger Satellite-Core
│   ├── core/
│   ├── network/
│   └── protocol/
├── boards/
│   ├── generic-esp32s3/
│   └── waveshare-esp32-s3-touch-lcd-1.85c/
├── docs/
├── scripts/
├── .github/workflows/
│   ├── build.yml
│   ├── build-number.yml
│   └── create-release.yml
├── platformio.ini
├── VERSION
└── BUILD
```

Board-spezifische GPIOs, Display-, Touch- und Codec-Logik bleiben unter `boards/`. Der Core kennt keine Waveshare-Pins.

## Konfiguration

```bash
cp include/local_config.example.h include/local_config.h
```

Dann WLAN, Core und Satellite-ID anpassen:

```cpp
#define JARVIS_WIFI_SSID      "MeinWLAN"
#define JARVIS_WIFI_PASSWORD  "..."
#define JARVIS_CORE_HOST      "172.16.2.30"
#define JARVIS_CORE_PORT      8081
#define JARVIS_CORE_PATH      "/api/v1/voice/live"
#define JARVIS_SATELLITE_ID   "satellite-livingroom"
#define JARVIS_SATELLITE_NAME "Wohnzimmer"
```

`include/local_config.h` wird nicht in Git eingecheckt.

## Bauen

Waveshare:

```bash
pio run -e waveshare-1_85c
```

Generisch:

```bash
pio run -e generic-esp32s3
```

## Flashen

```bash
pio run -e waveshare-1_85c -t upload
pio device monitor -b 115200
```

## Bedienung 0.1.x

Waveshare:

- Touch antippen → Aufnahme startet
- erneut antippen → Aufnahme vorzeitig beenden
- alternativ BOOT-Taste
- ohne zweiten Trigger endet die Aufnahme nach 8 Sekunden

Generisches ESP32-S3:

- BOOT-Taste → Aufnahme starten/stoppen

Typische Ausgabe:

```text
Jarvis ESP32 Satellite 0.1.0 Build 3
Client: jarvis-satellite-esp32
Board: Waveshare ESP32-S3-Touch-LCD-1.85C V2
WLAN verbunden ...
Core bereit: jarvis.voice.v1

Aufnahme läuft (8s)...
Sende Daten an Jarvis
Du: Wie wird das Wetter?
Jarvis: Morgen wird es ...
TTS Wiedergabe ...
TTS beendet.
```

## Voice-Protokoll

Handshake:

```json
{
  "type": "hello",
  "protocol": "jarvis.voice.v1",
  "client": "jarvis-satellite-esp32",
  "client_version": "0.1.0",
  "client_build": 3,
  "satellite_id": "satellite-livingroom",
  "board_profile": "waveshare-esp32-s3-touch-lcd-1.85c",
  "audio": {
    "format": "pcm_s16le",
    "sample_rate": 16000,
    "channels": 1
  }
}
```

Audio-Uplink wird als rohes PCM16LE gesendet. TTS kann als binäres PCM16LE zurückkommen.

## Entwicklungsgrenzen

0.1.0 konzentriert sich auf:

- Board-Abstraktion
- WLAN
- Voice-WebSocket
- Mikrofon-Uplink
- TTS-Playback
- Waveshare Display/Touch
- Versionsinformationen

Später vorgesehen:

- VAD / automatische Aufnahmebeendigung
- lokales Wake Word
- OTA
- Einstellungs-UI
- Full-Duplex / Bar​ge-in

## Waveshare-Hinweis

Das Hardwareprofil ist ausschließlich auf **V2 / Rev2.0** ausgelegt. Display- und Codec-Bring-up sind bewusst isoliert, sodass Hardwareanpassungen nicht das Voice-Protokoll oder den Satellite-Core verändern.


## Version und Build

Die Firmwareversion liegt in `VERSION`, die fortlaufende Buildnummer in `BUILD`. Beide Werte werden beim PlatformIO-Build automatisch in die Firmware übernommen. `include/build_info.h` enthält deshalb keine separat gepflegte Versionsnummer mehr.

Bei einem normalen Push auf den Default-Branch erhöht `.github/workflows/build-number.yml` `BUILD`, commitet die neue Nummer zurück und baut Generic sowie Waveshare. Pull Requests kompilieren nur und verändern weder `VERSION` noch `BUILD`.

Releases werden über **Actions → Create firmware release → Run workflow** erzeugt. Dort wird `patch`, `minor`, `major` oder `custom` gewählt. Der Workflow erhöht automatisch `VERSION` und `BUILD`, baut die Firmware, erstellt den Release-Commit und den passenden Git-Tag und veröffentlicht anschließend das GitHub Release.

Beispiel bei `VERSION=0.1.0`:

```text
patch  → 0.1.1
minor  → 0.2.0
major  → 1.0.0
custom → frei angegebene höhere SemVer-Version
```

Details: [`docs/versioning.md`](docs/versioning.md).

## Automatische Waveshare-Releasepakete

Der Workflow **Create firmware release** baut automatisch `waveshare-1_85c`. Das Release erhält anschließend:

```text
...-app.bin       Applikations-Firmware
...-factory.bin   vollständiges Flash-Image ab 0x0
...zip            komplettes Waveshare-Paket
SHA256SUMS.txt    Prüfsummen
```

Das ZIP enthält zusätzlich `bootloader.bin`, `partitions.bin`, `manifest.json` und Flash-Hinweise.
