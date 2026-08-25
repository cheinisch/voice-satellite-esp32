#pragma once

// JARVIS_SATELLITE_VERSION and JARVIS_SATELLITE_BUILD are injected by
// scripts/platformio_version.py from the repository root files VERSION/BUILD.
#ifndef JARVIS_SATELLITE_VERSION
#error "JARVIS_SATELLITE_VERSION is missing. Build with PlatformIO so scripts/platformio_version.py can read VERSION."
#endif

#ifndef JARVIS_SATELLITE_BUILD
#error "JARVIS_SATELLITE_BUILD is missing. Build with PlatformIO so scripts/platformio_version.py can read BUILD."
#endif

#define JARVIS_PROTOCOL_NAME "jarvis.voice.v1"
#define JARVIS_CLIENT_NAME "jarvis-satellite-esp32"
