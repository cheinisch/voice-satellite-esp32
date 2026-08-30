#pragma once

// AIVOICE-SATELLITE_SATELLITE_VERSION and AIVOICE-SATELLITE_SATELLITE_BUILD are injected by
// scripts/platformio_version.py from the repository root files VERSION/BUILD.
#ifndef AIVOICE-SATELLITE_SATELLITE_VERSION
#error "AIVOICE-SATELLITE_SATELLITE_VERSION is missing. Build with PlatformIO so scripts/platformio_version.py can read VERSION."
#endif

#ifndef AIVOICE-SATELLITE_SATELLITE_BUILD
#error "AIVOICE-SATELLITE_SATELLITE_BUILD is missing. Build with PlatformIO so scripts/platformio_version.py can read BUILD."
#endif

#define AIVOICE-SATELLITE_PROTOCOL_NAME "ai-voice-satellite.voice.v1"
#define AIVOICE-SATELLITE_CLIENT_NAME "ai-voice-satellite-satellite-esp32"
