#include "generic_esp32s3_board.h"
#include "ai-voice-satellite_config.h"

namespace {
const char* stateName(SatelliteState state) {
    switch (state) {
        case SatelliteState::Booting: return "BOOT";
        case SatelliteState::ConnectingWifi: return "WIFI";
        case SatelliteState::ConnectingCore: return "CORE";
        case SatelliteState::Ready: return "READY";
        case SatelliteState::Listening: return "LISTENING";
        case SatelliteState::Processing: return "PROCESSING";
        case SatelliteState::Speaking: return "SPEAKING";
        case SatelliteState::Muted: return "MUTED";
        case SatelliteState::Error: return "ERROR";
    }
    return "?";
}
}

bool GenericEsp32S3Board::begin() {
    pinMode(AIVOICE-SATELLITE_GENERIC_BUTTON_PIN, INPUT_PULLUP);
    if (!audio_.begin()) return false;
    Serial.println("Generic ESP32-S3: BOOT-Taste = sprechen/stoppen");
    return true;
}

void GenericEsp32S3Board::loop() {
    const bool pressed = digitalRead(AIVOICE-SATELLITE_GENERIC_BUTTON_PIN) == LOW;
    const uint32_t now = millis();
    if (pressed != lastButton_ && now - lastEdgeAt_ > 30) {
        lastEdgeAt_ = now;
        lastButton_ = pressed;
        if (pressed) trigger_ = true;
    }
}

BoardCapabilities GenericEsp32S3Board::capabilities() const {
    return {.microphone=true, .speaker=true, .display=false, .touch=false, .buttons=true, .psram=false, .sdcard=false};
}

bool GenericEsp32S3Board::consumeVoiceTrigger() {
    const bool value = trigger_;
    trigger_ = false;
    return value;
}

void GenericEsp32S3Board::setState(SatelliteState state, const String& detail) {
    Serial.printf("[%s] %s\n", stateName(state), detail.c_str());
}
void GenericEsp32S3Board::showTranscript(const String& text) { Serial.printf("[Display:N/A] Du: %s\n", text.c_str()); }
void GenericEsp32S3Board::showAssistant(const String& text) { Serial.printf("[Display:N/A] Ai-Voice-Satellite: %s\n", text.c_str()); }
