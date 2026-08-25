#pragma once
#include "board/board.h"
#include "generic_esp32s3_audio.h"

class GenericEsp32S3Board : public Board {
public:
    bool begin() override;
    void loop() override;
    const char* model() const override { return "ESP32-S3"; }
    const char* profile() const override { return "generic-esp32s3"; }
    BoardCapabilities capabilities() const override;
    AudioIO& audio() override { return audio_; }
    bool consumeVoiceTrigger() override;
    void setState(SatelliteState state, const String& detail) override;
    void showTranscript(const String& text) override;
    void showAssistant(const String& text) override;
private:
    GenericEsp32S3Audio audio_;
    bool lastButton_ = false;
    bool trigger_ = false;
    uint32_t lastEdgeAt_ = 0;
};
