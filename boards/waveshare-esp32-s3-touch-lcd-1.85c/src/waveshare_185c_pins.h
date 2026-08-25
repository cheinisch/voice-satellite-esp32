#pragma once

namespace waveshare185c {
constexpr int I2C_SCL = 10;
constexpr int I2C_SDA = 11;
constexpr uint8_t TCA9554_ADDR = 0x20;
constexpr uint8_t CST816_ADDR = 0x15;
constexpr uint8_t ES8311_ADDR = 0x18;
constexpr uint8_t ES7210_ADDR = 0x40;

constexpr int I2S_MCLK = 2;
constexpr int I2S_BCLK = 48;
constexpr int I2S_LRCK = 38;
constexpr int I2S_DOUT = 47;
constexpr int I2S_DIN = 39;
constexpr int PA_CTRL = 15;

constexpr int LCD_CS = 21;
constexpr int LCD_SCK = 40;
constexpr int LCD_D0 = 46;
constexpr int LCD_D1 = 45;
constexpr int LCD_D2 = 42;
constexpr int LCD_D3 = 41;
constexpr int LCD_TE = 18;
constexpr int LCD_BL = 5;

constexpr int TOUCH_INT = 4;
constexpr int BOOT_BUTTON = 0;

constexpr uint8_t EXIO_TOUCH_RST = 1;
constexpr uint8_t EXIO_LCD_RST = 2;
}
