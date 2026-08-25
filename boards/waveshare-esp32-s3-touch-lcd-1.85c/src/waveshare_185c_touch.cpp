#include "waveshare_185c_touch.h"
#include "waveshare_185c_pins.h"

bool Waveshare185CTouch::begin(TwoWire& wire, Waveshare185CExpander& expander) {
    wire_ = &wire;
    pinMode(waveshare185c::TOUCH_INT, INPUT_PULLUP);
    if (!expander.pinModeOutput(waveshare185c::EXIO_TOUCH_RST, true)) return false;
    expander.write(waveshare185c::EXIO_TOUCH_RST, false);
    delay(10);
    expander.write(waveshare185c::EXIO_TOUCH_RST, true);
    delay(60);

    uint8_t finger = 0;
    return readBytes(0x02, &finger, 1);
}

bool Waveshare185CTouch::readBytes(uint8_t reg, uint8_t* data, size_t len) {
    if (!wire_ || !data || !len) return false;
    wire_->beginTransmission(waveshare185c::CST816_ADDR);
    wire_->write(reg);
    if (wire_->endTransmission(false) != 0) return false;
    const size_t got = wire_->requestFrom(waveshare185c::CST816_ADDR, static_cast<uint8_t>(len));
    if (got != len) return false;
    for (size_t i = 0; i < len; ++i) data[i] = wire_->read();
    return true;
}

bool Waveshare185CTouch::consumeTap() {
    if (!wire_ || millis() - lastPollAt_ < 35) return false;
    lastPollAt_ = millis();
    uint8_t finger = 0;
    if (!readBytes(0x02, &finger, 1)) return false;
    const bool touched = (finger & 0x0F) > 0;
    const bool rising = touched && !wasTouched_;
    wasTouched_ = touched;
    return rising;
}
