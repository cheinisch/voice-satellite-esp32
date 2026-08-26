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
  "auto_tts": false,
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
- `error`

Zur Abwärtskompatibilität akzeptiert der ESP32 zusätzlich ältere Aliasnamen für Transkript-/Assistant-/TTS-Ereignisse.

## Authentifizierung

Wenn `JARVIS_CORE_TOKEN` gesetzt ist, sendet der ESP32 beim HTTP-WebSocket-Upgrade:

```http
Authorization: Bearer <token>
```

Der Token wird absichtlich nicht in `hello` übertragen und nicht geloggt.
