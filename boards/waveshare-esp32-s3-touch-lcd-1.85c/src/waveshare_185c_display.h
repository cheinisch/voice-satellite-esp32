#pragma once
#include <Arduino.h>
#include "board/board.h"
#include "waveshare_185c_expander.h"

class Arduino_GFX;
class Arduino_DataBus;

class Waveshare185CDisplay {
public:
    bool begin(Waveshare185CExpander& expander);
    void showState(SatelliteState state, const String& detail);
    void showTranscript(const String& text);
    void showAssistant(const String& text);
private:
    Arduino_DataBus* bus_ = nullptr;
    Arduino_GFX* gfx_ = nullptr;
    bool ready_ = false;
    void header(const char* title);
    void centered(const String& text, int y, uint16_t color, uint8_t size = 2);
    String compact(const String& text, size_t maxChars) const;
};
