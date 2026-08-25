#include "protocol/voice_protocol.h"
#include "build_info.h"
#include "jarvis_config.h"
#include <ArduinoJson.h>
#include <cstring>

namespace {
const char* firstString(JsonDocument& doc, const char* a, const char* b) {
    const char* value = doc[a].as<const char*>();
    if (value && *value) return value;
    value = doc[b].as<const char*>();
    return value ? value : "";
}
}

void VoiceProtocol::begin(const Board& board) {
    board_ = &board;
#if JARVIS_CORE_TLS
    ws_.beginSSL(JARVIS_CORE_HOST, JARVIS_CORE_PORT, JARVIS_CORE_PATH);
#else
    ws_.begin(JARVIS_CORE_HOST, JARVIS_CORE_PORT, JARVIS_CORE_PATH);
#endif
    ws_.setReconnectInterval(JARVIS_RECONNECT_MS);
    ws_.enableHeartbeat(15000, 3000, 2);
    ws_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        onEvent(type, payload, length);
    });
}

void VoiceProtocol::loop() {
    ws_.loop();
}

void VoiceProtocol::emit(VoiceEvent event, const String& text) {
    if (eventHandler_) eventHandler_(event, text);
}

void VoiceProtocol::onEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            connected_ = true;
            ready_ = false;
            Serial.printf("Core verbunden: %s:%d%s\n", JARVIS_CORE_HOST, JARVIS_CORE_PORT, JARVIS_CORE_PATH);
            emit(VoiceEvent::Connected);
            sendHello();
            break;
        case WStype_DISCONNECTED:
            connected_ = false;
            ready_ = false;
            Serial.println("Core getrennt; Reconnect läuft ...");
            emit(VoiceEvent::Disconnected);
            break;
        case WStype_TEXT:
            handleText(payload, length);
            break;
        case WStype_BIN:
            if (binaryHandler_) binaryHandler_(payload, length);
            break;
        case WStype_ERROR:
            Serial.println("WebSocket-Fehler");
            emit(VoiceEvent::Error, "WebSocket-Fehler");
            break;
        default:
            break;
    }
}

void VoiceProtocol::sendHello() {
    JsonDocument doc;
    doc["type"] = "hello";
    doc["protocol"] = JARVIS_PROTOCOL_NAME;
    doc["client"] = JARVIS_CLIENT_NAME;
    doc["client_version"] = JARVIS_SATELLITE_VERSION;
    doc["client_build"] = JARVIS_SATELLITE_BUILD;
    doc["satellite_id"] = JARVIS_SATELLITE_ID;
    doc["satellite_name"] = JARVIS_SATELLITE_NAME;
    doc["transport"] = "websocket";

    if (board_) {
        doc["board_model"] = board_->model();
        doc["board_profile"] = board_->profile();
        const BoardCapabilities caps = board_->capabilities();
        JsonObject capabilities = doc["capabilities"].to<JsonObject>();
        capabilities["microphone"] = caps.microphone;
        capabilities["speaker"] = caps.speaker;
        capabilities["display"] = caps.display;
        capabilities["touch"] = caps.touch;
        capabilities["buttons"] = caps.buttons;
        capabilities["psram"] = caps.psram;
        capabilities["sdcard"] = caps.sdcard;
    }

    JsonObject audio = doc["audio"].to<JsonObject>();
    audio["format"] = "pcm_s16le";
    audio["sample_rate"] = JARVIS_AUDIO_RATE;
    audio["channels"] = JARVIS_AUDIO_CHANNELS;

    String out;
    serializeJson(doc, out);
    sendJson(out);
}

