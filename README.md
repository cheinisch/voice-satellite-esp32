# jarvis-satellite-esp32

Standalone firmware for **ESP32-based Jarvis Voice Satellites**.

> This repository is intentionally **separate from the Linux satellite repository**. Linux satellites are developed independently. This repository contains only firmware, hardware profiles, and build files for ESP32/ESP32-S3 devices.

## Architecture

```text
Microphone / Touch / Buttons
          ↓
jarvis-satellite-esp32
          ↓  PCM16 16 kHz Mono / WebSocket
Jarvis Voice Core :8081
          ↓
STT → Assistant → TTS
          ↓  PCM16
ESP32 Satellite
          ↓
Speaker / Display
```

The ESP32 does not run server-side STT, TTS, or LLM logic. It acts as an audio and UI endpoint.

## Supported Hardware

### `waveshare-1_85c` – Primary Board

Waveshare **ESP32-S3-Touch-LCD-1.85C V2 / Rev2.0**:

- 360×360 ST77916 QSPI display
- CST816 touch controller
- ES7210 dual-microphone ADC
- ES8311 audio codec
- Speaker amplifier
- 16 MB flash
- 8 MB PSRAM
- microSD slot available, currently unused by the firmware

**V1 is not part of this repository.**

### `generic-esp32s3`

Test profile for an ESP32-S3 with an external digital I2S microphone and I2S amplifier.

### `satellite1-1` – Experimental

FutureProofHomes **Satellite1.1** with ESP32-S3 N16R8 and XMOS XU316.

The profile uses the official 48 kHz I2S path and converts audio locally to the Jarvis PCM16 / 16 kHz / mono format. XMOS version detection and the direct action button are integrated.

TAS2780 / line-out, LED ring, and XMOS button handling still require additional verification on real hardware.

See [`docs/satellite1-1.md`](docs/satellite1-1.md).

## Repository Layout

```text
jarvis-satellite-esp32/
├── include/                     shared interfaces / configuration
├── src/                         hardware-independent satellite core
│   ├── core/
│   ├── network/
│   └── protocol/
├── boards/
│   ├── generic-esp32s3/
│   ├── waveshare-esp32-s3-touch-lcd-1.85c/
│   └── futureproofhomes-satellite1-1/
├── docs/
├── scripts/
├── .github/workflows/
│   ├── build.yml
│   ├── build-number.yml
│   └── create-release.yml
├── platformio.ini
├── VERSION
└── BUILD
```

Board-specific GPIO definitions, display code, touch handling, and codec logic remain under `boards/`.

The shared satellite core does not know about Waveshare-specific pins.

## Configuration

Create a local configuration file:

```bash
cp include/local_config.example.h include/local_config.h
```

Then configure Wi-Fi, Jarvis Core, and the satellite identity:

```cpp
#define JARVIS_WIFI_SSID       "MyWiFi"
#define JARVIS_WIFI_PASSWORD   "..."

#define JARVIS_CORE_HOST       "172.16.2.30"
#define JARVIS_CORE_PORT       8081
#define JARVIS_CORE_PATH       "/api/v1/voice/live"
#define JARVIS_CORE_TLS        0
#define JARVIS_CORE_TOKEN      "jv_YOUR_TOKEN"

#define JARVIS_SATELLITE_ID    "satellite-livingroom"
#define JARVIS_SATELLITE_NAME  "Living Room"
```

`include/local_config.h` is not committed to Git. Wi-Fi credentials and `JARVIS_CORE_TOKEN` therefore remain local.

The token is transmitted during the WebSocket upgrade as:

```text
Authorization: Bearer <token>
```

It is not printed to the serial console.

## Recording Configuration

The maximum recording duration can be configured per satellite:

```cpp
#define JARVIS_RECORD_MS 8000
```

For example:

```cpp
#define JARVIS_RECORD_MS 12000
```

This value is the maximum recording duration. If silence detection is enabled, a recording may end earlier.

## Optional Silence Detection

The satellite can automatically stop recording after speech has ended:

```cpp
#define JARVIS_SILENCE_DETECTION       1
#define JARVIS_SILENCE_THRESHOLD       500
#define JARVIS_SILENCE_TIMEOUT_MS      900
#define JARVIS_SILENCE_MIN_SPEECH_MS   250
#define JARVIS_SILENCE_ARM_MS          300
```

Typical flow:

```text
Recording starts
      ↓
Speech detected
      ↓
"How will the weather be?"
      ↓
~900 ms silence
      ↓
Recording ends automatically
      ↓
Audio is sent to Jarvis
```

`JARVIS_RECORD_MS` always remains the hard maximum.

## Optional Wake Word

The Waveshare ESP32-S3 can optionally use local ESP-SR / WakeNet wake-word detection.

Example:

```cpp
#define JARVIS_WAKEWORD_ENABLED 1
#define JARVIS_WAKEWORD_NAME    "Hi ESP"
```

