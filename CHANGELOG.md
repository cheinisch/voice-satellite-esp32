# Changelog

All notable changes to `ai-voice-satellite-satellite-esp32` are documented in this file.

## 1.0.0 - 2026-08-30

First stable release of the standalone ESP32 / ESP32-S3 Ai-Voice-Satellite satellite firmware.

### Added

#### Voice and audio

- Added authenticated `ai-voice-satellite.voice.v1` WebSocket communication with Bearer token support.
- Added microphone uplink using PCM16 / 16 kHz / mono WAV audio.
- Added full STT → Assistant → TTS voice roundtrips.
- Added `AIVOICE-SATELLITE_AUTO_TTS` for normal voice interactions.
- Added binary PCM16 and PCM16-WAV TTS playback support.
- Added WAV metadata parsing for sample rate, channel count, and 16-bit PCM.
- Added sample-rate conversion to the internal 16 kHz playback format.
- Added configurable TTS quality selection through `AIVOICE-SATELLITE_TTS_QUALITY`.
- Supported local TTS quality values:
  - `low`
  - `medium`
  - `high`
- `medium` is mapped to the Core `balanced` profile.
- Added chunked TTS transport for ESP32 clients.
- Added ACK-based TTS streaming so the Core only sends the next binary audio chunk after the ESP32 confirms successful buffering of the previous chunk.
- Added diagnostics for expected, received, and missing TTS bytes.
- Added local PSRAM buffering for microphone and TTS audio.

#### Waveshare ESP32-S3-Touch-LCD-1.85C V2 / Rev2.0

- Added primary hardware support for the Waveshare ESP32-S3-Touch-LCD-1.85C V2 / Rev2.0.
- Added ST77916 360×360 QSPI display support.
- Added CST816 touch support.
- Added TCA9554 I/O expander support.
- Added ES7210 dual-microphone ADC support.
- Added ES8311 speaker codec support.
- Added NS4150B / speaker amplifier control.
- Added board-specific Waveshare audio initialization based on the official 16 kHz / 16-bit / stereo reference profile.
- Added microphone-first audio initialization so speaker bring-up can no longer block STT.
- Added lazy ES8311 initialization on first playback.
- Added automatic microphone restore after TTS playback.
- Added PA mute/unmute handling around speaker playback.
- Added boot-time I2C probes for ES8311 (`0x18`) and ES7210 (`0x40`).
- Added ES7210 register readback and extended zero-RX diagnostics.
- Added a shared 100 kHz Waveshare I2C bus matching the vendor reference implementation.

#### Display UI

- Added a full Waveshare touch UI inspired by the Ai-Voice-Satellite Command Center.
- Added a large clock display.
- Added U8g2 font support through Arduino_GFX.
- Added UTF-8 capable UI text rendering.
- Added improved proportional fonts for clock, status labels, transcript text, and assistant responses.
- Added color-coded satellite states:
  - blue
  - yellow
  - gold
  - turquoise
  - red
- Added touch controls for:
  - recording
  - stop
  - mute / listen
  - network details
  - speaker volume up/down
- Added larger invisible touch hit areas for easier operation.
- Added partial redraws for volume controls to reduce display flicker.
- Added network details popup with:
  - SSID
  - IP address
  - gateway
  - DNS
  - RSSI
  - Ai-Voice-Satellite Core address
- Added RSSI quality indication.
- Added speaker volume display.
- Added runtime speaker volume control in 10% steps.
- Added physical BOOT button handling to toggle the display backlight on/off.
- Hardware RESET remains unchanged.
- Added configurable display rotation through `AIVOICE-SATELLITE_DISPLAY_ROTATION`.
- Supported rotations:
  - `0`
  - `1` / 90°
  - `2` / 180°
  - `3` / 270°
- Touch coordinates are rotated together with the display.
- Existing touch swap/invert calibration settings remain available.

#### Recording and voice activation

- Added configurable maximum recording duration through `AIVOICE-SATELLITE_RECORD_MS`.
- Added optional local silence detection.
- Added configurable silence threshold.
- Added configurable silence timeout.
- Added minimum speech duration before silence detection can stop a recording.
- Added silence-detection arming delay.
- Added optional local ESP-SR / WakeNet wake-word support on the ESP32-S3.
- Added the Espressif `wn9_hiesp` model as the initial test wake word.
- Added `Hi ESP` as the default WakeNet test phrase.
- Wake-word detection runs locally on the device.
- Microphone audio is not continuously sent to the Core while waiting for the wake word.
- WakeNet is paused during STT/TTS activity and resumed after the microphone path has been restored.
- Added ESP-SR model partition support.
- Added automatic `srmodels.bin` flashing through PlatformIO when WakeNet is enabled.

