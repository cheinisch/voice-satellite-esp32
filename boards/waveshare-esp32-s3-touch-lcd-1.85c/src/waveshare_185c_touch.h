#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "waveshare_185c_expander.h"

class Waveshare185CTouch {
public:
    bool begin(TwoWire& wire, Waveshare185CExpander& expander);
    bool consumeTap();
private:
    TwoWire* wire_ = nullptr;
    bool wasTouched_ = false;
    uint32_t lastPollAt_ = 0;
    bool readBytes(uint8_t reg, uint8_t* data, size_t len);
};