The current test implementation uses Espressif's local **`wn9_hiesp`** WakeNet model.

The wake word is detected locally on the ESP32-S3. Microphone audio is not continuously sent to the Jarvis Core while waiting for the wake word.

Flow:

```text
Idle
  ↓
Local WakeNet
  ↓
"Hi ESP"
  ↓
Recording starts
  ↓
Question
  ↓
Silence detection / recording timeout
  ↓
Jarvis Core
  ↓
STT → Assistant → TTS
```

WakeNet is paused while STT/TTS is active and resumes after the microphone input path has been restored.

Changing only:

```cpp
#define JARVIS_WAKEWORD_NAME "Computer"
```

does **not** train a new wake word. The configured model still determines which word is recognized.

## TTS Quality

The ESP32 uses the `low` TTS quality profile by default.

It can be changed per satellite:

```cpp
#define JARVIS_TTS_QUALITY "low"
```

Supported local values:

```text
low
medium
high
```

`medium` is translated to the Core's `balanced` quality profile.

The actual TTS provider and voice are selected by the Jarvis Voice service configuration. The ESP32 does not need to know whether the server uses Piper, Kokoro, or another provider.

## Display Rotation

The Waveshare display can be rotated through `local_config.h`:

```cpp
#define JARVIS_DISPLAY_ROTATION 0
```

Supported values:

```text
0 = default
1 = 90°
2 = 180°
3 = 270°
```

Touch coordinates are rotated together with the display.

Existing touch calibration options remain available:

```cpp
#define JARVIS_WAVESHARE_TOUCH_SWAP_XY   0
#define JARVIS_WAVESHARE_TOUCH_INVERT_X  0
#define JARVIS_WAVESHARE_TOUCH_INVERT_Y  0
```

## Building

Waveshare:

```bash
pio run -e waveshare-1_85c
```

Generic ESP32-S3:

```bash
pio run -e generic-esp32s3
```

Satellite1.1:

```bash
pio run -e satellite1-1
```

## Flashing

```bash
pio run -e waveshare-1_85c -t upload
pio device monitor -b 115200
```

On Windows / PowerShell, the repository can also use the local helper script:

```powershell
.\run_waveshare.ps1
```

## Waveshare Controls

Current Waveshare controls:

- Touch `RECORD` → start recording
- Touch `RECORD` again → stop recording early
- Touch `MUTE` / `LISTEN` → toggle microphone mute state
- Touch `NET` → open network details
- Touch `+` / `-` → change speaker volume
- Physical `BOOT` button → display backlight on/off
- Physical `RESET` button → hardware reset
- Battery power slider → hardware power control

The physical BOOT button is no longer used as the normal voice-recording trigger on the Waveshare profile.

When the display is switched off, only the backlight is disabled. Wi-Fi, WebSocket connectivity, microphone handling, and TTS continue to run.

## Waveshare Display UI

The Waveshare UI uses Arduino_GFX with U8g2 font support.

The display includes:

- large clock
- current satellite state
- transcript / assistant response
- record and mute controls
- speaker volume controls
- network details popup
- Wi-Fi signal strength
- Core address
- IP, gateway, and DNS information

The display and touch rotation can be configured per device.

## Typical Serial Output

```text
Jarvis ESP32 Satellite 0.1.0 Build 3
Client: jarvis-satellite-esp32
Board: Waveshare ESP32-S3-Touch-LCD-1.85C V2

Wi-Fi connected ...
Core ready: jarvis.voice.v1

Recording started (max. 8000 ms, auto_tts=yes, silence=yes)...
Sending data to Jarvis

You: How will the weather be tomorrow?
Jarvis: Tomorrow will be ...

TTS playback ...
TTS finished.
```

With wake-word support enabled:

```text
WakeNet active: wake word 'Hi ESP'
Wake word ready: 'Hi ESP'

Wake word detected: Hi ESP
Recording started ...
```

## Voice Protocol

The satellite connects to:

```text
/api/v1/voice/live
```

using protocol:

```text
jarvis.voice.v1
```

The client can advertise capabilities such as:

```json
{
  "type": "client.capabilities",
  "client": {
    "id": "satellite-livingroom",
    "name": "Living Room",
    "platform": "esp32",
    "board": "waveshare-1_85c"
  },
  "audio": {
    "max_binary_frame_bytes": 14336,
    "preferred_tts_chunk_bytes": 12288
  },
  "features": {
    "display": true,
    "touch": true,
    "microphone": true,
    "speaker": true
  }
}
```

### Audio Uplink

The current Voice protocol sends microphone audio as PCM16LE WAV:

```text
session.start
      ↓
binary WAV message
      ↓
audio.commit
```

The Waveshare microphone path records at 16 kHz / 16-bit and converts the ES7210 stereo input to Jarvis mono audio.

### TTS Downlink

