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
    bool muted_ = false;
    uint32_t recordingStartedAt_ = 0;
    uint32_t lastStatusAt_ = 0;
    String serialCommand_;
    bool sttTestActive_ = false;
    bool ttsTestActive_ = false;
    uint8_t* recordingBuffer_ = nullptr;
    size_t recordingCapacityBytes_ = 0;
    size_t recordingPcmBytes_ = 0;

    bool ttsReceiving_ = false;
    bool ttsPlaybackPending_ = false;
    bool ttsPlaybackActive_ = false;
    uint8_t* ttsBuffer_ = nullptr;
    size_t ttsBufferCapacity_ = 0;
    size_t ttsBufferBytes_ = 0;
    size_t ttsPcmOffset_ = 0;
    size_t ttsPcmEnd_ = 0;
    uint32_t ttsInputRate_ = 16000;
    uint8_t ttsInputChannels_ = 1;
    uint32_t ttsResampleAccumulator_ = 0;
    size_t ttsInputBytes_ = 0;
    size_t ttsOutputSamples_ = 0;
    size_t ttsExpectedBytes_ = 0;
    uint32_t ttsChunkSequence_ = 0;

    void ensureProtocol();
    void setUiState(SatelliteState state, const String& detail = String());
    void toggleMute();
    void startRecording(bool autoTts = true);
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
    void runSpeakerTest();
    void handleTtsBinary(const uint8_t* data, size_t length);
    void resetTtsPlayback(uint32_t sampleRate, uint8_t channels);
    bool allocateTtsBuffer();
    void freeTtsBuffer();
    bool appendTtsData(const uint8_t* data, size_t length);
    bool prepareTtsPlayback();
    void pumpTtsPlayback();
    void finishTtsPlayback();
};
