#include "waveshare_185c_display.h"
#include "waveshare_185c_pins.h"
#include "voice_satellite_config.h"
#include <WiFi.h>
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

// Volume controls live together on the right edge.  The visible circles stay
// compact, but the touch radius is deliberately much larger for reliable use
// on the small round panel.
constexpr int VOL_X        = 329;
constexpr int VOL_UP_Y     = 158;
constexpr int VOL_DOWN_Y   = 222;
constexpr int VOL_R        = 18;
constexpr int VOL_HIT_R    = 34;
constexpr int VOL_REGION_X = 293;
constexpr int VOL_REGION_Y = 120;
constexpr int VOL_REGION_W = 62;
constexpr int VOL_REGION_H = 140;

// Primary voice action: the large centre status ring itself is the record
// button.  The hit radius is slightly larger than the visible outer ring so
// it remains easy to hit without creating hidden controls elsewhere.
constexpr int CENTER_HIT_R = 78;

// Dedicated microphone mute/listen control on the left edge.  The visible
// button stays compact while the touch radius is deliberately generous.
constexpr int MIC_X     = 31;
constexpr int MIC_Y     = 190;
constexpr int MIC_R     = 18;
constexpr int MIC_HIT_R = 34;

constexpr int NET_X = 278;
constexpr int NET_Y = 84;
constexpr int NET_W = 52;
constexpr int NET_H = 24;

constexpr int POP_X = 43;
constexpr int POP_Y = 88;
constexpr int POP_W = 274;
constexpr int POP_H = 188;

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

    const uint8_t rotation = static_cast<uint8_t>(VOICE_SATELLITE_DISPLAY_ROTATION_INDEX);
    gfx_ = new Arduino_ST77916(
        bus_, -1, rotation, true, 360, 360, 0, 0, 0, 0,
        st77916_150_init_operations, sizeof(st77916_150_init_operations));

    if (!gfx_ || !gfx_->begin(80000000)) {
        Serial.println("Display: ST77916/QSPI begin fehlgeschlagen.");
        return false;
    }

    // begin() applies the constructor rotation through Arduino_ST77916::setRotation().
    // Apply it once more explicitly so a future Arduino_GFX init-table change cannot
    // leave MADCTL at the panel default.
    gfx_->setRotation(rotation);
#ifdef VOICE_SATELLITE_DISPLAY_ROTATION_INVALID
    Serial.printf("WARNUNG: Ungueltige Display-Rotation %d; verwende 0 Grad.\n",
                  static_cast<int>(VOICE_SATELLITE_DISPLAY_ROTATION));
#endif
    Serial.printf("Display: Rotation config=%d -> index=%u (%u Grad).\n",
                  static_cast<int>(VOICE_SATELLITE_DISPLAY_ROTATION), rotation,
                  static_cast<unsigned>(rotation) * 90U);

    gfx_->setUTF8Print(true);
    gfx_->setTextWrap(false);
    Serial.println("Display: U8g2-Fonts + UTF-8 aktiviert.");
    Serial.println("Display: ST77916/QSPI bereit (360x360, 80 MHz).");
    digitalWrite(waveshare185c::LCD_BL, HIGH);
    delay(20);

    ready_ = true;
    displayOn_ = true;
    state_ = SatelliteState::Booting;
    detail_ = "";
    renderDashboard();
    Serial.println("Display: Voice Satellite UI gerendert.");
    return true;
}

void Waveshare185CDisplay::loop() {
    if (!ready_ || !displayOn_) return;
    renderClock(false);
}

// ---------------------------------------------------------------------------
// Clock — larger U8g2 Logisoso face, slightly lower than the old bitmap clock
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::renderClock(bool force) {
    if (!ready_ || !displayOn_) return;
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
    centeredFont(compact(displayName_, 28), 65, MUTED_TXT, u8g2_font_helvR08_tf);
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
    centeredFont(compact(displayName_, 28), baseY, MUTED_TXT, u8g2_font_helvR08_tf);
    centeredFont(stateLabel(), baseY + 14, accent, u8g2_font_helvB10_tf);
    if (detail_.length()) {
        gfx_->fillRect(48, baseY + 30, 264, 14, BG);
        centeredFont(compact(detail_, 36), baseY + 30, MUTED_TXT, u8g2_font_helvR08_tf);
    }
}

// ---------------------------------------------------------------------------
// Partial state redraw
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::clearMessageArea() {
    // Transcript/assistant bubbles live in this small lower-centre area.
    // Clear only that area instead of rebuilding the whole dashboard.
    gfx_->fillRect(126, 282, 108, 68, BG);
    messageBubbleVisible_ = false;
}

