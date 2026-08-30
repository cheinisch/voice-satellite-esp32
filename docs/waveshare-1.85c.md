# Waveshare ESP32-S3-Touch-LCD-1.85C V2

Im Projekt wird ausschließlich die V2/Rev2.0 unterstützt.

Die Implementierung ist in einzelne Hardwarebereiche aufgeteilt:

```text
waveshare_185c_board.*      Integration
waveshare_185c_audio.*      ES7210 + ES8311 + I2S
waveshare_185c_display.*    ST77916
waveshare_185c_touch.*      CST816
waveshare_185c_expander.*   TCA9554
waveshare_185c_pins.h       Pin-Mapping
```

Dadurch kann z. B. ein Display-Bring-up-Fix vorgenommen werden, ohne Audio oder Voice-Protokoll anzufassen.


## Shared I2C bus

The V2 board uses one shared I2C bus on GPIO11 (SDA) / GPIO10 (SCL).
`Waveshare185CBoard` initializes `Wire` exactly once. The TCA9554, CST816,
ES8311 and ES7210 drivers reuse this existing bus. Board drivers must not call
`Wire.begin()` or `Wire.end()` independently.

For the audio-driver integration, use the already-initialized-Wire overload
with `setActive=false`; this preserves the codecs' default 7-bit addresses
(ES8311 `0x18`, ES7210 `0x40`).

## ES7210 microphone bring-up

The V2 microphone path uses a Waveshare-specific ES7210 register profile: 16 kHz, 16-bit stereo I2S, MCLK ratio 256, 2.87 V microphone bias and 36 dB analog gain. This intentionally follows the vendor V2 speech-recognition example rather than the generic ES7210 wrapper.

The serial console prints I2C probes for ES8311 at `0x18` and ES7210 at `0x40`. Use `mic` to run a two-second local microphone test before testing STT.

## Audio bring-up / diagnostics

The microphone path (ES7210 + ESP32-S3 I2S RX) is a required part of board initialization.
The ES8311 speaker is initialized lazily on the first TTS frame so a speaker problem cannot prevent STT tests.
On boot the serial console prints each initialization stage and the `mic` command tests the local microphone path without Ai-Voice-Satellite/STT.
