#pragma once

#if __has_include("local_config.h")
#include "local_config.h"
#endif

#ifndef JARVIS_WIFI_SSID
#define JARVIS_WIFI_SSID "CHANGE_ME"
#endif
#ifndef JARVIS_WIFI_PASSWORD
#define JARVIS_WIFI_PASSWORD "CHANGE_ME"
#endif

#ifndef JARVIS_CORE_HOST
#define JARVIS_CORE_HOST "172.16.2.30"
#endif
#ifndef JARVIS_CORE_PORT
#define JARVIS_CORE_PORT 8081
#endif
#ifndef JARVIS_CORE_PATH
#define JARVIS_CORE_PATH "/api/v1/voice/live"
#endif
#ifndef JARVIS_CORE_TLS
#define JARVIS_CORE_TLS 0
#endif
#ifndef JARVIS_CORE_TOKEN
#define JARVIS_CORE_TOKEN ""
#endif

#ifndef JARVIS_SATELLITE_ID
#define JARVIS_SATELLITE_ID "satellite-esp32-01"
#endif
#ifndef JARVIS_SATELLITE_NAME
#define JARVIS_SATELLITE_NAME "ESP32 Satellite"
#endif

#ifndef JARVIS_RECORD_MS
#define JARVIS_RECORD_MS 8000
#endif

// Optional local end-of-speech detection. Recording always stops at
// JARVIS_RECORD_MS even when no speech/silence transition was detected.
#ifndef JARVIS_SILENCE_DETECTION
#define JARVIS_SILENCE_DETECTION 0
#endif
#ifndef JARVIS_SILENCE_THRESHOLD
#define JARVIS_SILENCE_THRESHOLD 500
#endif
#ifndef JARVIS_SILENCE_TIMEOUT_MS
#define JARVIS_SILENCE_TIMEOUT_MS 900
#endif
#ifndef JARVIS_SILENCE_MIN_SPEECH_MS
#define JARVIS_SILENCE_MIN_SPEECH_MS 250
#endif
#ifndef JARVIS_SILENCE_ARM_MS
#define JARVIS_SILENCE_ARM_MS 300
#endif

// ESP32-S3 WakeNet test wake word. The first implementation intentionally
// uses Espressif's bundled "Hi ESP" model (wn9_hiesp).
#ifndef JARVIS_WAKEWORD_ENABLED
#define JARVIS_WAKEWORD_ENABLED 0
#endif
#ifndef JARVIS_WAKEWORD_NAME
#define JARVIS_WAKEWORD_NAME "Hi ESP"
#endif
#ifndef JARVIS_AUDIO_RATE
#define JARVIS_AUDIO_RATE 16000
#endif
#ifndef JARVIS_AUDIO_CHANNELS
#define JARVIS_AUDIO_CHANNELS 1
#endif
#ifndef JARVIS_AUDIO_CHUNK_MS
#define JARVIS_AUDIO_CHUNK_MS 20
#endif

#ifndef JARVIS_AUTO_TTS
#define JARVIS_AUTO_TTS 1
#endif

// TTS quality requested from Jarvis Core for auto-TTS sessions.
// User-facing values: low, medium, high. "medium" is translated to
// the Core API quality name "balanced" for compatibility.
#ifndef JARVIS_TTS_QUALITY
#define JARVIS_TTS_QUALITY "low"
#endif


// Display rotation for boards with a screen. Values follow Arduino_GFX:
//   0 = default, 1 = 90 degrees clockwise,
//   2 = 180 degrees, 3 = 270 degrees clockwise.
// The Waveshare CST816 touch coordinates are rotated with the display.
#ifndef JARVIS_DISPLAY_ROTATION
#define JARVIS_DISPLAY_ROTATION 0
#endif

#ifndef JARVIS_NTP_SERVER
#define JARVIS_NTP_SERVER "pool.ntp.org"
#endif
#ifndef JARVIS_TIMEZONE_POSIX
#define JARVIS_TIMEZONE_POSIX "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

#ifndef JARVIS_WAVESHARE_TOUCH_SWAP_XY
#define JARVIS_WAVESHARE_TOUCH_SWAP_XY 0
#endif
#ifndef JARVIS_WAVESHARE_TOUCH_INVERT_X
#define JARVIS_WAVESHARE_TOUCH_INVERT_X 0
#endif
#ifndef JARVIS_WAVESHARE_TOUCH_INVERT_Y
#define JARVIS_WAVESHARE_TOUCH_INVERT_Y 0
#endif

#ifndef JARVIS_RECONNECT_MS
#define JARVIS_RECONNECT_MS 3000
#endif
#ifndef JARVIS_WIFI_TIMEOUT_MS
#define JARVIS_WIFI_TIMEOUT_MS 20000
#endif

#ifndef JARVIS_PROTOCOL_STRUCTURED
#define JARVIS_PROTOCOL_STRUCTURED 1
#endif

// Generic ESP32-S3 defaults. Override in local_config.h if needed.
#ifndef JARVIS_GENERIC_BUTTON_PIN
#define JARVIS_GENERIC_BUTTON_PIN 0
#endif
#ifndef JARVIS_GENERIC_MIC_BCLK_PIN
#define JARVIS_GENERIC_MIC_BCLK_PIN 4
#endif
#ifndef JARVIS_GENERIC_MIC_WS_PIN
#define JARVIS_GENERIC_MIC_WS_PIN 5
#endif
#ifndef JARVIS_GENERIC_MIC_DATA_PIN
#define JARVIS_GENERIC_MIC_DATA_PIN 6
#endif
#ifndef JARVIS_GENERIC_MIC_SHIFT
#define JARVIS_GENERIC_MIC_SHIFT 14
#endif
#ifndef JARVIS_GENERIC_MIC_RIGHT_SLOT
#define JARVIS_GENERIC_MIC_RIGHT_SLOT 0
#endif
#ifndef JARVIS_GENERIC_SPK_BCLK_PIN
#define JARVIS_GENERIC_SPK_BCLK_PIN 15
#endif
#ifndef JARVIS_GENERIC_SPK_WS_PIN
#define JARVIS_GENERIC_SPK_WS_PIN 16
#endif
#ifndef JARVIS_GENERIC_SPK_DATA_PIN
#define JARVIS_GENERIC_SPK_DATA_PIN 17
#endif

#ifndef JARVIS_WAVESHARE_SPEAKER_VOLUME
#define JARVIS_WAVESHARE_SPEAKER_VOLUME 70
#endif
#ifndef JARVIS_WAVESHARE_MIC_GAIN
#define JARVIS_WAVESHARE_MIC_GAIN 70
#endif

#ifndef JARVIS_SEND_HELLO
#define JARVIS_SEND_HELLO 0
#endif
