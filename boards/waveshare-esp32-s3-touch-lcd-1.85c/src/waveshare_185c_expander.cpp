#include "waveshare_185c_expander.h"

namespace {
constexpr uint8_t REG_INPUT = 0x00;
constexpr uint8_t REG_OUTPUT = 0x01;
constexpr uint8_t REG_CONFIG = 0x03;
}

bool Waveshare185CExpander::begin(TwoWire& wire, uint8_t address) {
    wire_ = &wire;
    address_ = address;
    uint8_t tmp = 0;
    if (!readReg(REG_OUTPUT, tmp)) return false;
    output_ = tmp;
    if (!readReg(REG_CONFIG, tmp)) return false;
    config_ = tmp;
    return true;
}

bool Waveshare185CExpander::writeReg(uint8_t reg, uint8_t value) {
    if (!wire_) return false;
    wire_->beginTransmission(address_);
    wire_->write(reg);
    wire_->write(value);
    return wire_->endTransmission() == 0;
}

bool Waveshare185CExpander::readReg(uint8_t reg, uint8_t& value) {
    if (!wire_) return false;
    wire_->beginTransmission(address_);
    wire_->write(reg);
    if (wire_->endTransmission(false) != 0) return false;
    if (wire_->requestFrom(address_, static_cast<uint8_t>(1)) != 1) return false;
    value = wire_->read();
    return true;
}

bool Waveshare185CExpander::pinModeOutput(uint8_t pin, bool initialHigh) {
    if (pin > 7) return false;
    if (initialHigh) output_ |= (1U << pin); else output_ &= ~(1U << pin);
    if (!writeReg(REG_OUTPUT, output_)) return false;
    config_ &= ~(1U << pin);
    return writeReg(REG_CONFIG, config_);
}

bool Waveshare185CExpander::write(uint8_t pin, bool high) {
    if (pin > 7) return false;
    if (high) output_ |= (1U << pin); else output_ &= ~(1U << pin);
    return writeReg(REG_OUTPUT, output_);
}
