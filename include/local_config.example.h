#pragma once

// Diese Datei nach include/local_config.h kopieren.
// local_config.h liegt in .gitignore.

#define JARVIS_WIFI_SSID        "MeinWLAN"
#define JARVIS_WIFI_PASSWORD    "MeinPasswort"

#define JARVIS_CORE_HOST        "172.16.2.30"
#define JARVIS_CORE_PORT        8081
#define JARVIS_CORE_PATH        "/api/v1/voice/live"
#define JARVIS_CORE_TLS         0

#define JARVIS_SATELLITE_ID     "satellite-livingroom"
#define JARVIS_SATELLITE_NAME   "Wohnzimmer"

// 0.1.x: Push-to-talk / Touch startet eine feste Aufnahme.
#define JARVIS_RECORD_MS        8000

// Optional: Lautstärke für das Waveshare-Audioprofil (0..100).
#define JARVIS_WAVESHARE_SPEAKER_VOLUME 70
#define JARVIS_WAVESHARE_MIC_GAIN       70
