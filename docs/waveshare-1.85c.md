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
