# jarvis.voice.v1

Der ESP32 verwendet denselben Voice-WebSocket-Vertrag wie der Linux-/ReSpeaker-Satellite.

## Client → Core

1. WebSocket-Verbindung zu `/api/v1/voice/live`
2. `hello` JSON für Client-/Versionsinformationen
3. `session.start` JSON
4. eine binäre WAV-Nachricht (`audio/wav`, PCM16, 16 kHz, mono)
5. `audio.commit` JSON

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

Primäre `jarvis.voice.v1`-Events:

- `ready`
- `session.started`
- `transcript.partial`
- `transcript.final`
- `assistant.final`
- TTS start/end events (`tts_start`, `tts_end` und kompatible Aliase)
- binäre TTS-Audiodaten (PCM16 oder PCM16-WAV)
- `error`

Zur Abwärtskompatibilität akzeptiert der ESP32 zusätzlich ältere Aliasnamen für Transkript-/Assistant-/TTS-Ereignisse.

## Authentifizierung

Wenn `JARVIS_CORE_TOKEN` gesetzt ist, sendet der ESP32 beim HTTP-WebSocket-Upgrade:

```http
Authorization: Bearer <token>
```

Der Token wird absichtlich nicht in `hello` übertragen und nicht geloggt.

## TTS / Audio zurück zum Satellite

Normale Sprachrunden setzen `auto_tts=true`. Der serielle Befehl `stt` setzt ihn zum isolierten STT-Test dagegen bewusst auf `false`; `tts` startet einen vollständigen STT→Assistant→TTS-Roundtrip.

Der ESP32 akzeptiert binäre PCM16-Daten sowie vollständige PCM16-WAV-Nachrichten. WAV-Header werden lokal ausgewertet; Mono/Stereo wird auf Mono reduziert und übliche TTS-Sampleraten (z. B. 22050/24000/48000 Hz) werden beim Streaming auf das interne 16-kHz-Ausgabeformat umgesetzt.
