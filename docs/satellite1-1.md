# FutureProofHomes Satellite1.1

`futureproofhomes-satellite1-1` ist das dritte Boardprofil in `ai-voice-satellite-satellite-esp32`.

## Warum ein eigenes Profil?

Satellite1.1 verwendet einen ESP32-S3 als Netzwerk-/Steuercontroller, aber die Mikrofone hängen nicht direkt am ESP32. Ein XMOS XU316 übernimmt das Audio-Frontend. Die offizielle Konfiguration verwendet 48 kHz, 32 Bit und Stereo. Ai-Voice-Satellite transportiert derzeit 16 kHz PCM16 Mono. Die Formatwandlung bleibt deshalb im Boardtreiber.

## Relevante Pins

| Funktion | GPIO |
|---|---:|
| Action Button | 0 |
| XMOS Reset | 4 |
| I2C SDA | 5 |
| I2C SCL | 6 |
| I2S LRCLK | 7 |
| I2S BCLK | 8 |
| I2S Speaker DOUT | 9 |
| XMOS SPI CS | 10 |
| SPI MOSI | 11 |
| SPI CLK | 12 |
| SPI MISO | 13 |
| I2S Mic DIN | 15 |
| I2S MCLK | 16 |
| Status LED | 45 |

## Build

```bash
pio run -e satellite1-1
```

## Flash

```bash
pio run -e satellite1-1 -t upload
pio device monitor -b 115200
```

## XMOS

Der Ai-Voice-Satellite-Treiber prüft beim Start die XMOS-Firmwareversion über SPI. In der ersten Integration wird **keine XMOS-Firmware mit Ai-Voice-Satellite ausgeliefert oder automatisch geflasht**. Dadurch bleibt der ESP32-Release unabhängig von der separaten FutureProofHomes-XMOS-Firmware.

## Audio

Uplink:

```text
XMOS -> I2S 48 kHz / Stereo / 32 bit
      -> Board Driver
      -> PCM16 16 kHz Mono
      -> ai-voice-satellite.voice.v1
```

Downlink:

```text
Ai-Voice-Satellite PCM16 16 kHz Mono
      -> Board Driver
      -> I2S 48 kHz / Stereo / 32 bit
      -> Satellite1.1 Audio Path
```

Der TAS2780-Lautsprecherverstärker und PCM5122-Line-Out werden in diesem ersten Profil noch als experimentell geführt und müssen auf echter Satellite1.1-Hardware verifiziert werden.