void VoiceProtocol::handleText(const uint8_t* payload, size_t length) {
    String text;
    text.reserve(length + 1);
    for (size_t i = 0; i < length; ++i) text += static_cast<char>(payload[i]);

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, text);
    if (err) {
        Serial.printf("Core Text: %s\n", text.c_str());
        return;
    }

    const char* type = doc["type"] | "";
    const char* protocol = doc["protocol"] | "";

    if (!strcmp(type, "hello") || !strcmp(type, "hello_ack") || !strcmp(type, "welcome") || !strcmp(type, "ready")) {
        ready_ = true;
        const String detail = strlen(protocol) ? String(protocol) : String(JARVIS_PROTOCOL_NAME);
        Serial.printf("Core bereit: %s\n", detail.c_str());
        emit(VoiceEvent::Ready, detail);

        const char* minVersion = doc["minimum_client_version"].as<const char*>();
        const char* latestVersion = doc["latest_client_version"].as<const char*>();
        if (minVersion) Serial.printf("Min. Satellite-Version: %s\n", minVersion);
        if (latestVersion && strcmp(latestVersion, JARVIS_SATELLITE_VERSION) != 0) {
            Serial.printf("Update verfügbar: %s (installiert %s)\n", latestVersion, JARVIS_SATELLITE_VERSION);
        }
        return;
    }

    if (!strcmp(type, "transcript") || !strcmp(type, "stt") || !strcmp(type, "user_text")) {
        const String value = firstString(doc, "text", "transcript");
        Serial.printf("Du: %s\n", value.c_str());
        emit(VoiceEvent::Transcript, value);
        return;
    }

    if (!strcmp(type, "assistant") || !strcmp(type, "assistant_text") || !strcmp(type, "response")) {
        const String value = firstString(doc, "text", "response");
        Serial.printf("Jarvis: %s\n", value.c_str());
        emit(VoiceEvent::Assistant, value);
        return;
    }

    if (!strcmp(type, "tts_start") || !strcmp(type, "audio_output_start")) {
        Serial.println("TTS Wiedergabe ...");
        emit(VoiceEvent::TtsStart);
        return;
    }

    if (!strcmp(type, "tts_end") || !strcmp(type, "audio_output_end")) {
        Serial.println("TTS beendet.");
        emit(VoiceEvent::TtsEnd);
        return;
    }

    if (!strcmp(type, "error")) {
        String message = firstString(doc, "message", "detail");
        if (!message.length()) message = "Unbekannter Fehler";
        Serial.printf("Core Fehler: %s\n", message.c_str());
        emit(VoiceEvent::Error, message);
        return;
    }

    Serial.printf("Core Event: %s\n", text.c_str());
}

void VoiceProtocol::sendAudioStart() {
#if JARVIS_PROTOCOL_STRUCTURED
    JsonDocument doc;
    doc["type"] = "audio_start";
    doc["satellite_id"] = JARVIS_SATELLITE_ID;
    doc["format"] = "pcm_s16le";
    doc["sample_rate"] = JARVIS_AUDIO_RATE;
    doc["channels"] = JARVIS_AUDIO_CHANNELS;
    String out;
    serializeJson(doc, out);
    sendJson(out);
#endif
}

bool VoiceProtocol::sendAudio(const int16_t* samples, size_t count) {
    if (!connected_ || !samples || count == 0) return false;
    return ws_.sendBIN(reinterpret_cast<const uint8_t*>(samples), count * sizeof(int16_t));
}

void VoiceProtocol::sendAudioEnd() {
#if JARVIS_PROTOCOL_STRUCTURED
    JsonDocument doc;
    doc["type"] = "audio_end";
    doc["satellite_id"] = JARVIS_SATELLITE_ID;
    String out;
    serializeJson(doc, out);
    sendJson(out);
#endif
}

void VoiceProtocol::sendPing() {
    if (!connected_) return;
    JsonDocument doc;
    doc["type"] = "ping";
    doc["satellite_id"] = JARVIS_SATELLITE_ID;
    String out;
    serializeJson(doc, out);
    sendJson(out);
}

void VoiceProtocol::sendJson(const String& json) {
    if (connected_) ws_.sendTXT(json);
}