TTS is returned over the same WebSocket connection.

The satellite supports:

- PCM16 WAV
- raw PCM16LE
- mono and stereo input
- common TTS sample rates
- conversion to the internal 16 kHz mono playback path

For ESP32 clients, the Core can split TTS into small binary chunks.

The satellite can request ACK-based TTS streaming so that the Core only sends the next audio chunk after the ESP32 has successfully stored the previous chunk.

Typical flow:

```text
Core                     ESP32

tts.start
   │
   ├── binary chunk ────►
   │                ◄──── tts.ack
   │
   ├── binary chunk ────►
   │                ◄──── tts.ack
   │
   └── ...
tts.end ────────────────►
```

This prevents the ESP32 WebSocket receive path from being flooded by large TTS transfers.

## Serial Test Console

After flashing, open the serial monitor at 115200 baud:

```bash
pio device monitor -b 115200
```

Available commands:

```text
mic      local microphone / I2S test
spk      local 1 kHz speaker test
stt      STT-only test via Jarvis, auto_tts=false
tts      full STT → Jarvis → TTS → speaker roundtrip
stop     stop the current recording
mute     toggle microphone mute / listen state
status   show Wi-Fi, Core, recording, mute, and memory status
help     show command help
```

## First STT Test

Once the console shows:

```text
Core ready
```

enter:

```text
stt
```

and press ENTER.

The satellite records up to `JARVIS_RECORD_MS`. If silence detection is enabled, it may stop earlier.

`stop` can always be used to finish the recording manually.

Example:

```text
=== STT TEST ===
Speak into the microphone now.

Recording started (max. 8000 ms)...
Sending data to Jarvis

You: How will the weather be tomorrow?

------------------------------
STT result: How will the weather be tomorrow?
STT test: result received successfully
==============================
```

## Microphone Hardware Test

Before testing STT, the Waveshare audio input can be verified without using the Jarvis Core.

Enter:

```text
mic
```

The test reads PCM directly through I2S for approximately two seconds and reports sample count and level statistics.

If the result is:

```text
0 samples
```

the problem is local to I2S / ES7210 rather than the Jarvis Core.

## Speaker Hardware Test

Enter:

```text
spk
```

The firmware generates a local 1 kHz test tone through the ES8311 and speaker path.

No Core connection is required.

## Development Scope

The current firmware includes:

- board abstraction
- Wi-Fi connectivity
- authenticated Voice WebSocket
- microphone uplink
- STT integration
- TTS playback
- chunked / ACK-based TTS transport
- Waveshare display and touch UI
- U8g2 font support
- network details UI
- speaker volume controls
- display on/off control
- configurable display rotation
- configurable recording timeout
- optional silence detection
- optional local ESP-SR / WakeNet wake word
- version and build metadata

Potential future work:

- OTA firmware updates
- richer on-device settings UI
- configurable wake-word models
- additional board profiles
- full-duplex audio
- barge-in
- improved VAD / AFE integration

## Waveshare Notes

The hardware profile is designed exclusively for **V2 / Rev2.0**.

Display, touch, microphone, speaker, and codec bring-up remain isolated inside the board implementation so hardware-specific changes do not affect the Voice protocol or shared satellite core.

## Version and Build

The firmware version is stored in:

```text
VERSION
```

The continuously increasing build number is stored in:

```text
BUILD
```

Both values are injected into the firmware automatically during the PlatformIO build.

`include/build_info.h` therefore does not contain a separately maintained firmware version.

For a normal push to the default branch, `.github/workflows/build-number.yml` increments `BUILD`, commits the new number, and builds the Generic, Waveshare, and Satellite1.1 targets.

Pull requests only compile the firmware and do not modify `VERSION` or `BUILD`.

## Releases

Releases are created through:

**Actions → Create firmware release → Run workflow**

Available version bumps:

```text
patch
minor
major
custom
```

The workflow:

1. calculates the new version
2. increments `BUILD`
3. builds the firmware
4. creates the release commit
5. creates the Git tag
6. publishes the GitHub release

Example with:

```text
VERSION=0.1.0
```

```text
patch  → 0.1.1
minor  → 0.2.0
major  → 1.0.0
custom → any explicitly specified higher SemVer version
```

See [`docs/versioning.md`](docs/versioning.md).

## Automatic Hardware Release Packages

The **Create firmware release** workflow automatically builds:

```text
waveshare-1_85c
satellite1-1
```

Versioned release assets are generated for both hardware profiles.

A package contains:

```text
...-app.bin
...-factory.bin
...zip
...-SHA256SUMS.txt
```

The ZIP additionally contains:

```text
bootloader.bin
partitions.bin
manifest.json
```

and board-specific flashing instructions.

When ESP-SR wake-word support is enabled for the Waveshare profile, the required model partition / `srmodels.bin` is also handled by the PlatformIO build and upload configuration.