void Waveshare185CDisplay::renderCenterState(bool updateMic) {
    if (!ready_ || !displayOn_ || networkPopupVisible_) return;

    const uint16_t accent = stateAccent();

    // The ring itself can be redrawn in place. renderDotGrid() completely
    // repaints its interior and all state-coloured ring pixels.
    renderDotGrid(accent);

    // State/detail strings have different widths. Clear only their local
    // label strip first so shorter labels never leave glyph remnants behind.
    const int baseY = CY + R2 + 9;
    gfx_->fillRect(52, baseY - 2, 256, 46, BG);
    renderStatusLabels(accent);

    if (updateMic) {
        renderMicControl();
    }
}

// ---------------------------------------------------------------------------
// Edge controls + network popup
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::renderMicControl() {
    const bool muted = state_ == SatelliteState::Muted;
    const uint16_t accent = muted ? RED : CYAN;

    gfx_->fillCircle(MIC_X, MIC_Y, MIC_R, PANEL_2);
    gfx_->drawCircle(MIC_X, MIC_Y, MIC_R, BORDER);

    // Small vector microphone icon so no additional icon font is required.
    // Capsule / microphone body.
    gfx_->drawRoundRect(MIC_X - 4, MIC_Y - 9, 8, 13, 4, accent);
    // Support / stem.
    gfx_->drawLine(MIC_X - 8, MIC_Y - 1, MIC_X - 8, MIC_Y + 1, accent);
    gfx_->drawLine(MIC_X + 8, MIC_Y - 1, MIC_X + 8, MIC_Y + 1, accent);
    gfx_->drawLine(MIC_X - 8, MIC_Y + 1, MIC_X - 5, MIC_Y + 6, accent);
    gfx_->drawLine(MIC_X + 8, MIC_Y + 1, MIC_X + 5, MIC_Y + 6, accent);
    gfx_->drawLine(MIC_X - 5, MIC_Y + 6, MIC_X + 5, MIC_Y + 6, accent);
    gfx_->drawLine(MIC_X, MIC_Y + 8, MIC_X, MIC_Y + 12, accent);
    gfx_->drawLine(MIC_X - 5, MIC_Y + 12, MIC_X + 5, MIC_Y + 12, accent);

    if (muted) {
        gfx_->drawLine(MIC_X - 11, MIC_Y - 12, MIC_X + 11, MIC_Y + 12, RED);
        gfx_->drawLine(MIC_X - 10, MIC_Y - 12, MIC_X + 12, MIC_Y + 10, RED);
    }
}

void Waveshare185CDisplay::renderNetworkButton() {
    gfx_->fillRoundRect(NET_X, NET_Y, NET_W, NET_H, 8, PANEL_2);
    gfx_->drawRoundRect(NET_X, NET_Y, NET_W, NET_H, 8, BORDER);
    fontText("NET", NET_X + 13, NET_Y + 6, CYAN, u8g2_font_helvB08_tf);
}

void Waveshare185CDisplay::renderVolumeControls(bool partial) {
    // A volume change should not repaint the complete dashboard.  Clearing
    // only this right-edge strip prevents the visible full-screen flicker.
    if (partial) {
        gfx_->fillRect(VOL_REGION_X, VOL_REGION_Y, VOL_REGION_W, VOL_REGION_H, BG);
    }

    // Louder (+) above, quieter (-) below: both controls are on one side.
    gfx_->fillCircle(VOL_X, VOL_UP_Y, VOL_R, PANEL_2);
    gfx_->drawCircle(VOL_X, VOL_UP_Y, VOL_R, BORDER);
    gfx_->drawLine(VOL_X - 7, VOL_UP_Y, VOL_X + 7, VOL_UP_Y, TEXT);
    gfx_->drawLine(VOL_X, VOL_UP_Y - 7, VOL_X, VOL_UP_Y + 7, TEXT);

    gfx_->fillCircle(VOL_X, VOL_DOWN_Y, VOL_R, PANEL_2);
    gfx_->drawCircle(VOL_X, VOL_DOWN_Y, VOL_R, BORDER);
    gfx_->drawLine(VOL_X - 7, VOL_DOWN_Y, VOL_X + 7, VOL_DOWN_Y, TEXT);

    // Small centre label between + and -.
    const String vol = String(volumePercent_) + "%";
    const int labelW = textWidth(vol, u8g2_font_helvR08_tf);
    fontText(vol, max(VOL_REGION_X + 2, VOL_X - labelW / 2), 188,
             MUTED_TXT, u8g2_font_helvR08_tf);
}

