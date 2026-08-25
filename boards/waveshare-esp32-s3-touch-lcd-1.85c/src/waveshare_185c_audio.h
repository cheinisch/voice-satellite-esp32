#pragma once
#include "audio/audio_io.h"

class Waveshare185CAudio : public AudioIO {
public:
    Waveshare185CAudio();
    ~Waveshare185CAudio() override;
    bool begin() override;
    size_t readPcm16(int16_t* dst, size_t samples, uint32_t timeoutMs) override;
    size_t writePcm16(const int16_t* src, size_t samples, uint32_t timeoutMs) override;
    void clearOutput() override;
private:
    struct Impl;
    Impl* impl_;
};
