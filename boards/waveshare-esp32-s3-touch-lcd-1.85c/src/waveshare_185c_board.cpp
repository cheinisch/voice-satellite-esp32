#include "waveshare_185c_board.h"
#include "waveshare_185c_pins.h"
#include <Wire.h>

bool Waveshare185CBoard::begin() {
    Serial.println("Waveshare Init: I2C ...");
    // This is the single owner/initialization point for the Waveshare shared
    // I2C bus. Audio codecs must reuse Wire without reinitializing it.
    if (!Wire.begin(waveshare185c::I2C_SDA, waveshare185c::I2C_SCL, 100000)) {
        Serial.println("Waveshare I2C-Bus konnte nicht initialisiert werden.");
        return false;
    }
    Wire.setTimeOut(50);

    Serial.println("Waveshare Init: TCA9554 ...");
    if (!expander_.begin(Wire, waveshare185c::TCA9554_ADDR)) {
        Serial.println("TCA9554 nicht erreichbar.");
        return false;
    }

    Serial.println("Waveshare Init: Display ...");
    if (!display_.begin(expander_)) {
        Serial.println("ST77916 Display-Initialisierung fehlgeschlagen.");
        return false;
    }

    Serial.println("Waveshare Init: Touch ...");
    if (!touch_.begin(Wire, expander_)) {
        Serial.println("CST816 Touch-Initialisierung fehlgeschlagen.");
        return false;
    }

    Serial.println("Waveshare Init: Audio/Mikrofon ...");
    if (!audio_.begin()) {
        display_.showState(SatelliteState::Error, "Audio init fehlgeschlagen");
        return false;
    }

    Serial.println("Waveshare Init: Audio/Mikrofon OK");
    pinMode(waveshare185c::BOOT_BUTTON, INPUT_PULLUP);
    display_.setVolumePercent(audio_.volume());
    display_.showState(SatelliteState::Ready, "Bereit");
    Serial.println("Waveshare 1.85C V2: Touch AUFNEHMEN = sprechen/stoppen");
    Serial.println("Waveshare 1.85C V2: Touch STUMM/ZUHOEREN = Mikrofon umschalten");
    Serial.println("Waveshare 1.85C V2: Touch +/- rechts = Lautstaerke, NET = Netzwerkdetails");
    Serial.println("Waveshare 1.85C V2: BOOT = Display an/aus; RESET bleibt Hardware-Reset");
    return true;
}

void Waveshare185CBoard::loop() {
    display_.loop();

    Waveshare185CTouchPoint point;
    if (display_.displayEnabled() && touch_.consumeTap(point)) {
        Serial.printf("Touch: x=%u y=%u\n", point.x, point.y);

        if (display_.networkPopupVisible()) {
            if (display_.hitNetworkCloseButton(point.x, point.y) ||
                display_.hitNetworkButton(point.x, point.y)) {
                display_.toggleNetworkPopup();
            }
        } else if (display_.hitNetworkButton(point.x, point.y)) {
            display_.toggleNetworkPopup();
        } else if (display_.hitVolumeDown(point.x, point.y)) {
            const uint8_t current = audio_.volume();
            const uint8_t next = current <= 10 ? 0 : static_cast<uint8_t>(current - 10);
            if (audio_.setVolume(next)) display_.setVolumePercent(next);
        } else if (display_.hitVolumeUp(point.x, point.y)) {
            const uint8_t current = audio_.volume();
            const uint8_t next = current >= 90 ? 100 : static_cast<uint8_t>(current + 10);
            if (audio_.setVolume(next)) display_.setVolumePercent(next);
        } else if (display_.hitMuteButton(point.x, point.y)) {
            muteToggle_ = true;
        } else if (display_.hitRecordButton(point.x, point.y)) {
            trigger_ = true;
        }
    }

    const bool pressed = digitalRead(waveshare185c::BOOT_BUTTON) == LOW;
    const uint32_t now = millis();
    if (pressed != lastBoot_ && now - lastBootEdgeAt_ > 30) {
        lastBootEdgeAt_ = now;
        lastBoot_ = pressed;
        if (pressed) {
            display_.toggleDisplay();
            Serial.printf("BOOT: Display %s.\n", display_.displayEnabled() ? "an" : "aus");
        }
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

bool Waveshare185CBoard::consumeMuteToggle() {
    const bool value = muteToggle_;
    muteToggle_ = false;
    return value;
}

void Waveshare185CBoard::setState(SatelliteState state, const String& detail) {
    display_.showState(state, detail);
}
void Waveshare185CBoard::showTranscript(const String& text) { display_.showTranscript(text); }
void Waveshare185CBoard::showAssistant(const String& text) { display_.showAssistant(text); }
