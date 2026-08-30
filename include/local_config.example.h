#pragma once

// Diese Datei nach include/local_config.h kopieren.
// local_config.h liegt in .gitignore.

#define VOICE_SATELLITE_WIFI_SSID        "MeinWLAN"
#define VOICE_SATELLITE_WIFI_PASSWORD    "MeinPasswort"

#define VOICE_SATELLITE_CORE_HOST        "172.16.2.30"
#define VOICE_SATELLITE_CORE_PORT        8081
#define VOICE_SATELLITE_CORE_PATH        "/api/v1/voice/live"
#define VOICE_SATELLITE_CORE_TLS         0

// API-/Voice-Token. Wird als HTTP-Header beim WebSocket-Upgrade gesendet:
// Authorization: Bearer <token>
#define VOICE_SATELLITE_CORE_TOKEN       "jv_DEIN_TOKEN"

#define VOICE_SATELLITE_ID     "satellite-livingroom"
#define VOICE_SATELLITE_NAME   "Wohnzimmer"

// Maximale Aufnahmedauer. Bei aktivierter Silence-Erkennung kann eine
// Aufnahme vorher automatisch beendet werden.
#define VOICE_SATELLITE_RECORD_MS        8000

// Optional: lokale Silence-Erkennung.
// threshold = mittlerer Absolutpegel eines 20-ms-Audioblocks. Bei Bedarf mit
// dem seriellen "mic"-Test an die reale Mikrofonumgebung anpassen.
#define VOICE_SATELLITE_SILENCE_DETECTION      1
#define VOICE_SATELLITE_SILENCE_THRESHOLD      500
#define VOICE_SATELLITE_SILENCE_TIMEOUT_MS     900
#define VOICE_SATELLITE_SILENCE_MIN_SPEECH_MS  250
#define VOICE_SATELLITE_SILENCE_ARM_MS         300

// Optional: lokales Wakeword auf dem ESP32-S3 via Arduino ESP_SR/WakeNet.
// Testmodell der ersten Version: "Hi ESP" (wn9_hiesp).
// Beim Aktivieren wird srmodels.bin beim PlatformIO-Upload automatisch in
// die model-Partition geflasht.
#define VOICE_SATELLITE_WAKEWORD_ENABLED       0
#define VOICE_SATELLITE_WAKEWORD_NAME          "Hi ESP"

// Nach Wakeword: dynamische Begruessung vom Core per TTS. Falls der Core/TTS
// nicht erreichbar ist, wird lokal ein kurzer Signalton abgespielt.
#define VOICE_SATELLITE_WAKE_GREETING_ENABLED  1
#define VOICE_SATELLITE_WAKE_GREETING_CONTEXT  "wakeword"
#define VOICE_SATELLITE_WAKE_ACK_TONE_ENABLED  1

// LLM direkt bei Wakeword-Erkennung im Hintergrund vorladen.
#define VOICE_SATELLITE_LLM_WAKEUP_ENABLED      1
#define VOICE_SATELLITE_LLM_WAKEUP_KEEP_ALIVE  "10m"

// Normale Touch/BOOT-Sprachrunden sprechen die Antwort aus.
#define VOICE_SATELLITE_AUTO_TTS         1

// Gewuenschte TTS-Qualitaet fuer diesen ESP32:
//   "low"    -> Core quality=low (Default, schnell fuer Satelliten)
//   "medium" -> Core quality=balanced
//   "high"   -> Core quality=high
#define VOICE_SATELLITE_TTS_QUALITY      "low"

// Optional: Lautstärke für das Waveshare-Audioprofil (0..100).
#define VOICE_SATELLITE_WAVESHARE_SPEAKER_VOLUME 70
#define VOICE_SATELLITE_WAVESHARE_MIC_GAIN       70

// Optional: display rotation. Degree values are recommended; the old
// Arduino_GFX indices 1/2/3 remain supported for compatibility.
//   0   = default
//   90  = 90 degrees clockwise
//   180 = 180 degrees
//   270 = 270 degrees clockwise
// Waveshare touch coordinates are rotated together with the display.
#define VOICE_SATELLITE_DISPLAY_ROTATION  0

// Display-Uhrzeit via NTP. POSIX-TZ fuer Deutschland inkl. Sommerzeit.
#define VOICE_SATELLITE_NTP_SERVER       "pool.ntp.org"
#define VOICE_SATELLITE_TIMEZONE_POSIX   "CET-1CEST,M3.5.0,M10.5.0/3"

// Nur anpassen, falls die Touch-Koordinaten auf einer Hardwarecharge gedreht sind.
#define VOICE_SATELLITE_WAVESHARE_TOUCH_SWAP_XY   0
#define VOICE_SATELLITE_WAVESHARE_TOUCH_INVERT_X  0
#define VOICE_SATELLITE_WAVESHARE_TOUCH_INVERT_Y  0
