#pragma once
#include <Arduino.h>

class WifiManager {
public:
    bool begin();
    void loop();
    bool connected() const;
    String ip() const;

private:
    uint32_t nextRetryAt_ = 0;
    bool connect(uint32_t timeoutMs);
};
