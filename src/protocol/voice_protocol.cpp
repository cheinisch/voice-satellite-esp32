#include "protocol/voice_protocol.h"
#include "build_info.h"
#include "voice_satellite_config.h"
#include "voice_satellite_media_playback.h"
#include "waveshare_185c_board.h"
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
    const String configured = String(VOICE_SATELLITE_TTS_QUALITY);
    if (!configured.length()) return "low";

    if (configured.equalsIgnoreCase("low")) return "low";
    if (configured.equalsIgnoreCase("medium") || configured.equalsIgnoreCase("balanced")) {
        return "balanced";
    }
    if (configured.equalsIgnoreCase("high")) return "high";

    Serial.printf(
        "WARNUNG: Unbekannte VOICE_SATELLITE_TTS_QUALITY '%s'; verwende low.\n",
        configured.c_str()
    );
    return "low";
}
}

void VoiceProtocol::begin(Board& board) {
    board_ = &board;

    // ESP32 clients can send an Authorization header during the HTTP WebSocket
    // upgrade. Keep the token out of the protocol payload and out of logs.
    if (strlen(VOICE_SATELLITE_CORE_TOKEN) > 0 && strcmp(VOICE_SATELLITE_CORE_TOKEN, "CHANGE_ME") != 0 && strcmp(VOICE_SATELLITE_CORE_TOKEN, "jv_DEIN_TOKEN") != 0) {
        authorizationHeader_ = "Authorization: Bearer ";
        authorizationHeader_ += VOICE_SATELLITE_CORE_TOKEN;
        ws_.setExtraHeaders(authorizationHeader_.c_str());
        Serial.println("Core-Authentifizierung: Bearer-Token konfiguriert");
    } else {
        authorizationHeader_ = "";
        ws_.setExtraHeaders("");
        Serial.println("WARNUNG: Kein Core Token konfiguriert.");
    }

#if VOICE_SATELLITE_CORE_TLS
    ws_.beginSSL(VOICE_SATELLITE_CORE_HOST, VOICE_SATELLITE_CORE_PORT, VOICE_SATELLITE_CORE_PATH, VOICE_SATELLITE_PROTOCOL_NAME);
#else
    ws_.begin(VOICE_SATELLITE_CORE_HOST, VOICE_SATELLITE_CORE_PORT, VOICE_SATELLITE_CORE_PATH, VOICE_SATELLITE_PROTOCOL_NAME);
#endif
    ws_.setReconnectInterval(VOICE_SATELLITE_RECONNECT_MS);
    ws_.enableHeartbeat(15000, 3000, 2);
    ws_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        onEvent(type, payload, length);
    });
}

void VoiceProtocol::loop() {
    ws_.loop();
    jarvisMediaLoop();

    if (connected_) {
        String mediaState;
        if (jarvisMediaPollState(mediaState)) {
            sendJson(mediaState);
        }

        // Poll media touch controls from the board and send commands to Core.
        if (board_) {
            auto* wb = static_cast<Waveshare185CBoard*>(board_);
            if (wb->consumeMediaPlayPause()) {
                JsonDocument cmd;
                cmd["type"] = "media.toggle";
                String out; serializeJson(cmd, out);
                sendJson(out);
                Serial.println("[media] Touch: play/pause -> Core");
            }
            if (wb->consumeMediaPrev()) {
                JsonDocument cmd;
                cmd["type"] = "media.previous";
                String out; serializeJson(cmd, out);
                sendJson(out);
                Serial.println("[media] Touch: prev -> Core");
            }
            if (wb->consumeMediaNext()) {
                JsonDocument cmd;
                cmd["type"] = "media.next";
                String out; serializeJson(cmd, out);
                sendJson(out);
                Serial.println("[media] Touch: next -> Core");
            }
        }
    }
}

void VoiceProtocol::emit(VoiceEvent event, const String& text) {
    if (eventHandler_) eventHandler_(event, text);
}

void VoiceProtocol::onEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            connected_ = true;
            ready_ = false;
            Serial.printf("Core verbunden: %s:%d%s\n", VOICE_SATELLITE_CORE_HOST, VOICE_SATELLITE_CORE_PORT, VOICE_SATELLITE_CORE_PATH);
            emit(VoiceEvent::Connected);
#if VOICE_SATELLITE_SEND_HELLO
            sendHello();
