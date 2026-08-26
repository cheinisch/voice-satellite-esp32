#include "waveshare_185c_board.h"
#include "waveshare_185c_pins.h"
#include <Wire.h>

bool Waveshare185CBoard::begin() {
    // This is the single owner/initialization point for the Waveshare shared
    // I2C bus. Audio codecs must reuse Wire without reinitializing it.
    if (!Wire.begin(waveshare185c::I2C_SDA, waveshare185c::I2C_SCL, 100000)) {
        Serial.println("Waveshare I2C-Bus konnte nicht initialisiert werden.");
        return false;
    }
    Wire.setTimeOut(50);

    if (!expander_.begin(Wire, waveshare185c::TCA9554_ADDR)) {
        Serial.println("TCA9554 nicht erreichbar.");
        return false;
    }

    if (!display_.begin(expander_)) {
        Serial.println("ST77916 Display-Initialisierung fehlgeschlagen.");
        return false;
    }

    if (!touch_.begin(Wire, expander_)) {
        Serial.println("CST816 Touch-Initialisierung fehlgeschlagen.");
        return false;
    }

    if (!audio_.begin()) {
        display_.showState(SatelliteState::Error, "Audio init fehlgeschlagen");
        return false;
    }

    pinMode(waveshare185c::BOOT_BUTTON, INPUT_PULLUP);
    display_.showState(SatelliteState::Ready, "Touch oder BOOT");
    Serial.println("Waveshare 1.85C V2: Touch oder BOOT = sprechen/stoppen");
    return true;
}

void Waveshare185CBoard::loop() {
    if (touch_.consumeTap()) trigger_ = true;

    const bool pressed = digitalRead(waveshare185c::BOOT_BUTTON) == LOW;
    const uint32_t now = millis();
    if (pressed != lastBoot_ && now - lastBootEdgeAt_ > 30) {
        lastBootEdgeAt_ = now;
        lastBoot_ = pressed;
        if (pressed) trigger_ = true;
    }
}

BoardCapabilities Waveshare185CBoard::capabilities() const {
    return {.microphone=true, .speaker=true, .display=true, .touch=true, .buttons=true, .psram=true, .sdcard=true};
}

bool Waveshare185CBoard::consumeVoiceTrigger() {
    const bool value = trigger_;
    trigger_ = false;
    return value;
}

void Waveshare185CBoard::setState(SatelliteState state, const String& detail) {
    display_.showState(state, detail);
}
void Waveshare185CBoard::showTranscript(const String& text) { display_.showTranscript(text); }
void Waveshare185CBoard::showAssistant(const String& text) { display_.showAssistant(text); }
