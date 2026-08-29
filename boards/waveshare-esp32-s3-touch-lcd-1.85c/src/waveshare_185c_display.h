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
    bool hitRecordButton(uint16_t x, uint16_t y) const;
    bool hitMuteButton(uint16_t x, uint16_t y) const;

private:
    Arduino_DataBus* bus_        = nullptr;
    Arduino_GFX*     gfx_        = nullptr;
    bool             ready_      = false;
    SatelliteState   state_      = SatelliteState::Booting;
    String           detail_;
    uint32_t         lastClockAt_ = 0;
    int              lastMinute_  = -1;

    void renderDashboard();
    void renderClock(bool force = false);
    void renderDotGrid(uint16_t accent);
    void renderStatusLabels(uint16_t accent);

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