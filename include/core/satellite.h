#pragma once

#include <Arduino.h>
#include "board/board.h"
#include "network/wifi_manager.h"
#include "protocol/voice_protocol.h"

class Satellite {
public:
    explicit Satellite(Board& board);
    bool begin();
    void loop();

private:
    Board& board_;
    WifiManager wifi_;
    VoiceProtocol protocol_;
    bool protocolStarted_ = false;
    bool recording_ = false;
    uint32_t recordingStartedAt_ = 0;
    uint32_t lastStatusAt_ = 0;
    String serialCommand_;
    bool sttTestActive_ = false;
    uint8_t* recordingBuffer_ = nullptr;
    size_t recordingCapacityBytes_ = 0;
    size_t recordingPcmBytes_ = 0;

    void ensureProtocol();
    void startRecording();
    void stopRecording();
    void pumpRecording();
    bool allocateRecordingBuffer();
    void freeRecordingBuffer();
    void finalizeWavHeader();
    void onVoiceEvent(VoiceEvent event, const String& text);
    void pollSerialConsole();
    void printStatus() const;
    void printConsoleHelp() const;
    void runMicTest();
};
