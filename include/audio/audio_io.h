#pragma once

#include <Arduino.h>

class AudioIO {
public:
    virtual ~AudioIO() = default;
    virtual bool begin() = 0;
    virtual size_t readPcm16(int16_t* dst, size_t samples, uint32_t timeoutMs = 100) = 0;
    virtual size_t writePcm16(const int16_t* src, size_t samples, uint32_t timeoutMs = 1000) = 0;
    virtual void clearOutput() = 0;

    // Optional local wake-word engine. Boards without a supported engine keep
    // these no-op defaults, so existing targets remain source compatible.
    virtual bool beginWakeWord() { return false; }
    virtual void pauseWakeWord() {}
    virtual void resumeWakeWord() {}
    virtual bool consumeWakeWordTrigger() { return false; }
    virtual bool wakeWordActive() const { return false; }
    virtual const char* wakeWordName() const { return ""; }
};
