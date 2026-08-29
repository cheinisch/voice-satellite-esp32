#pragma once
#include "board/board.h"
#include "waveshare_185c_audio.h"
#include "waveshare_185c_display.h"
#include "waveshare_185c_expander.h"
#include "waveshare_185c_touch.h"

class Waveshare185CBoard : public Board {
public:
    bool begin() override;
    void loop() override;
    const char* model() const override { return "Waveshare ESP32-S3-Touch-LCD-1.85C V2"; }
    const char* profile() const override { return "waveshare-esp32-s3-touch-lcd-1.85c"; }
    BoardCapabilities capabilities() const override;
    AudioIO& audio() override { return audio_; }
    bool consumeVoiceTrigger() override;
    bool consumeMuteToggle() override;
    void setState(SatelliteState state, const String& detail) override;
    void showTranscript(const String& text) override;
    void showAssistant(const String& text) override;
private:
    Waveshare185CExpander expander_;
    Waveshare185CTouch touch_;
    Waveshare185CDisplay display_;
    Waveshare185CAudio audio_;
    bool trigger_ = false;
    bool muteToggle_ = false;
    bool lastBoot_ = false;
    uint32_t lastBootEdgeAt_ = 0;
};