#### Serial diagnostics and tests

- Added serial test console.
- Added `mic` command for a local microphone/I2S hardware test.
- Added `spk` command for a local 1 kHz speaker test without the Core.
- Added `stt` command for STT-only tests with `auto_tts=false`.
- Added `tts` command for complete STT → Assistant → TTS roundtrip tests.
- Added `stop` command to end a recording early.
- Added `mute` command to toggle microphone mute/listen state.
- Added `status` command for connection, recording, mute, memory, and wake-word status.
- Added `help` command.
- Added microphone level diagnostics including:
  - sample count
  - minimum/maximum level
  - mean absolute level
  - non-zero sample ratio
- Added detailed hardware initialization logs.
- Network and protocol diagnostics remain available even when board initialization is incomplete.

#### Generic ESP32-S3

- Added a generic ESP32-S3 hardware profile.
- Added support for an external digital I2S microphone.
- Added support for an external I2S amplifier.
- Added BOOT-button recording control for the generic profile.

#### FutureProofHomes Satellite1.1

- Added experimental `satellite1-1` hardware profile.
- Added ESP32-S3 N16R8 support.
- Added XMOS XU316 SPI version probing.
- Added Satellite1.1 48 kHz I2S audio path.
- Added local conversion to Ai-Voice-Satellite PCM16 / 16 kHz / mono.
- Added direct action-button support.
- TAS2780 / line-out, LED ring, and XMOS button handling remain experimental and require further validation on real hardware.

#### Build and release automation

- `VERSION` and `BUILD` are the single source of truth for firmware version and build number.
- PlatformIO injects both values automatically through `scripts/platformio_version.py`.
- GitHub Actions increments `BUILD` for successful pushes to the default branch.
- Pull requests compile firmware without modifying `VERSION` or `BUILD`.
- Added race protection so outdated build commits are not pushed when a newer commit arrives during CI.
- Added manual `Create firmware release` workflow.
- Release workflow supports:
  - `patch`
  - `minor`
  - `major`
  - explicit custom SemVer
- Release workflow:
  1. calculates the new firmware version
  2. increments `BUILD`
  3. builds all supported targets
  4. creates the release commit
  5. creates the Git tag
  6. publishes the GitHub release
- Added automatic release packages for Waveshare and Satellite1.1.
- Release assets include:
  - application binary
  - merged factory image
  - ZIP package
  - manifest
  - SHA-256 checksums
- Waveshare ESP-SR builds can include the required model image in the flash layout.

### Changed

- Adapted the ESP32 Voice uplink to the production `ai-voice-satellite.voice.v1` contract already used by the Linux / ReSpeaker satellites:
  - `session.start`
  - binary WAV message
  - `audio.commit`
- Push-to-talk recordings are buffered locally before upload.
- The Voice client now handles native Core events including:
  - `session.started`
  - `transcript.partial`
  - `transcript.final`
  - `assistant.final`
  - `reset`
- TTS can now be streamed in small binary chunks instead of requiring one large WebSocket frame.
- ESP32 clients advertise preferred TTS chunk sizes and maximum binary frame sizes.
- Client capability metadata can be sent to the Core for improved satellite identification.
- Waveshare BOOT button behavior changed from voice trigger to display backlight toggle.
- Waveshare voice recording is now primarily controlled through the touch UI, silence detection, or WakeNet.
- Display volume controls were moved to one side and given larger touch hitboxes.
- Speaker volume is applied at runtime instead of being UI-only.
- Waveshare display updates use partial redraws where possible to reduce visible flicker.
- The Waveshare microphone path is initialized before and independently from the speaker output.
- ES8311 is initialized only when playback is required.
- WakeNet and the normal Ai-Voice-Satellite recording path share the microphone safely by pausing WakeNet before STT recording.
- The Generic, Waveshare, and Satellite1.1 PlatformIO environments are isolated from one another.

### Fixed

