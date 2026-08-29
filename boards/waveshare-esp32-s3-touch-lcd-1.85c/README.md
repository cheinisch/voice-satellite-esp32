# Waveshare ESP32-S3-Touch-LCD-1.85C

Unterstütztes Zielgerät dieses Repositories:

- **nur V2 / Rev2.0**
- 360 × 360 ST77916 QSPI LCD
- CST816 Touch
- TCA9554 GPIO-Expander
- ES8311 Audio-Decoder / Lautsprecherpfad
- ES7210 Audio-ADC / Dual-Mikrofonpfad
- 16 MB Flash / 8 MB PSRAM
- BOOT-Taste zusätzlich zum Touch als Sprachtrigger

V1 ist absichtlich nicht enthalten, da sie im Projekt nicht getestet werden kann.

## Pinbelegung V2

| Funktion | GPIO |
|---|---:|
| I2C SCL | 10 |
| I2C SDA | 11 |
| I2S MCLK | 2 |
| I2S BCLK | 48 |
| I2S LRCK | 38 |
| I2S Speaker DOUT | 47 |
| I2S Mic DIN | 39 |
| PA CTRL | 15 |
| LCD CS | 21 |
| LCD SCK | 40 |
| LCD D0..D3 | 46,45,42,41 |
| LCD BL | 5 |
| LCD TE | 18 |
| Touch INT | 4 |
| LCD Reset | TCA9554 EXIO2 |
| Touch Reset | TCA9554 EXIO1 |

Der erste Bring-up nutzt Arduino_GFX für den ST77916. Falls Waveshare für eine konkrete Fertigungscharge zusätzliche Vendor-Init-Sequenzen benötigt, ist diese Logik ausschließlich in `waveshare_185c_display.cpp` anzupassen; Voice/Core-Code bleibt unverändert.

## Display-Schriften

Die Waveshare-Oberfläche nutzt U8g2-Fonts über die native U8g2-Unterstützung von Arduino_GFX.
PlatformIO zieht `olikraus/U8g2` über die Board-Library-Abhängigkeit automatisch ein.

Aktuell werden verwendet:

- `u8g2_font_logisoso38_tr` für die Uhr
- `u8g2_font_helvB10_tf` für Statusbeschriftungen
- `u8g2_font_helvR08_tf` für sekundäre Beschriftungen und Dialogtext
- `u8g2_font_helvB08_tf` für kleine Badges

UTF-8-Ausgabe ist aktiviert, damit deutschsprachige Texte inklusive Umlauten sauber dargestellt werden können.
