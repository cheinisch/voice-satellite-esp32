#include "waveshare_185c_board.h"
#include "waveshare_185c_pins.h"
#include <Wire.h>

bool Waveshare185CBoard::begin() {
    Serial.println("Waveshare Init: I2C ...");
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
    Serial.println("Waveshare 1.85C V2: Touch CENTER = sprechen/stoppen");
    Serial.println("Waveshare 1.85C V2: Touch MIC links = stumm/zuhoeren");
    Serial.println("Waveshare 1.85C V2: Touch +/- rechts = Lautstaerke, NET = Netzwerkdetails");
    Serial.println("Waveshare 1.85C V2: Media-Controls: ⏮ ⏸/▶ ⏭ (unten, bei aktiver Wiedergabe)");
    Serial.println("Waveshare 1.85C V2: BOOT = Display an/aus; RESET bleibt Hardware-Reset");
    return true;
}

void Waveshare185CBoard::loop() {
    display_.loop();
    const uint32_t now = millis();

    Waveshare185CTouchPoint point;
    if (display_.displayEnabled() && touch_.consumeTap(point)) {
        Serial.printf("Touch: x=%u y=%u\n", point.x, point.y);

        if (display_.mediaScreenActive()) {
            // ── Media Screen — eigene Touch-Logik ───────────────────────────
            // VOL– / VOL+ funktionieren auch hier (gleiche Geometrie).
            // NET-Button und Voice-Controls sind gesperrt.
            constexpr int MS_VOLDOWN_X = 90,  MS_VOLUP_X = 270;
            constexpr int MS_VOL_Y     = 285, MS_VOL_HIT_R = 42;

            const int dxD = static_cast<int>(point.x) - MS_VOLDOWN_X;
            const int dyD = static_cast<int>(point.y) - MS_VOL_Y;
            const int dxU = static_cast<int>(point.x) - MS_VOLUP_X;
            const bool hitVolDown = dxD*dxD + dyD*dyD <= MS_VOL_HIT_R*MS_VOL_HIT_R;
            const bool hitVolUp   = dxU*dxU + dyD*dyD <= MS_VOL_HIT_R*MS_VOL_HIT_R;

            if (hitVolDown) {
                if (lastVolumeActionAt_ == 0 || now - lastVolumeActionAt_ >= 300) {
                    lastVolumeActionAt_ = now;
                    const uint8_t current = audio_.volume();
                    const uint8_t next = current <= 10 ? 0 : static_cast<uint8_t>(current - 10);
                    if (audio_.setVolume(next)) display_.setVolumePercent(next);
                }
            } else if (hitVolUp) {
                if (lastVolumeActionAt_ == 0 || now - lastVolumeActionAt_ >= 300) {
                    lastVolumeActionAt_ = now;
                    const uint8_t current = audio_.volume();
                    const uint8_t next = current >= 90 ? 100 : static_cast<uint8_t>(current + 10);
                    if (audio_.setVolume(next)) display_.setVolumePercent(next);
                }
            } else if (display_.hitMediaPlayPause(point.x, point.y)) {
                mediaPlayPause_ = true;
                Serial.printf("[BTN] PLAY/PAUSE  x=%u y=%u\n", point.x, point.y);
            } else if (display_.hitMediaPrev(point.x, point.y)) {
                mediaPrev_ = true;
                Serial.printf("[BTN] PREV        x=%u y=%u\n", point.x, point.y);
            } else if (display_.hitMediaNext(point.x, point.y)) {
                mediaNext_ = true;
                Serial.printf("[BTN] NEXT        x=%u y=%u\n", point.x, point.y);
            } else if (display_.hitMediaStop(point.x, point.y)) {
                mediaStop_ = true;
                Serial.printf("[BTN] STOP        x=%u y=%u\n", point.x, point.y);
            } else if (display_.mediaScreenActive()) {
                // Touch auf Media-Screen aber kein Button getroffen — hilft bei Kalibrierung
                Serial.printf("[BTN] miss (media screen) x=%u y=%u\n", point.x, point.y);
            }

        } else if (display_.networkPopupVisible()) {
            // ── Network-Popup fängt alle Touches ab ─────────────────────────
            if (display_.hitNetworkCloseButton(point.x, point.y) ||
                display_.hitNetworkButton(point.x, point.y)) {
                display_.toggleNetworkPopup();
            }

        } else {
            // ── Standard Voice-Dashboard ─────────────────────────────────────
            if (display_.hitNetworkButton(point.x, point.y)) {
                display_.toggleNetworkPopup();

            } else if (display_.hitVolumeDown(point.x, point.y)) {
                if (lastVolumeActionAt_ == 0 || now - lastVolumeActionAt_ >= 300) {
                    lastVolumeActionAt_ = now;
                    const uint8_t current = audio_.volume();
                    const uint8_t next = current <= 10 ? 0 : static_cast<uint8_t>(current - 10);
                    if (audio_.setVolume(next)) display_.setVolumePercent(next);
                }
            } else if (display_.hitVolumeUp(point.x, point.y)) {
                if (lastVolumeActionAt_ == 0 || now - lastVolumeActionAt_ >= 300) {
                    lastVolumeActionAt_ = now;
                    const uint8_t current = audio_.volume();
                    const uint8_t next = current >= 90 ? 100 : static_cast<uint8_t>(current + 10);
                    if (audio_.setVolume(next)) display_.setVolumePercent(next);
                }
            } else if (display_.hitMicButton(point.x, point.y)) {
                muteToggle_ = true;
            } else if (display_.hitCenterRecordButton(point.x, point.y)) {
                trigger_ = true;
            }
        }
    }

    const bool pressed = digitalRead(waveshare185c::BOOT_BUTTON) == LOW;
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
    return {.microphone=true, .speaker=true, .display=true,
            .touch=true, .buttons=true, .psram=true, .sdcard=true};
}

bool Waveshare185CBoard::consumeVoiceTrigger() {
    const bool v = trigger_; trigger_ = false; return v;
}
bool Waveshare185CBoard::consumeMuteToggle() {
    const bool v = muteToggle_; muteToggle_ = false; return v;
}
bool Waveshare185CBoard::consumeMediaPlayPause() {
    const bool v = mediaPlayPause_; mediaPlayPause_ = false; return v;
}
bool Waveshare185CBoard::consumeMediaPrev() {
    const bool v = mediaPrev_; mediaPrev_ = false; return v;
}
bool Waveshare185CBoard::consumeMediaNext() {
    const bool v = mediaNext_; mediaNext_ = false; return v;
}
bool Waveshare185CBoard::consumeMediaStop() {
    const bool v = mediaStop_; mediaStop_ = false; return v;
}

void Waveshare185CBoard::setDisplayName(const String& name) {
    display_.setDisplayName(name);
}
void Waveshare185CBoard::setState(SatelliteState state, const String& detail) {
    display_.showState(state, detail);
}
void Waveshare185CBoard::showTranscript(const String& text) { display_.showTranscript(text); }
void Waveshare185CBoard::showAssistant(const String& text)  { display_.showAssistant(text);  }