# jarvis.voice.v1

## Client → Core

1. WebSocket-Verbindung zu `/api/v1/voice/live`
2. `hello` JSON
3. `audio_start` JSON
4. binäre PCM16LE Frames
5. `audio_end` JSON

## Core → Client

Akzeptierte Text-Events:

- `hello`, `hello_ack`, `welcome`, `ready`
- `transcript`, `stt`, `user_text`
- `assistant`, `assistant_text`, `response`
- `tts_start`, `audio_output_start`
- `tts_end`, `audio_output_end`
- `error`

Binäre Frames werden als PCM16LE-TTS behandelt.
