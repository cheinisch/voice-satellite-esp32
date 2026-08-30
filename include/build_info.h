#pragma once

// VOICE_SATELLITE_VERSION and VOICE_SATELLITE_BUILD are injected by
// scripts/platformio_version.py from the repository root files VERSION/BUILD.
#ifndef VOICE_SATELLITE_VERSION
#error "VOICE_SATELLITE_VERSION is missing. Build with PlatformIO so scripts/platformio_version.py can read VERSION."
#endif

#ifndef VOICE_SATELLITE_BUILD
#error "VOICE_SATELLITE_BUILD is missing. Build with PlatformIO so scripts/platformio_version.py can read BUILD."
#endif

#define VOICE_SATELLITE_PROTOCOL_NAME "voice.satellite.v1"
#define VOICE_SATELLITE_CLIENT_NAME "voice-satellite-esp32"
