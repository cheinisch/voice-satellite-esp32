#include "satellite1_1_xmos.h"
#include "satellite1_1_pins.h"
#include <SPI.h>
#include <cstring>

namespace {
constexpr uint8_t kDfuResource = 240;
constexpr uint8_t kReadBit = 0x80;
constexpr uint8_t kGetVersion = 88 | kReadBit;
constexpr uint8_t kIgnored = 7;
SPISettings xmosSettings(8000000, MSBFIRST, SPI_MODE3);

void selectXmos(bool selected) {
    digitalWrite(SAT1_XMOS_CS_PIN, selected ? LOW : HIGH);
}
}

bool Satellite11Xmos::begin() {
    pinMode(SAT1_XMOS_CS_PIN, OUTPUT);
    selectXmos(false);
    pinMode(SAT1_XMOS_RESET_PIN, OUTPUT);

    SPI.begin(SAT1_SPI_SCLK_PIN, SAT1_SPI_MISO_PIN, SAT1_SPI_MOSI_PIN, SAT1_XMOS_CS_PIN);

    // Same reset polarity/timing used by the upstream Satellite1 control path.
    digitalWrite(SAT1_XMOS_RESET_PIN, HIGH);
    delay(100);
    digitalWrite(SAT1_XMOS_RESET_PIN, LOW);
    delay(150);

    connected_ = readFirmwareVersion();
    return connected_;
}

bool Satellite11Xmos::transferRead(uint8_t resource, uint8_t command, uint8_t* payload, uint8_t payloadLen) {
    if (!payload || payloadLen == 0 || payloadLen > 32) return false;

    uint8_t frame[40] = {0};
    frame[0] = resource;
    frame[1] = command;
    frame[2] = payloadLen + 1;

    for (int attempt = 0; attempt < 3; ++attempt) {
        SPI.beginTransaction(xmosSettings);
        selectXmos(true);
        for (size_t i = 0; i < static_cast<size_t>(payloadLen) + 3; ++i) {
            frame[i] = SPI.transfer(frame[i]);
        }
        selectXmos(false);
        SPI.endTransaction();

        if (frame[0] != kIgnored) break;
        delay(1);
    }

    if (frame[0] == kIgnored || (frame[0] == 0 && frame[1] == 0 && frame[2] == 0)) return false;

    memset(frame, 0, sizeof(frame));
    for (int attempt = 0; attempt < 3; ++attempt) {
        SPI.beginTransaction(xmosSettings);
        selectXmos(true);
        for (size_t i = 0; i < static_cast<size_t>(payloadLen) + 3; ++i) {
            frame[i] = SPI.transfer(0);
        }
        selectXmos(false);
        SPI.endTransaction();

        if (frame[0] != kIgnored) break;
        delay(1);
    }

    if (frame[0] == kIgnored) return false;
    memcpy(payload, &frame[1], payloadLen);
    return true;
}

bool Satellite11Xmos::readFirmwareVersion() {
    uint8_t version[5] = {0};
    if (!transferRead(kDfuResource, kGetVersion, version, sizeof(version))) {
        strncpy(version_, "not responding", sizeof(version_) - 1);
        return false;
    }

    if ((version[0] | version[1] | version[2] | version[3] | version[4]) == 0) {
        strncpy(version_, "invalid", sizeof(version_) - 1);
        return false;
    }

    if (version[3] == 0) {
        snprintf(version_, sizeof(version_), "v%u.%u.%u", version[0], version[1], version[2]);
    } else {
        const char* pre = version[3] == 1 ? "alpha" : version[3] == 2 ? "beta" : version[3] == 3 ? "rc" : "dev";
        snprintf(version_, sizeof(version_), "v%u.%u.%u-%s.%u", version[0], version[1], version[2], pre, version[4]);
    }
    return true;
}