void Waveshare185CDisplay::renderNetworkPopup() {
    if (!networkPopupVisible_) return;

    gfx_->fillRoundRect(POP_X, POP_Y, POP_W, POP_H, 14, BG);
    gfx_->drawRoundRect(POP_X, POP_Y, POP_W, POP_H, 14, CYAN);
    gfx_->drawRoundRect(POP_X + 1, POP_Y + 1, POP_W - 2, POP_H - 2, 13, BORDER);

    fontText("NETZWERK", POP_X + 18, POP_Y + 14, CYAN, u8g2_font_helvB10_tf);

    // Close affordance. The surrounding hit area is deliberately larger than
    // the visible X so it remains easy to tap on the 1.85" panel.
    const int closeX = POP_X + POP_W - 28;
    const int closeY = POP_Y + 20;
    gfx_->drawLine(closeX - 5, closeY - 5, closeX + 5, closeY + 5, MUTED_TXT);
    gfx_->drawLine(closeX + 5, closeY - 5, closeX - 5, closeY + 5, MUTED_TXT);

    const bool connected = WiFi.status() == WL_CONNECTED;
    const String ssid = connected ? WiFi.SSID() : String("nicht verbunden");
    const String ip = connected ? WiFi.localIP().toString() : String("-");
    const String gateway = connected ? WiFi.gatewayIP().toString() : String("-");
    const String dns = connected ? WiFi.dnsIP().toString() : String("-");
    const String rssi = connected ? String(WiFi.RSSI()) + " dBm" : String("-");
    const String core = String(VOICE_SATELLITE_CORE_HOST) + ":" + String(VOICE_SATELLITE_CORE_PORT);

    const int labelX = POP_X + 18;
    const int valueX = POP_X + 78;
    int y = POP_Y + 46;

    auto row = [&](const char* label, const String& value, uint16_t valueColor) {
        fontText(label, labelX, y, MUTED_TXT, u8g2_font_helvR08_tf);
        String shown = value;
        const int maxValueWidth = POP_X + POP_W - 18 - valueX;
        while (shown.length() > 3 && textWidth(shown, u8g2_font_helvR08_tf) > maxValueWidth) {
            shown.remove(shown.length() - 1);
        }
        if (shown != value && shown.length() > 3) {
            while (shown.length() > 3 && textWidth(shown + "...", u8g2_font_helvR08_tf) > maxValueWidth) {
                shown.remove(shown.length() - 1);
            }
            shown += "...";
        }
        fontText(shown, valueX, y, valueColor, u8g2_font_helvR08_tf);
        y += 20;
    };

    row("WLAN", ssid, TEXT);
    row("IP", ip, TEXT);
    row("GW", gateway, TEXT);
    row("DNS", dns, TEXT);
    row("RSSI", rssi, connected && WiFi.RSSI() <= -80 ? RED : (connected && WiFi.RSSI() <= -67 ? GOLD : CYAN));
    row("CORE", core, TEXT);
}

void Waveshare185CDisplay::toggleNetworkPopup() {
    if (!ready_ || !displayOn_) return;
    networkPopupVisible_ = !networkPopupVisible_;
    renderDashboard();
}

void Waveshare185CDisplay::setVolumePercent(uint8_t percent) {
    volumePercent_ = percent > 100 ? 100 : percent;
    if (ready_ && displayOn_) renderVolumeControls(true);
}

void Waveshare185CDisplay::setDisplayName(const String& name) {
    String next = name;
    next.trim();
    if (!next.length()) next = "Voice Satellite";
    if (next == displayName_) return;

    displayName_ = next;
    Serial.printf("Display: Anzeigename = %s\n", displayName_.c_str());

    if (!ready_ || !displayOn_ || networkPopupVisible_) return;

    // Update only the two branding areas. Avoid a full-screen redraw when the
    // Core handshake supplies the configured display name.
    renderClock(true);
    renderCenterState(false);
}

void Waveshare185CDisplay::setDisplayEnabled(bool enabled) {
    if (!ready_ || displayOn_ == enabled) return;
    displayOn_ = enabled;
    networkPopupVisible_ = false;
    digitalWrite(waveshare185c::LCD_BL, enabled ? HIGH : LOW);
    if (enabled) {
        delay(10);
        renderDashboard();
        Serial.println("Display: an.");
    } else {
        Serial.println("Display: aus (Backlight). Voice Satellite bleibt aktiv.");
    }
}

void Waveshare185CDisplay::toggleDisplay() {
    setDisplayEnabled(!displayOn_);
}

// ---------------------------------------------------------------------------
// Dashboard
// ---------------------------------------------------------------------------

