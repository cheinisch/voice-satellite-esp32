#include "waveshare_185c_touch.h"
#include "waveshare_185c_pins.h"
#include "jarvis_config.h"
#include <algorithm>

namespace {
constexpr uint16_t TOUCH_MAX = 359;
}

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

void Waveshare185CTouch::transform(uint16_t& x, uint16_t& y) const {
#if JARVIS_WAVESHARE_TOUCH_SWAP_XY
    std::swap(x, y);
#endif
#if JARVIS_WAVESHARE_TOUCH_INVERT_X
    x = x <= TOUCH_MAX ? TOUCH_MAX - x : 0;
#endif
#if JARVIS_WAVESHARE_TOUCH_INVERT_Y
    y = y <= TOUCH_MAX ? TOUCH_MAX - y : 0;
#endif
    if (x > TOUCH_MAX) x = TOUCH_MAX;
    if (y > TOUCH_MAX) y = TOUCH_MAX;

    // Keep the touch coordinate system aligned with Arduino_GFX setRotation().
    // Hardware-specific swap/invert corrections above are applied first.
    const uint16_t rawX = x;
    const uint16_t rawY = y;
    switch (static_cast<uint8_t>(JARVIS_DISPLAY_ROTATION) & 0x03) {
        case 1:
            x = TOUCH_MAX - rawY;
            y = rawX;
            break;
        case 2:
            x = TOUCH_MAX - rawX;
            y = TOUCH_MAX - rawY;
            break;
        case 3:
            x = rawY;
            y = TOUCH_MAX - rawX;
            break;
        default:
            break;
    }
}

bool Waveshare185CTouch::consumeTap() {
    Waveshare185CTouchPoint point;
    return consumeTap(point);
}

bool Waveshare185CTouch::consumeTap(Waveshare185CTouchPoint& point) {
    if (!wire_) return false;

    // CST816 INT is active-low. Only access I2C while the controller signals
    // an event; the audio codecs share the same Wire bus on the Waveshare V2.
    if (digitalRead(waveshare185c::TOUCH_INT) != LOW) {
        wasTouched_ = false;
        return false;
    }

    if (millis() - lastPollAt_ < 35) return false;
    lastPollAt_ = millis();

    // Finger count + XH + XL + YH + YL.
    uint8_t data[5] = {0};
    if (!readBytes(0x02, data, sizeof(data))) return false;
    const bool touched = (data[0] & 0x0F) > 0;
    const bool rising = touched && !wasTouched_;
    wasTouched_ = touched;
    if (!rising) return false;

    uint16_t x = (static_cast<uint16_t>(data[1] & 0x0F) << 8) | data[2];
    uint16_t y = (static_cast<uint16_t>(data[3] & 0x0F) << 8) | data[4];
    transform(x, y);
    point.x = x;
    point.y = y;
    return true;
}