- Fixed Waveshare I2S setup to match the official V2 Arduino reference.
- Fixed forced I2S1 usage by switching to the correct automatic I2S port configuration.
- Added explicit I2S stream timeouts.
- Reduced repeated zero-byte RX diagnostics.
- Fixed the Waveshare shared-I2C runtime issue by making the board own the `Wire` bus exclusively.
- ES8311 and ES7210 now reuse the initialized shared I2C bus instead of reinitializing it.
- Fixed `arduino-audio-driver` v0.2.0 shared-Wire registration while preserving codec default addresses.
- Fixed Waveshare audio support for the `DriverPins` API and updated ES8311/ES7210 constructors.
- Fixed CST816 polling to use the active-low interrupt instead of continuously reading I2C.
- Fixed `sendTXT` usage for arduinoWebSockets 2.7.x while keeping `sendJson(const String&)` const-correct.
- Fixed Generic ESP32-S3 builds accidentally compiling the Waveshare board library.
- Fixed Waveshare builds accidentally compiling the Generic board library.
- Fixed shared headers under `include/` not being available to board libraries.
- Fixed ST77916 initialization for the Waveshare V2 panel.
- Fixed TCA9554 reset-pin mapping for LCD and touch.
- Fixed large incoming WebSocket TTS frame handling during bring-up.
- Replaced large single-frame TTS transfers with chunked / ACK-based streaming for ESP32 clients.
- Fixed microphone initialization after speaker playback by restoring RX I2S and reinitializing ES7210.
- Fixed repeated `ready` event handling.
- Added normal handling for Core `reset` session events.
- Disabled client `hello` by default for Core versions that already send `ready` and reject unknown `hello` messages.
- Fixed U8g2 dependency resolution by using the official `U8g2_Arduino` source.
- Fixed touch coordinate handling when the display is rotated.

### Notes

- The Waveshare hardware profile supports **V2 / Rev2.0 only**.
- Waveshare V1 is intentionally not supported.
- Linux satellites are developed in a separate repository.
- Wake-word support currently uses Espressif's `Hi ESP` model as the initial local test model.
- Custom wake words require a compatible trained wake-word model.

## 0.1.0 Build 4

### Added

- Added FutureProofHomes Satellite1.1 as the third ESP32-S3 board profile (`satellite1-1`).
- Added XMOS SPI version probing.
- Added the initial Satellite1.1 48 kHz I2S audio path.
- Added action-button preparation.
- CI now builds Generic, Waveshare, and Satellite1.1.
- Release workflow now generates Satellite1.1 firmware packages.

### Fixed

- Shared headers under `include/` are explicitly available while compiling board libraries.
- The push workflow commits `BUILD` only after successful Generic and Waveshare firmware builds.
- Branch race protection prevents stale build commits when a newer push arrives during the build.

## 0.1.0 - Initial ESP32 Repository

### Added

- Created the standalone `ai-voice-satellite-satellite-esp32` repository.
- Kept Linux satellite components completely separate.
- Added a shared ESP32-S3 Voice Satellite core.
- Added board abstraction through PlatformIO libraries under `boards/`.
- Added a generic ESP32-S3 profile for external I2S microphone and speaker hardware.
- Added the first display board profile for Waveshare ESP32-S3-Touch-LCD-1.85C **V2 / Rev2.0**.
- Explicitly excluded Waveshare V1.
- Added `ai-voice-satellite.voice.v1` WebSocket protocol support.
- Added PCM16 / 16 kHz / mono audio uplink to the Core.
- Added binary PCM16 TTS downlink.
- Added firmware version and build metadata to the client.
- Prepared Waveshare support for:
  - ST77916
  - CST816
  - TCA9554
  - ES8311
  - ES7210
  - touch trigger
  - status UI

### Build and Release Automation

- Added root `VERSION` and `BUILD` files.
- PlatformIO reads version/build information through `scripts/platformio_version.py`.
- GitHub Actions increments `BUILD` automatically for normal pushes.
- Pull requests do not modify build metadata.
- Added manual firmware release workflow with patch/minor/major/custom SemVer support.
- Release workflow updates `VERSION` and `BUILD`, builds before publishing, creates the release commit and tag, and publishes the GitHub release.
- Waveshare release packages include application binary, merged factory binary, ZIP archive, manifest, and SHA-256 checksums.
