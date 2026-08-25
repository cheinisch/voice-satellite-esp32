#pragma once

#include "board/board.h"
#include "satellite1_1_audio.h"
#include "satellite1_1_xmos.h"

class FutureProofHomesSatellite11Board : public Board {
public:
    bool begin() override;
    void loop() override;
    const char* model() const override { return "FutureProofHomes Satellite1.1"; }
    const char* profile() const override { return "futureproofhomes-satellite1-1"; }
    BoardCapabilities capabilities() const override;
    AudioIO& audio() override { return audio_; }
    bool consumeVoiceTrigger() override;
    void setState(SatelliteState state, const String& detail) override;
    void showTranscript(const String& text) override;
    void showAssistant(const String& text) override;

private:
    void updateStatusLed(SatelliteState state);
    Satellite11Audio audio_;
    Satellite11Xmos xmos_;
    bool lastButton_ = false;
    bool trigger_ = false;
    uint32_t lastEdgeAt_ = 0;
};