void Waveshare185CDisplay::renderDashboard() {
    if (!ready_ || !displayOn_) return;
    gfx_->fillScreen(BG);
    messageBubbleVisible_ = false;

    const uint16_t accent = stateAccent();

    // Corner brackets
    gfx_->drawLine( 44,  6,  84,  6, BORDER); gfx_->drawLine( 44,  6,  44, 18, BORDER);
    gfx_->drawLine(276,  6, 316,  6, BORDER); gfx_->drawLine(316,  6, 316, 18, BORDER);
    gfx_->drawLine( 44, 354,  44, 342, BORDER); gfx_->drawLine( 44, 354,  84, 354, BORDER);
    gfx_->drawLine(316, 354, 316, 342, BORDER); gfx_->drawLine(316, 354, 276, 354, BORDER);

    renderClock(true);
    renderDotGrid(accent);
    renderStatusLabels(accent);
    renderMicControl();
    renderVolumeControls();
    renderNetworkButton();
    if (networkPopupVisible_) renderNetworkPopup();
}

void Waveshare185CDisplay::showState(SatelliteState state, const String& detail) {
    if (!ready_) return;

    const SatelliteState previous = state_;
    const bool muteChanged =
        (previous == SatelliteState::Muted) != (state == SatelliteState::Muted);

    state_ = state;
    detail_ = detail;

    if (!displayOn_) return;

    // Do not paint underneath an open modal. Closing the network popup already
    // performs one complete dashboard render using the newest state.
    if (networkPopupVisible_) return;

    // Leaving transcript/speaking mode should remove the previous bubble, but
    // only from its own small region.
    if (messageBubbleVisible_ &&
        state != SatelliteState::Processing &&
        state != SatelliteState::Speaking) {
        clearMessageArea();
    }

    renderCenterState(muteChanged);
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
    const bool muteChanged = state_ == SatelliteState::Muted;
    state_ = SatelliteState::Processing;
    detail_ = "";
    if (!displayOn_) return;
    if (networkPopupVisible_) return;

    renderCenterState(muteChanged);

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
    messageBubbleVisible_ = true;
}

void Waveshare185CDisplay::showAssistant(const String& text) {
    if (!ready_) return;
    const bool muteChanged = state_ == SatelliteState::Muted;
    state_ = SatelliteState::Speaking;
    detail_ = "";
    if (!displayOn_) return;
    if (networkPopupVisible_) return;

    renderCenterState(muteChanged);

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

    // Assistant badge uses the display name supplied by the Core.
    const String assistantBadge = compact(displayName_, 18);
    const int assistantBadgeWidth = min(bw - 20, max(40, textWidth(assistantBadge, u8g2_font_helvB08_tf) + 8));
    gfx_->fillRect(bx + 10, by - 6, assistantBadgeWidth, 13, PANEL_2);
    fontText(assistantBadge, bx + 13, by - 4, GOLD, u8g2_font_helvB08_tf);

    const int textX = bx + 10;
    wrappedFontText(text, textX, by + 12, bw - 20, 3,
                    TEXT, u8g2_font_helvR08_tf, 12);
    messageBubbleVisible_ = true;
}

// ---------------------------------------------------------------------------
// Touch
// ---------------------------------------------------------------------------

bool Waveshare185CDisplay::hitCenterRecordButton(uint16_t x, uint16_t y) const {
    const int dx = static_cast<int>(x) - CX;
    const int dy = static_cast<int>(y) - CY;
    return dx * dx + dy * dy <= CENTER_HIT_R * CENTER_HIT_R;
}

bool Waveshare185CDisplay::hitMicButton(uint16_t x, uint16_t y) const {
    const int dx = static_cast<int>(x) - MIC_X;
    const int dy = static_cast<int>(y) - MIC_Y;
    return dx * dx + dy * dy <= MIC_HIT_R * MIC_HIT_R;
}

bool Waveshare185CDisplay::hitNetworkButton(uint16_t x, uint16_t y) const {
    constexpr int pad = 12;
    const int px = static_cast<int>(x);
    const int py = static_cast<int>(y);
    return px >= NET_X - pad && px <= NET_X + NET_W + pad &&
           py >= NET_Y - pad && py <= NET_Y + NET_H + pad;
}

bool Waveshare185CDisplay::hitNetworkCloseButton(uint16_t x, uint16_t y) const {
    if (!networkPopupVisible_) return false;
    return x >= POP_X + POP_W - 52 && x <= POP_X + POP_W &&
           y >= POP_Y && y <= POP_Y + 52;
}

bool Waveshare185CDisplay::hitVolumeDown(uint16_t x, uint16_t y) const {
    const int dx = static_cast<int>(x) - VOL_X;
    const int dy = static_cast<int>(y) - VOL_DOWN_Y;
    return dx * dx + dy * dy <= VOL_HIT_R * VOL_HIT_R;
}

bool Waveshare185CDisplay::hitVolumeUp(uint16_t x, uint16_t y) const {
    const int dx = static_cast<int>(x) - VOL_X;
    const int dy = static_cast<int>(y) - VOL_UP_Y;
    return dx * dx + dy * dy <= VOL_HIT_R * VOL_HIT_R;
}

