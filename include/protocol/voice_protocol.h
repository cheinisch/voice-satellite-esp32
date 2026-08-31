#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <functional>
#include <utility>
#include "board/board.h"

enum class VoiceEvent {
    Connected,
    Disconnected,
    Ready,
    Transcript,
    Assistant,
    TtsStart,
    TtsEnd,
    Error,
};

class VoiceProtocol {
public:
    using BinaryHandler = std::function<void(const uint8_t*, size_t)>;
    using EventHandler = std::function<void(VoiceEvent, const String&)>;

    void begin(Board& board);
    void loop();
    bool connected() const { return connected_; }
    bool ready() const { return ready_; }
    const String& displayName() const { return displayName_; }

    void sendSessionStart(bool autoTts);
    bool sendWav(const uint8_t* data, size_t length);
    void sendAudioCommit();
    void sendPing();
    void sendTtsAck(uint32_t sequence, size_t receivedBytes);

    uint32_t ttsSampleRate() const { return ttsSampleRate_; }
    uint8_t ttsChannels() const { return ttsChannels_; }
    uint8_t ttsBitsPerSample() const { return ttsBitsPerSample_; }
    bool ttsAckRequired() const { return ttsAckRequired_; }
    size_t ttsExpectedBytes() const { return ttsExpectedBytes_; }
    uint32_t ttsExpectedChunks() const { return ttsExpectedChunks_; }

    void setBinaryHandler(BinaryHandler handler) { binaryHandler_ = std::move(handler); }
    void setEventHandler(EventHandler handler) { eventHandler_ = std::move(handler); }

private:
    WebSocketsClient ws_;
    String authorizationHeader_;
    bool connected_ = false;
    bool ready_ = false;
    String displayName_ = "Voice Satellite";
    bool binaryFragmentActive_ = false;
    Board* board_ = nullptr;
    BinaryHandler binaryHandler_;
    EventHandler eventHandler_;
    uint32_t ttsSampleRate_ = 16000;
    uint8_t ttsChannels_ = 1;
    uint8_t ttsBitsPerSample_ = 16;
    bool ttsAckRequired_ = false;
    size_t ttsExpectedBytes_ = 0;
    uint32_t ttsExpectedChunks_ = 0;

    void onEvent(WStype_t type, uint8_t* payload, size_t length);
    void sendHello();
    void handleText(const uint8_t* payload, size_t length);
    void sendJson(const String& json);
    void emit(VoiceEvent event, const String& text = String());
    void updateTtsFormat(JsonDocument& doc);
};