#elif defined(JARVIS_MEDIA_PLAYBACK) && JARVIS_MEDIA_PLAYBACK
            // Media outputs must advertise themselves to the Core.
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
    doc["protocol"] = VOICE_SATELLITE_PROTOCOL_NAME;
    doc["client"] = VOICE_SATELLITE_CLIENT_NAME;
    doc["client_version"] = VOICE_SATELLITE_VERSION;
    doc["client_build"] = VOICE_SATELLITE_BUILD;
    doc["satellite_id"] = VOICE_SATELLITE_ID;
    doc["satellite_name"] = VOICE_SATELLITE_NAME;
    doc["transport"] = "websocket";
    doc["platform"] = "esp32";

    if (board_) {
        doc["board_model"] = board_->model();
        doc["board_profile"] = board_->profile();
        doc["board"] = board_->profile();
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
    audio["sample_rate"] = VOICE_SATELLITE_AUDIO_RATE;
    audio["channels"] = VOICE_SATELLITE_AUDIO_CHANNELS;

    // Adds top-level features.media + media.formats/controls. The Core accepts
    // these fields on the existing hello message and registers this connection
    // as a media output.
    jarvisMediaAugmentCapabilities(doc);

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

    String mediaResponse;
    if (jarvisMediaHandleMessage(doc, mediaResponse)) {
        if (mediaResponse.length()) {
            sendJson(mediaResponse);
        }
        return;
    }

    const char* type = doc["type"] | "";

    if (!strcmp(type, "client.capabilities.accepted")) {
        const bool mediaAccepted = doc["media"] | false;
        Serial.printf("Media Capability: %s\n", mediaAccepted ? "akzeptiert" : "nicht aktiv");
        return;
    }

    if (!strcmp(type, "hello") || !strcmp(type, "hello_ack") || !strcmp(type, "welcome") || !strcmp(type, "ready")) {
        const bool wasReady = ready_;
        ready_ = true;

        const char* remoteDisplayName = doc["display_name"].as<const char*>();
        if ((!remoteDisplayName || !*remoteDisplayName) && !doc["server"].isNull()) {
            remoteDisplayName = doc["server"]["display_name"].as<const char*>();
        }
        if (remoteDisplayName && *remoteDisplayName) {
            displayName_ = String(remoteDisplayName);
            displayName_.trim();
            if (!displayName_.length()) displayName_ = "Voice Satellite";
        }

        const String detail = String(VOICE_SATELLITE_PROTOCOL_NAME);
        if (!wasReady) {
            Serial.printf("Core bereit: %s\n", detail.c_str());
            Serial.printf("Voice Satellite Anzeigename: %s\n", displayName_.c_str());
            emit(VoiceEvent::Ready, detail);
        }

        const char* minVersion = doc["minimum_client_version"].as<const char*>();
        const char* latestVersion = doc["latest_client_version"].as<const char*>();
        if (minVersion) Serial.printf("Min. Satellite-Version: %s\n", minVersion);
        if (latestVersion && strcmp(latestVersion, VOICE_SATELLITE_VERSION) != 0) {
            Serial.printf("Update verfügbar: %s (installiert %s)\n", latestVersion, VOICE_SATELLITE_VERSION);
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
        Serial.printf("Voice Satellite: %s\n", value.c_str());
        emit(VoiceEvent::Assistant, value);
        return;
    }

    if (!strcmp(type, "tts_start") || !strcmp(type, "tts.start") || !strcmp(type, "tts.started") ||
        !strcmp(type, "audio_output_start") || !strcmp(type, "audio.output.start") ||
        !strcmp(type, "response.audio.start")) {
        updateTtsFormat(doc);
        ttsAckRequired_ = doc["ack_required"] | false;
        ttsExpectedBytes_ = doc["length"] | 0U;
        ttsExpectedChunks_ = doc["chunks"] | 0U;
        Serial.printf("TTS Wiedergabe ... (%lu Hz, %u Kanal/Kanäle, %u Bit)\n",
                      static_cast<unsigned long>(ttsSampleRate_), ttsChannels_, ttsBitsPerSample_);
        if (ttsExpectedBytes_ > 0) {
            Serial.printf("TTS Transfer: erwartet %u Bytes in %lu Chunk(s), ACK=%s.\n",
                          static_cast<unsigned>(ttsExpectedBytes_),
                          static_cast<unsigned long>(ttsExpectedChunks_),
                          ttsAckRequired_ ? "ja" : "nein");
        }
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
    // The voice path gets the speaker/microphone resources before a new round.
    jarvisMediaInterruptForVoice();

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
    tts["ack"] = true;
    tts["ack_timeout_ms"] = 8000U;

    // Keep frames below arduinoWebSockets' stock ESP32 receive limit (15 KiB).
    // Older Core versions ignore these hints; streaming-capable Core versions
    // use them to split TTS audio into ESP-friendly chunks.
    doc["client_max_binary_frame_bytes"] = 14U * 1024U;
    doc["preferred_tts_chunk_bytes"] = 12U * 1024U;

    Serial.printf(
        "TTS Profil: %s (Core quality=%s, Streaming=ja, ACK=ja, Chunk=%u Bytes)\n",
        VOICE_SATELLITE_TTS_QUALITY,
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
    doc["satellite_id"] = VOICE_SATELLITE_ID;
    String out;
    serializeJson(doc, out);
    sendJson(out);
}

void VoiceProtocol::sendTtsAck(uint32_t sequence, size_t receivedBytes) {
    if (!connected_ || !ttsAckRequired_) return;
    JsonDocument doc;
    doc["type"] = "tts.ack";
    doc["sequence"] = sequence;
    doc["received_bytes"] = static_cast<uint32_t>(receivedBytes);
    String out;
    serializeJson(doc, out);
    sendJson(out);
}

void VoiceProtocol::sendJson(const String& json) {
    if (connected_) {
        ws_.sendTXT(json.c_str(), json.length());
    }
}