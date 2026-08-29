#include "waveshare_185c_display.h"
#include "waveshare_185c_pins.h"
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Colour palette
// ---------------------------------------------------------------------------
namespace {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t BG        = rgb565(  4,  12,  22);
constexpr uint16_t PANEL_2   = rgb565( 12,  30,  48);
constexpr uint16_t BORDER    = rgb565( 22,  52,  72);
constexpr uint16_t TEXT      = rgb565(220, 238, 248);
constexpr uint16_t MUTED_TXT = rgb565(100, 130, 150);
constexpr uint16_t CYAN      = rgb565( 32, 210, 210);
constexpr uint16_t GOLD      = rgb565(218, 165,  48);
constexpr uint16_t BLUE      = rgb565( 48, 110, 230);
constexpr uint16_t RED       = rgb565(220,  55,  55);

constexpr int CX      = 180;
constexpr int CY      = 183;
constexpr int R1      = 62;
constexpr int R2      = 70;
constexpr int R3      = 73;
constexpr int DOT_COLS = 7;
constexpr int DOT_ROWS = 7;
constexpr int DOT_GAP  = 16;

} // namespace

// ---------------------------------------------------------------------------
// Pixel-accurate centering for the built-in bitmap font
// charW = 6*size, charH = 8*size
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::centered(const String& text, int y,
                                    uint16_t color, uint8_t size) {
    if (!ready_) return;
    const int charW = 6 * size;
    int x = (360 - static_cast<int>(text.length()) * charW) / 2;
    if (x < 4) x = 4;
    gfx_->setTextColor(color);
    gfx_->setTextSize(size);
    gfx_->setCursor(x, y);
    gfx_->print(text);
}

void Waveshare185CDisplay::lineText(const String& text, int x, int y,
                                    int maxChars, uint16_t color, uint8_t size) {
    gfx_->setTextColor(color);
    gfx_->setTextSize(size);
    gfx_->setCursor(x, y);
    gfx_->print(compact(text, static_cast<size_t>(maxChars)));
}

void Waveshare185CDisplay::wrappedText(const String& source, int x, int y,
                                       int widthChars, int maxLines,
                                       uint16_t color, uint8_t size) {
    String text = source;
    text.replace("\n", " ");
    text.trim();
    const int lineH = 9 * size + 2;
    for (int line = 0; line < maxLines && text.length(); ++line) {
        int take = min(static_cast<int>(text.length()), widthChars);
        if (take < static_cast<int>(text.length())) {
            int split = text.lastIndexOf(' ', take);
            if (split > widthChars / 2) take = split;
        }
        String part = text.substring(0, take);
        part.trim();
        if (line == maxLines - 1 && take < static_cast<int>(text.length()))
            part = compact(part, widthChars - 3) + "...";
        lineText(part, x, y + line * lineH, widthChars, color, size);
        text.remove(0, take);
        text.trim();
    }
}

String Waveshare185CDisplay::compact(const String& text, size_t maxChars) const {
    if (text.length() <= maxChars) return text;
    return text.substring(0, maxChars > 3 ? maxChars - 3 : maxChars) + "...";
}

// ---------------------------------------------------------------------------
// State helpers
// ---------------------------------------------------------------------------

uint16_t Waveshare185CDisplay::stateAccent() const {
    switch (state_) {
        case SatelliteState::Booting:          return CYAN;
        case SatelliteState::ConnectingWifi:
        case SatelliteState::ConnectingCore:   return GOLD;
        case SatelliteState::Ready:            return GOLD;
        case SatelliteState::Listening:        return CYAN;
        case SatelliteState::Processing:       return CYAN;
        case SatelliteState::Speaking:         return BLUE;
        case SatelliteState::Muted:
        case SatelliteState::Error:            return RED;
    }
    return GOLD;
}

