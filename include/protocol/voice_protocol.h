#pragma once

#include <Arduino.h>
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

    void begin(const Board& board);
    void loop();
    bool connected() const { return connected_; }
    bool ready() const { return ready_; }

    void sendAudioStart();
    bool sendAudio(const int16_t* samples, size_t count);
    void sendAudioEnd();
    void sendPing();

    void setBinaryHandler(BinaryHandler handler) { binaryHandler_ = std::move(handler); }
    void setEventHandler(EventHandler handler) { eventHandler_ = std::move(handler); }

private:
    WebSocketsClient ws_;
    bool connected_ = false;
    bool ready_ = false;
    const Board* board_ = nullptr;
    BinaryHandler binaryHandler_;
    EventHandler eventHandler_;

    void onEvent(WStype_t type, uint8_t* payload, size_t length);
    void sendHello();
    void handleText(const uint8_t* payload, size_t length);
    void sendJson(const String& json);
    void emit(VoiceEvent event, const String& text = String());
};
