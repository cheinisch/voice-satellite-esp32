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
    bool beginWakeWord() override;
    void pauseWakeWord() override;
    void resumeWakeWord() override;
    bool consumeWakeWordTrigger() override;
    bool wakeWordActive() const override;
    const char* wakeWordName() const override;
    bool setVolume(uint8_t percent);
    uint8_t volume() const;

    // Voice Satellite Media Playback:
    // The Waveshare board normally owns the I2S controller for microphone /
    // TTS. MP3 playback temporarily hands that controller to ESP32-audioI2S.
    static Waveshare185CAudio* activeInstance();
    bool suspendForMediaPlayback();
    bool resumeAfterMediaPlayback();

    // Internal callback bridge used by Arduino ESP_SR.
    void markWakeWordDetected();

private:
    struct Impl;
    Impl* impl_;
    bool ensureSpeaker();
    bool startMicI2s();
    bool restoreMicPath();
    bool startSpeakerI2s();
};