String Waveshare185CDisplay::stateLabel() const {
    String raw;
    switch (state_) {
        case SatelliteState::Booting:          raw = "BOOT"; break;
        case SatelliteState::ConnectingWifi:   raw = "WLAN"; break;
        case SatelliteState::ConnectingCore:   raw = "CORE"; break;
        case SatelliteState::Ready:            raw = "STANDBY"; break;
        case SatelliteState::Listening:        raw = "LISTEN"; break;
        case SatelliteState::Processing:       raw = "PROCESS"; break;
        case SatelliteState::Speaking:         raw = "SPEAKING"; break;
        case SatelliteState::Muted:            raw = "MUTED"; break;
        case SatelliteState::Error:            raw = "ERROR"; break;
    }
    String spaced;
    for (size_t i = 0; i < raw.length(); ++i) {
        if (i) spaced += ' ';
        spaced += raw[i];
    }
    return spaced;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

bool Waveshare185CDisplay::begin(Waveshare185CExpander& expander) {
    pinMode(waveshare185c::LCD_BL, OUTPUT);
    digitalWrite(waveshare185c::LCD_BL, LOW);

    if (!expander.pinModeOutput(waveshare185c::EXIO_LCD_RST, true)) {
        Serial.println("Display: TCA9554 LCD-Reset Ausgang fehlgeschlagen.");
        return false;
    }
    expander.write(waveshare185c::EXIO_LCD_RST, false);
    delay(10);
    expander.write(waveshare185c::EXIO_LCD_RST, true);
    delay(50);

    bus_ = new Arduino_ESP32QSPI(
        waveshare185c::LCD_CS, waveshare185c::LCD_SCK,
        waveshare185c::LCD_D0, waveshare185c::LCD_D1,
        waveshare185c::LCD_D2, waveshare185c::LCD_D3,
        false);

    gfx_ = new Arduino_ST77916(
        bus_, -1, 0, true, 360, 360, 0, 0, 0, 0,
        st77916_150_init_operations, sizeof(st77916_150_init_operations));

    if (!gfx_ || !gfx_->begin(80000000)) {
        Serial.println("Display: ST77916/QSPI begin fehlgeschlagen.");
        return false;
    }

    Serial.println("Display: ST77916/QSPI bereit (360x360, 80 MHz).");
    digitalWrite(waveshare185c::LCD_BL, HIGH);
    delay(20);

    ready_ = true;
    state_ = SatelliteState::Booting;
    detail_ = "";
    renderDashboard();
    Serial.println("Display: Jarvis Command Center UI gerendert.");
    return true;
}

void Waveshare185CDisplay::loop() {
    if (!ready_) return;
    renderClock(false);
}

// ---------------------------------------------------------------------------
// Clock — pixel-centred with size-3 bitmap font (18×8 px per char)
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::renderClock(bool force) {
    if (!ready_) return;
    if (!force && millis() - lastClockAt_ < 1000) return;
    lastClockAt_ = millis();

    struct tm nowInfo{};
    time_t now = time(nullptr);
    const bool synced = now > 1700000000 && localtime_r(&now, &nowInfo) != nullptr;
    const int minute = synced ? nowInfo.tm_min : -1;
    if (!force && minute == lastMinute_) return;
    lastMinute_ = minute;

    char buf[8] = "--:--";
    if (synced) snprintf(buf, sizeof(buf), "%02d:%02d", nowInfo.tm_hour, nowInfo.tm_min);

    // Clear top band
    gfx_->fillRect(0, 8, 360, 46, BG);

    // Size-3 clock: each char is 18 px wide, 24 px tall. "HH:MM" = 5 chars = 90 px.
    centered(String(buf), 14, TEXT, 3);

    // "JARVIS CORE" in cyan below, size 1
    centered("JARVIS CORE", 42, MUTED_TXT, 1);
}

// ---------------------------------------------------------------------------
// Dot-grid core
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::renderDotGrid(uint16_t accent) {
    gfx_->fillCircle(CX, CY, R1, PANEL_2);

    const int startX = CX - (DOT_COLS / 2) * DOT_GAP;
    const int startY = CY - (DOT_ROWS / 2) * DOT_GAP;

    for (int row = 0; row < DOT_ROWS; ++row) {
        for (int col = 0; col < DOT_COLS; ++col) {
            const int dx = startX + col * DOT_GAP - CX;
            const int dy = startY + row * DOT_GAP - CY;
            const int dist2 = dx * dx + dy * dy;
            if (dist2 > (R1 - 8) * (R1 - 8)) continue;
            uint16_t dotColor;
            if      (dist2 < 20 * 20) dotColor = rgb565(80, 160, 200);
            else if (dist2 < 38 * 38) dotColor = rgb565(40, 100, 140);
            else                       dotColor = rgb565(22,  58,  82);
            gfx_->fillCircle(startX + col * DOT_GAP,
                              startY + row * DOT_GAP, 2, dotColor);
        }
    }

    for (int r = R1 + 1; r < R2; ++r)
        gfx_->drawCircle(CX, CY, r, rgb565(10, 50, 60));

    gfx_->drawCircle(CX, CY, R2,     accent);
    gfx_->drawCircle(CX, CY, R2 + 1, accent);
    gfx_->drawCircle(CX, CY, R2 + 2, rgb565(160, 110, 22));
}

// ---------------------------------------------------------------------------
// Status labels below ring
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::renderStatusLabels(uint16_t accent) {
    const int baseY = CY + R2 + 10;
    centered("JARVIS CORE", baseY,      MUTED_TXT, 1);
    centered(stateLabel(),  baseY + 14, accent,    1);
    if (detail_.length()) {
        gfx_->fillRect(60, baseY + 26, 240, 10, BG);
        centered(compact(detail_, 38), baseY + 26, MUTED_TXT, 1);
    }
}

// ---------------------------------------------------------------------------
// Dashboard
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::renderDashboard() {
    if (!ready_) return;
    gfx_->fillScreen(BG);

    const uint16_t accent = stateAccent();

    // Corner brackets
    gfx_->drawLine( 44,  6,  84,  6, BORDER); gfx_->drawLine( 44,  6,  44, 18, BORDER);
    gfx_->drawLine(276,  6, 316,  6, BORDER); gfx_->drawLine(316,  6, 316, 18, BORDER);
    gfx_->drawLine( 44, 354,  44, 342, BORDER); gfx_->drawLine( 44, 354,  84, 354, BORDER);
    gfx_->drawLine(316, 354, 316, 342, BORDER); gfx_->drawLine(316, 354, 276, 354, BORDER);

    renderClock(true);
    renderDotGrid(accent);
    renderStatusLabels(accent);
}

void Waveshare185CDisplay::showState(SatelliteState state, const String& detail) {
    if (!ready_) return;
    state_ = state;
    detail_ = detail;
    renderDashboard();
}

// ---------------------------------------------------------------------------
// Bubble — width adapts to round display via chord formula
// radius = 180, centre at (180, 180)
// ---------------------------------------------------------------------------

static int chordHalfWidth(int y) {
    const int dy = y - 180;
    const int r2 = 180 * 180 - dy * dy;
    return (r2 > 0) ? static_cast<int>(sqrtf(static_cast<float>(r2))) : 0;
}

void Waveshare185CDisplay::showTranscript(const String& text) {
    if (!ready_) return;
    state_ = SatelliteState::Processing;
    detail_ = "";
    renderDashboard();

    // Find narrowest chord over the bubble rows (y 286..348)
    int minHalf = 180;
    for (int y = 286; y <= 348; y += 4)
        minHalf = min(minHalf, chordHalfWidth(y));

    const int margin = 18;
    const int bw  = (minHalf - margin) * 2;
    const int bx  = 180 - (minHalf - margin);
    const int by  = 288;
    const int bh  = 58;

    gfx_->fillRoundRect(bx, by, bw, bh, 10, PANEL_2);
    gfx_->drawRoundRect(bx, by, bw, bh, 10, CYAN);

    // Badge label "DU"
    gfx_->fillRect(bx + 10, by - 6, 22, 12, PANEL_2);
    gfx_->setTextColor(CYAN); gfx_->setTextSize(1);
    gfx_->setCursor(bx + 13, by - 4); gfx_->print("DU");

    const int textX     = bx + 10;
    const int textChars = max(1, (bw - 20) / 6);  // 6 px per char at size 1
    wrappedText(text, textX, by + 12, textChars, 3, TEXT, 1);
}

void Waveshare185CDisplay::showAssistant(const String& text) {
    if (!ready_) return;
    state_ = SatelliteState::Speaking;
    detail_ = "";
    renderDashboard();

    int minHalf = 180;
    for (int y = 286; y <= 348; y += 4)
        minHalf = min(minHalf, chordHalfWidth(y));

    const int margin = 18;
    const int bw  = (minHalf - margin) * 2;
    const int bx  = 180 - (minHalf - margin);
    const int by  = 288;
    const int bh  = 58;

    gfx_->fillRoundRect(bx, by, bw, bh, 10, PANEL_2);
    gfx_->drawRoundRect(bx, by, bw, bh, 10, GOLD);

    // Badge label "JARVIS"
    gfx_->fillRect(bx + 10, by - 6, 46, 12, PANEL_2);
    gfx_->setTextColor(GOLD); gfx_->setTextSize(1);
    gfx_->setCursor(bx + 13, by - 4); gfx_->print("JARVIS");

    const int textX     = bx + 10;
    const int textChars = max(1, (bw - 20) / 6);
    wrappedText(text, textX, by + 12, textChars, 3, TEXT, 1);
}

// ---------------------------------------------------------------------------
// Touch
// ---------------------------------------------------------------------------

bool Waveshare185CDisplay::hitRecordButton(uint16_t x, uint16_t y) const {
    return y >= 280 && x < 180;
}

bool Waveshare185CDisplay::hitMuteButton(uint16_t x, uint16_t y) const {
    return y >= 280 && x >= 180;
}