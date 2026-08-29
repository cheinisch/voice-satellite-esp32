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

const char* normalizedTtsQuality() {
    const String configured = String(JARVIS_TTS_QUALITY);
    if (!configured.length()) return "low";

    if (configured.equalsIgnoreCase("low")) return "low";
    if (configured.equalsIgnoreCase("medium") || configured.equalsIgnoreCase("balanced")) {
        return "balanced";
    }
    if (configured.equalsIgnoreCase("high")) return "high";

    Serial.printf(
        "WARNUNG: Unbekannte JARVIS_TTS_QUALITY '%s'; verwende low.\n",
        configured.c_str()
    );
    return "low";
}
}

void VoiceProtocol::begin(const Board& board) {
    board_ = &board;

    // ESP32 clients can send an Authorization header during the HTTP WebSocket
    // upgrade. Keep the token out of the protocol payload and out of logs.
    if (strlen(JARVIS_CORE_TOKEN) > 0 && strcmp(JARVIS_CORE_TOKEN, "CHANGE_ME") != 0 && strcmp(JARVIS_CORE_TOKEN, "jv_DEIN_TOKEN") != 0) {
        authorizationHeader_ = "Authorization: Bearer ";
        authorizationHeader_ += JARVIS_CORE_TOKEN;
        ws_.setExtraHeaders(authorizationHeader_.c_str());
        Serial.println("Core-Authentifizierung: Bearer-Token konfiguriert");
    } else {
        authorizationHeader_ = "";
        ws_.setExtraHeaders("");
        Serial.println("WARNUNG: Kein Jarvis Core Token konfiguriert.");
    }

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
#if JARVIS_SEND_HELLO
            sendHello();
#endif
            break;
        case WStype_DISCONNECTED:
            connected_ = false;
            ready_ = false;
            binaryFragmentActive_ = false;
            Serial.println("Core getrennt; Reconnect läuft ...");
            emit(VoiceEvent::Disconnected);
            break;
        case WStype_TEXT:
            handleText(payload, length);
            break;
        case WStype_BIN:
            if (binaryHandler_) binaryHandler_(payload, length);
            break;
        case WStype_FRAGMENT_BIN_START:
            binaryFragmentActive_ = true;
            if (binaryHandler_) binaryHandler_(payload, length);
            break;
        case WStype_FRAGMENT:
            if (binaryFragmentActive_ && binaryHandler_) binaryHandler_(payload, length);
            break;
        case WStype_FRAGMENT_FIN:
            if (binaryFragmentActive_ && binaryHandler_) binaryHandler_(payload, length);
            binaryFragmentActive_ = false;
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

void VoiceProtocol::updateTtsFormat(JsonDocument& doc) {
    uint32_t rate = 0;
    uint32_t channels = 0;
    uint32_t bits = 0;

    if (!doc["sample_rate"].isNull()) rate = doc["sample_rate"].as<uint32_t>();
    if (!rate && !doc["sample_rate_hz"].isNull()) rate = doc["sample_rate_hz"].as<uint32_t>();
    if (!rate && !doc["rate"].isNull()) rate = doc["rate"].as<uint32_t>();
    if (!doc["channels"].isNull()) channels = doc["channels"].as<uint32_t>();
    if (!doc["bits_per_sample"].isNull()) bits = doc["bits_per_sample"].as<uint32_t>();
    if (!bits && !doc["bits"].isNull()) bits = doc["bits"].as<uint32_t>();

    JsonObject audio = doc["audio"].as<JsonObject>();
    if (!audio.isNull()) {
        if (!rate && !audio["sample_rate"].isNull()) rate = audio["sample_rate"].as<uint32_t>();
        if (!rate && !audio["sample_rate_hz"].isNull()) rate = audio["sample_rate_hz"].as<uint32_t>();
        if (!channels && !audio["channels"].isNull()) channels = audio["channels"].as<uint32_t>();
        if (!bits && !audio["bits_per_sample"].isNull()) bits = audio["bits_per_sample"].as<uint32_t>();
    }

    if (rate >= 8000 && rate <= 96000) ttsSampleRate_ = rate;
    if (channels >= 1 && channels <= 2) ttsChannels_ = static_cast<uint8_t>(channels);
    if (bits == 16) ttsBitsPerSample_ = 16;
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
        const bool wasReady = ready_;
        ready_ = true;
        const String detail = strlen(protocol) ? String(protocol) : String(JARVIS_PROTOCOL_NAME);
        if (!wasReady) {
            Serial.printf("Core bereit: %s\n", detail.c_str());
            emit(VoiceEvent::Ready, detail);
        }

        const char* minVersion = doc["minimum_client_version"].as<const char*>();
        const char* latestVersion = doc["latest_client_version"].as<const char*>();
        if (minVersion) Serial.printf("Min. Satellite-Version: %s\n", minVersion);
        if (latestVersion && strcmp(latestVersion, JARVIS_SATELLITE_VERSION) != 0) {
            Serial.printf("Update verfügbar: %s (installiert %s)\n", latestVersion, JARVIS_SATELLITE_VERSION);
        }
        return;
    }

    if (!strcmp(type, "session.started")) {
        Serial.println("Voice Session gestartet.");
        return;
    }

    if (!strcmp(type, "transcript.partial")) {
        const String value = firstString(doc, "text", "transcript");
        if (value.length()) Serial.printf("STT partial: %s\n", value.c_str());
        return;
    }

    if (!strcmp(type, "transcript.final") || !strcmp(type, "transcript") || !strcmp(type, "stt") || !strcmp(type, "user_text")) {
        const String value = firstString(doc, "text", "transcript");
        Serial.printf("Du: %s\n", value.c_str());
        emit(VoiceEvent::Transcript, value);
        return;
    }

    if (!strcmp(type, "assistant.final") || !strcmp(type, "assistant") || !strcmp(type, "assistant_text") || !strcmp(type, "response")) {
        const String value = firstString(doc, "text", "response");
        Serial.printf("Jarvis: %s\n", value.c_str());
        emit(VoiceEvent::Assistant, value);
        return;
    }

    if (!strcmp(type, "tts_start") || !strcmp(type, "tts.start") || !strcmp(type, "tts.started") ||
        !strcmp(type, "audio_output_start") || !strcmp(type, "audio.output.start") ||
        !strcmp(type, "response.audio.start")) {
        updateTtsFormat(doc);
        Serial.printf("TTS Wiedergabe ... (%lu Hz, %u Kanal/Kanäle, %u Bit)\n",
                      static_cast<unsigned long>(ttsSampleRate_), ttsChannels_, ttsBitsPerSample_);
        emit(VoiceEvent::TtsStart);
        return;
    }

    if (!strcmp(type, "tts_end") || !strcmp(type, "tts.end") || !strcmp(type, "tts.finished") ||
        !strcmp(type, "audio_output_end") || !strcmp(type, "audio.output.end") ||
        !strcmp(type, "response.audio.done")) {
        Serial.println("TTS beendet.");
        emit(VoiceEvent::TtsEnd);
        return;
    }

    if (!strcmp(type, "reset")) {
        Serial.println("Voice Session zurückgesetzt.");
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

void VoiceProtocol::sendSessionStart(bool autoTts) {
    JsonDocument doc;
    doc["type"] = "session.start";
    doc["language"] = "de";
    doc["auto_chat"] = true;
    doc["auto_tts"] = autoTts;
    doc["content_type"] = "audio/wav";

    // ESP32 defaults to the fast/compact low profile. local_config.h may select
    // low, medium or high. The Core API calls the middle tier "balanced", so
    // "medium" is normalized before it is sent.
    const char* ttsQuality = normalizedTtsQuality();
    JsonObject tts = doc["tts"].to<JsonObject>();
    tts["stream"] = true;
    tts["quality"] = ttsQuality;
    tts["chunk_bytes"] = 12U * 1024U;

    // Keep frames below arduinoWebSockets' stock ESP32 receive limit (15 KiB).
    // Older Core versions ignore these hints; streaming-capable Core versions
    // use them to split TTS audio into ESP-friendly chunks.
    doc["client_max_binary_frame_bytes"] = 14U * 1024U;
    doc["preferred_tts_chunk_bytes"] = 12U * 1024U;

    Serial.printf(
        "TTS Profil: %s (Core quality=%s, Streaming=ja, Chunk=%u Bytes)\n",
        JARVIS_TTS_QUALITY,
        ttsQuality,
        12U * 1024U
    );

    String out;
    serializeJson(doc, out);
    sendJson(out);
}

bool VoiceProtocol::sendWav(const uint8_t* data, size_t length) {
    if (!connected_ || !data || length == 0) return false;
    return ws_.sendBIN(const_cast<uint8_t*>(data), length);
}

void VoiceProtocol::sendAudioCommit() {
    JsonDocument doc;
    doc["type"] = "audio.commit";
    String out;
    serializeJson(doc, out);
    sendJson(out);
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
    if (connected_) {
        ws_.sendTXT(json.c_str(), json.length());
    }
}
