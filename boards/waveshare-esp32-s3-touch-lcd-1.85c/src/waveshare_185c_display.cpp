#include "waveshare_185c_display.h"
#include "waveshare_185c_pins.h"
#include <Arduino_GFX_Library.h>

namespace {
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t RED = 0xF800;
constexpr uint16_t BLUE = 0x001F;
}

bool Waveshare185CDisplay::begin(Waveshare185CExpander& expander) {
    pinMode(waveshare185c::LCD_BL, OUTPUT);
    digitalWrite(waveshare185c::LCD_BL, LOW);

    if (!expander.pinModeOutput(waveshare185c::EXIO_LCD_RST, true)) return false;
    expander.write(waveshare185c::EXIO_LCD_RST, false);
    delay(20);
    expander.write(waveshare185c::EXIO_LCD_RST, true);
    delay(120);

    bus_ = new Arduino_ESP32QSPI(
        waveshare185c::LCD_CS,
        waveshare185c::LCD_SCK,
        waveshare185c::LCD_D0,
        waveshare185c::LCD_D1,
        waveshare185c::LCD_D2,
        waveshare185c::LCD_D3,
        false);
    gfx_ = new Arduino_ST77916(bus_, -1, 0, true, 360, 360);
    if (!gfx_ || !gfx_->begin()) return false;

    gfx_->fillScreen(BLACK);
    digitalWrite(waveshare185c::LCD_BL, HIGH);
    ready_ = true;
    showState(SatelliteState::Booting, "Jarvis Satellite");
    return true;
}

String Waveshare185CDisplay::compact(const String& text, size_t maxChars) const {
    if (text.length() <= maxChars) return text;
    return text.substring(0, maxChars > 3 ? maxChars - 3 : maxChars) + "...";
}

void Waveshare185CDisplay::header(const char* title) {
    if (!ready_) return;
    gfx_->fillScreen(BLACK);
    gfx_->setTextColor(WHITE);
    gfx_->setTextSize(2);
    gfx_->setCursor(138, 38);
    gfx_->print(title);
    gfx_->drawCircle(180, 180, 150, 0x4208);
}

void Waveshare185CDisplay::centered(const String& text, int y, uint16_t color, uint8_t size) {
    if (!ready_) return;
    const String line = compact(text, size == 1 ? 42 : 26);
    gfx_->setTextColor(color);
    gfx_->setTextSize(size);
    const int charWidth = 6 * size;
    int x = (360 - static_cast<int>(line.length()) * charWidth) / 2;
    if (x < 24) x = 24;
    gfx_->setCursor(x, y);
    gfx_->print(line);
}

void Waveshare185CDisplay::showState(SatelliteState state, const String& detail) {
    if (!ready_) return;
    header("JARVIS");
    String label;
    uint16_t color = WHITE;
    switch (state) {
        case SatelliteState::Booting: label = "Start"; color = CYAN; break;
        case SatelliteState::ConnectingWifi: label = "WLAN"; color = YELLOW; break;
        case SatelliteState::ConnectingCore: label = "Core"; color = YELLOW; break;
        case SatelliteState::Ready: label = "Bereit"; color = GREEN; break;
        case SatelliteState::Listening: label = "Hoert zu"; color = CYAN; break;
        case SatelliteState::Processing: label = "Verarbeite"; color = YELLOW; break;
        case SatelliteState::Speaking: label = "Antwort"; color = BLUE; break;
        case SatelliteState::Error: label = "Fehler"; color = RED; break;
    }
    gfx_->fillCircle(180, 145, 18, color);
    centered(label, 190, color, 3);
    centered(detail, 245, WHITE, 1);
}

void Waveshare185CDisplay::showTranscript(const String& text) {
    if (!ready_) return;
    header("DU");
    centered(compact(text, 48), 165, CYAN, 2);
    centered("Sende an Jarvis", 245, WHITE, 1);
}

void Waveshare185CDisplay::showAssistant(const String& text) {
    if (!ready_) return;
    header("JARVIS");
    centered(compact(text, 48), 155, WHITE, 2);
    centered("Antwort", 245, BLUE, 1);
}
