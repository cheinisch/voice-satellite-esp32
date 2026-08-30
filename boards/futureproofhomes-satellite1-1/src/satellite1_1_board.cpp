#include "satellite1_1_board.h"
#include "satellite1_1_pins.h"

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

bool FutureProofHomesSatellite11Board::begin() {
    pinMode(SAT1_ACTION_BUTTON_PIN, INPUT_PULLUP);
    pinMode(SAT1_STATUS_LED_PIN, OUTPUT);
    digitalWrite(SAT1_STATUS_LED_PIN, LOW);

    Serial.println("Satellite1.1: initialisiere XMOS ...");
    if (!xmos_.begin()) {
        Serial.printf("Satellite1.1: XMOS nicht bereit (%s).\n", xmos_.version());
        Serial.println("Hinweis: XMOS muss zuvor mit der FutureProofHomes-Firmware provisioniert sein.");
        return false;
    }
    Serial.printf("Satellite1.1: XMOS %s\n", xmos_.version());

    if (!audio_.begin()) return false;
    Serial.println("Satellite1.1: Action-Taste = sprechen/stoppen");
    Serial.println("Satellite1.1: 48-kHz-XMOS-Audio wird lokal auf Voice Satellite PCM16/16kHz umgesetzt.");
    Serial.println("Satellite1.1: TAS2780/Line-Out Initialisierung ist in 0.1.x noch experimentell.");
    return true;
}

void FutureProofHomesSatellite11Board::loop() {
    const bool pressed = digitalRead(SAT1_ACTION_BUTTON_PIN) == LOW;
    const uint32_t now = millis();
    if (pressed != lastButton_ && now - lastEdgeAt_ > 30) {
        lastEdgeAt_ = now;
        lastButton_ = pressed;
        if (pressed) trigger_ = true;
    }
}

BoardCapabilities FutureProofHomesSatellite11Board::capabilities() const {
    return {.microphone=true, .speaker=true, .display=false, .touch=false, .buttons=true, .psram=true, .sdcard=false};
}

bool FutureProofHomesSatellite11Board::consumeVoiceTrigger() {
    const bool value = trigger_;
    trigger_ = false;
    return value;
}

void FutureProofHomesSatellite11Board::updateStatusLed(SatelliteState state) {
    const bool on = state == SatelliteState::Listening || state == SatelliteState::Processing ||
                    state == SatelliteState::Speaking || state == SatelliteState::Muted || state == SatelliteState::Error;
    digitalWrite(SAT1_STATUS_LED_PIN, on ? HIGH : LOW);
}

void FutureProofHomesSatellite11Board::setState(SatelliteState state, const String& detail) {
    updateStatusLed(state);
    Serial.printf("[SAT1.1/%s] %s\n", stateName(state), detail.c_str());
}

void FutureProofHomesSatellite11Board::showTranscript(const String& text) {
    Serial.printf("[Satellite1.1] Du: %s\n", text.c_str());
}

void FutureProofHomesSatellite11Board::showAssistant(const String& text) {
    Serial.printf("[Satellite1.1] Voice Satellite: %s\n", text.c_str());
}
