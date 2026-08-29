#pragma once

// Diese Datei nach include/local_config.h kopieren.
// local_config.h liegt in .gitignore.

#define JARVIS_WIFI_SSID        "MeinWLAN"
#define JARVIS_WIFI_PASSWORD    "MeinPasswort"

#define JARVIS_CORE_HOST        "172.16.2.30"
#define JARVIS_CORE_PORT        8081
#define JARVIS_CORE_PATH        "/api/v1/voice/live"
#define JARVIS_CORE_TLS         0

// API-/Voice-Token. Wird als HTTP-Header beim WebSocket-Upgrade gesendet:
// Authorization: Bearer <token>
#define JARVIS_CORE_TOKEN       "jv_DEIN_TOKEN"

#define JARVIS_SATELLITE_ID     "satellite-livingroom"
#define JARVIS_SATELLITE_NAME   "Wohnzimmer"

// 0.1.x: Push-to-talk / Touch startet eine feste Aufnahme.
#define JARVIS_RECORD_MS        8000

// Normale Touch/BOOT-Sprachrunden sprechen die Antwort aus.
#define JARVIS_AUTO_TTS         1

// Gewuenschte TTS-Qualitaet fuer diesen ESP32:
//   "low"    -> Core quality=low (Default, schnell fuer Satelliten)
//   "medium" -> Core quality=balanced
//   "high"   -> Core quality=high
#define JARVIS_TTS_QUALITY      "low"

// Optional: Lautstärke für das Waveshare-Audioprofil (0..100).
#define JARVIS_WAVESHARE_SPEAKER_VOLUME 70
#define JARVIS_WAVESHARE_MIC_GAIN       70

// Display-Uhrzeit via NTP. POSIX-TZ fuer Deutschland inkl. Sommerzeit.
#define JARVIS_NTP_SERVER       "pool.ntp.org"
#define JARVIS_TIMEZONE_POSIX   "CET-1CEST,M3.5.0,M10.5.0/3"

// Nur anpassen, falls die Touch-Koordinaten auf einer Hardwarecharge gedreht sind.
#define JARVIS_WAVESHARE_TOUCH_SWAP_XY   0
#define JARVIS_WAVESHARE_TOUCH_INVERT_X  0
#define JARVIS_WAVESHARE_TOUCH_INVERT_Y  0
