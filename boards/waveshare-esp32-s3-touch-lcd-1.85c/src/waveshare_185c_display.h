#pragma once
#include <Arduino.h>
#include "board/board.h"
#include "waveshare_185c_expander.h"

class Arduino_GFX;
class Arduino_DataBus;

class Waveshare185CDisplay {
public:
    bool begin(Waveshare185CExpander& expander);
    void loop();
    void showState(SatelliteState state, const String& detail);
    void showTranscript(const String& text);
    void showAssistant(const String& text);

    // ── Standard touch hit-tests (active when NOT in media screen) ──────────
    bool hitCenterRecordButton(uint16_t x, uint16_t y) const;
    bool hitMicButton(uint16_t x, uint16_t y) const;
    bool hitNetworkButton(uint16_t x, uint16_t y) const;
    bool hitNetworkCloseButton(uint16_t x, uint16_t y) const;
    bool hitVolumeDown(uint16_t x, uint16_t y) const;
    bool hitVolumeUp(uint16_t x, uint16_t y) const;

    // ── Media screen ─────────────────────────────────────────────────────────
    // showMedia(info.active=true)  → switches to the dedicated media screen.
    // showMedia(info.active=false) → returns to the voice dashboard.
    void showMedia(const MediaInfo& info);

    // Touch hit-tests — only meaningful while mediaScreenActive() is true.
    bool hitMediaPlayPause(uint16_t x, uint16_t y) const;
    bool hitMediaPrev(uint16_t x, uint16_t y) const;
    bool hitMediaNext(uint16_t x, uint16_t y) const;
    bool hitMediaStop(uint16_t x, uint16_t y) const;
    bool mediaScreenActive() const { return mediaScreen_; }

    // Legacy alias kept so board.cpp compiles without changes.
    bool mediaOverlayActive() const { return mediaScreen_; }

    void toggleNetworkPopup();
    bool networkPopupVisible() const { return networkPopupVisible_; }
    void setVolumePercent(uint8_t percent);
    void setDisplayName(const String& name);
    void toggleDisplay();
    void setDisplayEnabled(bool enabled);
    bool displayEnabled() const { return displayOn_; }

private:
    Arduino_DataBus* bus_        = nullptr;
    Arduino_GFX*     gfx_        = nullptr;
    bool             ready_      = false;
    SatelliteState   state_      = SatelliteState::Booting;
    String           detail_;
    String           displayName_ = "Voice Satellite";
    uint32_t         lastClockAt_ = 0;
    int              lastMinute_  = -1;
    bool             displayOn_            = true;
    bool             networkPopupVisible_  = false;
    bool             messageBubbleVisible_ = false;
    bool             mediaScreen_          = false;   // true = media screen shown
    uint8_t          volumePercent_        = 70;
    MediaInfo        media_;

    // ── Dashboard rendering ──────────────────────────────────────────────────
    void renderDashboard();
    void renderCenterState(bool updateMic = false);
    void clearMessageArea();
    void renderClock(bool force = false);
    void renderDotGrid(uint16_t accent);
    void renderStatusLabels(uint16_t accent);
    void renderMicControl();
    void renderNetworkButton();
    void renderVolumeControls(bool partial = false);
    void renderNetworkPopup();

    // ── Media screen rendering ───────────────────────────────────────────────
    void renderMediaScreen();
    void renderMediaButtons();   // only redraws the three control circles

    // ── Text helpers ─────────────────────────────────────────────────────────
    int  textWidth(const String& text, const uint8_t* font);
    void fontText(const String& text, int x, int topY, uint16_t color,
                  const uint8_t* font);
    void centeredFont(const String& text, int topY, uint16_t color,
                      const uint8_t* font);
    void wrappedFontText(const String& text, int x, int topY, int maxWidth,
                         int maxLines, uint16_t color, const uint8_t* font,
                         int lineHeight);
    String compact(const String& text, size_t maxChars) const;

    uint16_t stateAccent() const;
    String   stateLabel()  const;
};
