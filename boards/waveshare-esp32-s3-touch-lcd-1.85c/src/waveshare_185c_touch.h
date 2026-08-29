#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "waveshare_185c_expander.h"

struct Waveshare185CTouchPoint {
    uint16_t x = 0;
    uint16_t y = 0;
};

class Waveshare185CTouch {
public:
    bool begin(TwoWire& wire, Waveshare185CExpander& expander);
    bool consumeTap();
    bool consumeTap(Waveshare185CTouchPoint& point);
private:
    TwoWire* wire_ = nullptr;
    bool wasTouched_ = false;
    uint32_t lastPollAt_ = 0;
    bool readBytes(uint8_t reg, uint8_t* data, size_t len);
    void transform(uint16_t& x, uint16_t& y) const;
};
