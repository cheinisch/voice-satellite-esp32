#include "waveshare_185c_display.h"
#include "waveshare_185c_pins.h"
#include <U8g2lib.h>
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
// U8g2 text helpers
// Arduino_GFX exposes U8g2 font support when U8g2lib.h is included first.
// Coordinates use a top edge so the dashboard layout stays predictable even
// though U8g2 fonts are baseline-oriented internally.
// ---------------------------------------------------------------------------

int Waveshare185CDisplay::textWidth(const String& text, const uint8_t* font) {
    if (!ready_ || !font) return 0;
    gfx_->setFont(font);
    gfx_->setTextSize(1);
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    gfx_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return static_cast<int>(w);
}

void Waveshare185CDisplay::fontText(const String& text, int x, int topY,
                                    uint16_t color, const uint8_t* font) {
    if (!ready_ || !font) return;
    gfx_->setFont(font);
    gfx_->setTextSize(1);
    gfx_->setTextColor(color);

    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    gfx_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor(x - x1, topY - y1);
    gfx_->print(text);
}

void Waveshare185CDisplay::centeredFont(const String& text, int topY,
                                        uint16_t color, const uint8_t* font) {
    if (!ready_ || !font) return;
    gfx_->setFont(font);
    gfx_->setTextSize(1);
    gfx_->setTextColor(color);

    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    gfx_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    const int x = (360 - static_cast<int>(w)) / 2 - x1;
    gfx_->setCursor(max(2, x), topY - y1);
    gfx_->print(text);
}

void Waveshare185CDisplay::wrappedFontText(const String& source, int x, int topY,
                                           int maxWidth, int maxLines,
                                           uint16_t color, const uint8_t* font,
                                           int lineHeight) {
    if (!ready_ || !font || maxWidth <= 0 || maxLines <= 0) return;

    String remaining = source;
    remaining.replace("\n", " ");
    remaining.trim();

    for (int line = 0; line < maxLines && remaining.length(); ++line) {
        String current;
        int consumed = 0;

        while (consumed < static_cast<int>(remaining.length())) {
            int nextSpace = remaining.indexOf(' ', consumed);
            const int wordEnd = nextSpace < 0 ? static_cast<int>(remaining.length()) : nextSpace;
            const String word = remaining.substring(consumed, wordEnd);
            const String candidate = current.length() ? current + " " + word : word;

            if (current.length() && textWidth(candidate, font) > maxWidth) break;

            current = candidate;
            consumed = wordEnd;
            while (consumed < static_cast<int>(remaining.length()) && remaining[consumed] == ' ') ++consumed;

            if (textWidth(current, font) > maxWidth) {
                // Very long words are clipped by shrinking byte-wise. This is
                // only a safety path; normal German/English words wrap above.
                while (current.length() > 1 && textWidth(current + "...", font) > maxWidth)
                    current.remove(current.length() - 1);
                current += "...";
                consumed = max(consumed, 1);
                break;
            }
        }

        if (!current.length()) break;

        const bool more = consumed < static_cast<int>(remaining.length());
        if (line == maxLines - 1 && more) {
            while (current.length() > 1 && textWidth(current + "...", font) > maxWidth)
                current.remove(current.length() - 1);
            current += "...";
        }

        fontText(current, x, topY + line * lineHeight, color, font);
        if (!more || line == maxLines - 1) break;
        remaining.remove(0, consumed);
        remaining.trim();
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

    gfx_->setUTF8Print(true);
    gfx_->setTextWrap(false);
    Serial.println("Display: U8g2-Fonts + UTF-8 aktiviert.");
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
// Clock — larger U8g2 Logisoso face, slightly lower than the old bitmap clock
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

    // The clock intentionally sits a little lower than before. Logisoso 38 is
    // visibly larger and smoother than the old 3x bitmap font while leaving
    // enough air above the central status ring.
    gfx_->fillRect(0, 8, 360, 72, BG);
    centeredFont(String(buf), 20, TEXT, u8g2_font_logisoso38_tr);
    centeredFont("JARVIS CORE", 65, MUTED_TXT, u8g2_font_helvR08_tf);
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
    const int baseY = CY + R2 + 9;
    centeredFont("JARVIS CORE", baseY, MUTED_TXT, u8g2_font_helvR08_tf);
    centeredFont(stateLabel(), baseY + 14, accent, u8g2_font_helvB10_tf);
    if (detail_.length()) {
        gfx_->fillRect(48, baseY + 30, 264, 14, BG);
        centeredFont(compact(detail_, 36), baseY + 30, MUTED_TXT, u8g2_font_helvR08_tf);
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
    gfx_->fillRect(bx + 10, by - 6, 26, 13, PANEL_2);
    fontText("DU", bx + 13, by - 4, CYAN, u8g2_font_helvB08_tf);

    const int textX = bx + 10;
    wrappedFontText(text, textX, by + 12, bw - 20, 3,
                    TEXT, u8g2_font_helvR08_tf, 12);
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
    gfx_->fillRect(bx + 10, by - 6, 52, 13, PANEL_2);
    fontText("JARVIS", bx + 13, by - 4, GOLD, u8g2_font_helvB08_tf);

    const int textX = bx + 10;
    wrappedFontText(text, textX, by + 12, bw - 20, 3,
                    TEXT, u8g2_font_helvR08_tf, 12);
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