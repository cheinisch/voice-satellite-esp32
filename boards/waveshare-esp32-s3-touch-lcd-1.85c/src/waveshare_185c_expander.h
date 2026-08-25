#pragma once
#include <Arduino.h>
#include <Wire.h>

class Waveshare185CExpander {
public:
    bool begin(TwoWire& wire, uint8_t address);
    bool pinModeOutput(uint8_t pin, bool initialHigh);
    bool write(uint8_t pin, bool high);
private:
    TwoWire* wire_ = nullptr;
    uint8_t address_ = 0;
    uint8_t output_ = 0xFF;
    uint8_t config_ = 0xFF;
    bool writeReg(uint8_t reg, uint8_t value);
    bool readReg(uint8_t reg, uint8_t& value);
};
