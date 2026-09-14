# voice.satellite.v1

Der ESP32 verwendet denselben Voice-WebSocket-Vertrag wie der Linux-/ReSpeaker-Satellite.

## Client → Core

1. WebSocket-Verbindung zu `/api/v1/voice/live`
2. Core sendet `ready`
3. ESP32 sendet `client.info` mit stabiler `hardware_id` aus der eFuse/Base-MAC
4. Core antwortet mit `satellite.config` und dem Core-verwalteten Gerätenamen
5. `session.start` JSON
6. eine binäre WAV-Nachricht (`audio/wav`, PCM16, 16 kHz, mono)
7. `audio.commit` JSON

`client.info`:

```json
{
  "type": "client.info",
  "client": {
    "hardware_id": "34:85:18:ab:cd:ef",
    "id": "esp32-satellite",
    "name": "ESP32 Satellite",
    "platform": "esp32",
    "board": "Waveshare ESP32-S3-Touch-LCD-1.85C V2",
    "version": "1.0.0",
    "build": 1
  },
  "audio": {
    "format": "pcm_s16le",
    "sample_rate": 16000,
    "channels": 1,
    "bits_per_sample": 16,
    "max_binary_frame_bytes": 14336,
    "preferred_tts_chunk_bytes": 12288
  },
  "client_max_binary_frame_bytes": 14336,
  "preferred_tts_chunk_bytes": 12288,
  "tts_output": {
    "containers": ["wav"],
    "sample_formats": ["pcm_s16le"],
    "sample_rates": [16000],
    "channels": [1],
    "bits_per_sample": [16],
    "preferred": {
      "container": "wav",
      "sample_format": "pcm_s16le",
      "sample_rate": 16000,
      "channels": 1,
      "bits_per_sample": 16
    }
  }
}
```

`client.id` und `client.name` sind nur Diagnoseinformationen. Die dauerhafte
Identität ist ausschließlich `client.hardware_id`. Sie wird aus der
werksseitigen eFuse/Base-MAC gelesen und bleibt bei Reconnects identisch.

`tts_output` beschreibt ausschließlich das Format, das der Satellite für die
Wiedergabe vom Core erwartet. Provider dürfen andere Formate erzeugen. Der Core
normalisiert diese vor der Übertragung auf das bevorzugte Format. Beim aktuellen
ESP32-Protokollpfad ist das `wav` mit PCM16, 16 kHz, mono und 16 Bit. Die
Frame-/Chunk-Werte werden sowohl top-level als auch im `audio`-Block gesendet,
damit neue und ältere Core-Versionen denselben Streaming-Vertrag nutzen können.

`session.start`:

```json
{
  "type": "session.start",
  "language": "de",
  "auto_chat": true,
  "auto_tts": true,
  "content_type": "audio/wav"
}
```

Die 0.1.x-Firmware puffert die Push-to-talk-Aufnahme lokal, setzt einen PCM-WAV-Header davor und sendet die vollständige WAV-Datei als binäre WebSocket-Nachricht. Erst `audio.commit` startet die finale serverseitige Verarbeitung.

## Core → Client

Primäre `voice.satellite.v1`-Events:

- `ready`
- `session.started`
- `transcript.partial`
- `transcript.final`
- `assistant.final`
- TTS start/end events (`tts_start`, `tts_end` und kompatible Aliase)
- binäre TTS-Audiodaten (PCM16 oder PCM16-WAV)
- `error`

Zur Abwärtskompatibilität akzeptiert der ESP32 zusätzlich ältere Aliasnamen für Transkript-/Assistant-/TTS-Ereignisse.


## Registrierung / Core-Name

Nach erfolgreicher Registrierung sendet der Core beispielsweise:

```json
{
  "type": "satellite.config",
  "hardware_id": "34:85:18:ab:cd:ef",
  "name": "Wohnzimmer",
  "config": {"name": "Wohnzimmer"}
}
```

Der ESP32 übernimmt diesen Namen für die lokale Anzeige. Eine Umbenennung im
Command Center wird über dieselbe Nachricht ohne Neustart auf den verbundenen
ESP32 gepusht. Fehlt die Hardware-ID, kann der Core `satellite.identity.required`
senden; Registrierungsfehler werden als `satellite.identity.error` gemeldet.

## Authentifizierung

Wenn `VOICE_SATELLITE_CORE_TOKEN` gesetzt ist, sendet der ESP32 beim HTTP-WebSocket-Upgrade:

```http
Authorization: Bearer <token>
```

Der Token wird absichtlich nicht in `hello` übertragen und nicht geloggt.

## TTS / Audio zurück zum Satellite

Normale Sprachrunden setzen `auto_tts=true`. Der serielle Befehl `stt` setzt ihn zum isolierten STT-Test dagegen bewusst auf `false`; `tts` startet einen vollständigen STT→Assistant→TTS-Roundtrip.

Der ESP32 akzeptiert binäre PCM16-Daten sowie vollständige PCM16-WAV-Nachrichten. WAV-Header werden lokal ausgewertet; Mono/Stereo wird auf Mono reduziert und übliche TTS-Sampleraten (z. B. 22050/24000/48000 Hz) werden beim Streaming auf das interne 16-kHz-Ausgabeformat umgesetzt.
