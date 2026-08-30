"""Patch arduinoWebSockets for large inbound TTS frames on ESP32-S3.

arduinoWebSockets 2.7.x limits a single inbound frame to 15 KiB and allocates
that complete frame from the normal heap before invoking the application
callback. Ai-Voice-Satellite TTS PCM/WAV responses can be several hundred KiB, while the
Waveshare has 8 MiB PSRAM. This build-time patch raises the frame limit and
places large receive buffers in PSRAM.

The repair section also fixes files modified by the first Ai-Voice-Satellite version of
this script, which accidentally wrote the two characters "\\n" instead of
real newlines into the dependency source.
"""
from pathlib import Path
import re

Import("env")  # type: ignore[name-defined]
if env.IsIntegrationDump():
    Return()

pioenv = env.subst("$PIOENV")
libdeps = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / pioenv
header = libdeps / "WebSockets" / "src" / "WebSockets.h"
source = libdeps / "WebSockets" / "src" / "WebSockets.cpp"
marker = "AIVOICE-SATELLITE_WEBSOCKETS_PSRAM_PATCH"

if not header.exists() or not source.exists():
    raise RuntimeError(f"Ai-Voice-Satellite WebSockets dependency not found under {libdeps}")

# ---------------------------------------------------------------------------
# WebSockets.h: raise the maximum inbound frame size.
# ---------------------------------------------------------------------------
h = header.read_text(encoding="utf-8")

bad_header_block = (
    f"// {marker}: Ai-Voice-Satellite TTS can exceed the upstream 15 KiB receive limit.\\n"
    "#define WEBSOCKETS_MAX_DATA_SIZE (2 * 1024 * 1024)"
)
good_header_block = (
    f"// {marker}: Ai-Voice-Satellite TTS can exceed the upstream 15 KiB receive limit.\n"
    "#define WEBSOCKETS_MAX_DATA_SIZE (2 * 1024 * 1024)"
)

if bad_header_block in h:
    h = h.replace(bad_header_block, good_header_block, 1)

if marker not in h:
    pattern = r"#define WEBSOCKETS_MAX_DATA_SIZE \(15 \* 1024\)"
    h, count = re.subn(pattern, good_header_block, h, count=1)
    if count != 1:
        raise RuntimeError("Could not patch WEBSOCKETS_MAX_DATA_SIZE in WebSockets.h")

# Validate that the define did not accidentally remain inside a // comment.
if "\\n#define WEBSOCKETS_MAX_DATA_SIZE" in h:
    raise RuntimeError("WebSockets.h still contains an escaped newline from an old Ai-Voice-Satellite patch")
if "#define WEBSOCKETS_MAX_DATA_SIZE (2 * 1024 * 1024)" not in h:
    raise RuntimeError("Ai-Voice-Satellite WebSockets receive-size define is missing after patch")

header.write_text(h, encoding="utf-8")

# ---------------------------------------------------------------------------
# WebSockets.cpp: allocate large inbound frames from PSRAM.
# ---------------------------------------------------------------------------
c = source.read_text(encoding="utf-8")

bad_include_block = (
    f"// {marker}\\n"
    "#if defined(ESP32)\\n"
    "#include <esp_heap_caps.h>\\n"
    "#endif\\n"
)
good_include_block = (
    f"// {marker}\n"
    "#if defined(ESP32)\n"
    "#include <esp_heap_caps.h>\n"
    "#endif\n"
)

bad_alloc_block = (
    "#if defined(ESP32) && defined(BOARD_HAS_PSRAM)\\n"
    "            if(header->payloadLen > (16 * 1024)) {\\n"
    "                payload = (uint8_t *)heap_caps_malloc(header->payloadLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);\\n"
    "            } else {\\n"
    "                payload = (uint8_t *)malloc(header->payloadLen + 1);\\n"
    "            }\\n"
    "            if(!payload) {\\n"
    "                payload = (uint8_t *)malloc(header->payloadLen + 1);\\n"
    "            }\\n"
    "#else\\n"
    "            payload = (uint8_t *)malloc(header->payloadLen + 1);\\n"
    "#endif"
)
good_alloc_block = (
    "#if defined(ESP32) && defined(BOARD_HAS_PSRAM)\n"
    "            if(header->payloadLen > (16 * 1024)) {\n"
    "                payload = (uint8_t *)heap_caps_malloc(\n"
    "                    header->payloadLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);\n"
    "            } else {\n"
    "                payload = (uint8_t *)malloc(header->payloadLen + 1);\n"
    "            }\n"
    "            if(!payload) {\n"
    "                payload = (uint8_t *)malloc(header->payloadLen + 1);\n"
    "            }\n"
    "#else\n"
    "            payload = (uint8_t *)malloc(header->payloadLen + 1);\n"
    "#endif"
)

# Repair a dependency already corrupted by the previous script version.
if bad_include_block in c:
    c = c.replace(bad_include_block, good_include_block, 1)
if bad_alloc_block in c:
    c = c.replace(bad_alloc_block, good_alloc_block, 1)

if marker not in c:
    include_needle = '#include "WebSockets.h"\n'
    if include_needle not in c:
        raise RuntimeError("Could not find WebSockets.h include in WebSockets.cpp")
    c = c.replace(include_needle, include_needle + good_include_block, 1)

    alloc_needle = "payload = (uint8_t *)malloc(header->payloadLen + 1);"
    if alloc_needle not in c:
        raise RuntimeError("Could not find inbound payload allocation in WebSockets.cpp")
    c = c.replace(alloc_needle, good_alloc_block, 1)

# If the marker was already present but only the include block had been fixed,
# ensure the allocation patch exists as well.
if "heap_caps_malloc" not in c:
    alloc_needle = "payload = (uint8_t *)malloc(header->payloadLen + 1);"
    if alloc_needle not in c:
        raise RuntimeError("Could not find inbound payload allocation while repairing WebSockets.cpp")
    c = c.replace(alloc_needle, good_alloc_block, 1)

# Fail early with a readable error instead of letting the C++ preprocessor
# report a cryptic 'token \\"\\ is not valid' message.
for escaped in (
    f"// {marker}\\n",
    "#if defined(ESP32)\\n",
    "#if defined(ESP32) && defined(BOARD_HAS_PSRAM)\\n",
    "#else\\n",
    "#endif\\n",
):
    if escaped in c:
        raise RuntimeError(
            "WebSockets.cpp still contains escaped newlines from an old Ai-Voice-Satellite patch; "
            "delete .pio/libdeps for this environment and rebuild"
        )

source.write_text(c, encoding="utf-8")
print(f"Ai-Voice-Satellite WebSockets patch: large inbound frames use PSRAM ({pioenv})")
