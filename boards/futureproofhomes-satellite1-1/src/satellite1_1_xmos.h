#pragma once

#include <Arduino.h>

class Satellite11Xmos {
public:
    bool begin();
    bool connected() const { return connected_; }
    const char* version() const { return version_; }

private:
    bool readFirmwareVersion();
    bool transferRead(uint8_t resource, uint8_t command, uint8_t* payload, uint8_t payloadLen);

    bool connected_ = false;
    char version_[24] = "unknown";
};
