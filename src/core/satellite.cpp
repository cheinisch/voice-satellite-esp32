#include "core/satellite.h"
#include "build_info.h"
#include "jarvis_config.h"
#include <WiFi.h>

namespace {
constexpr size_t CHUNK_SAMPLES = (JARVIS_AUDIO_RATE * JARVIS_AUDIO_CHUNK_MS) / 1000;
static_assert(CHUNK_SAMPLES > 0, "audio chunk must contain samples");
int16_t audioChunk[CHUNK_SAMPLES];
}

Satellite::Satellite(Board& board) : board_(board) {}

bool Satellite::begin() {
    board_.setState(SatelliteState::Booting, "Initialisiere Hardware");
    if (!board_.begin()) {
        board_.setState(SatelliteState::Error, "Board-Initialisierung fehlgeschlagen");
        return false;
    }

    protocol_.setBinaryHandler([this](const uint8_t* data, size_t length) {
        if (length < sizeof(int16_t)) return;
        board_.audio().writePcm16(reinterpret_cast<const int16_t*>(data), length / sizeof(int16_t));
    });
    protocol_.setEventHandler([this](VoiceEvent event, const String& text) {
        onVoiceEvent(event, text);
    });

    board_.setState(SatelliteState::ConnectingWifi, "WLAN");
    const bool wifiOk = wifi_.begin();
    if (wifiOk) ensureProtocol();
    return true;
}

void Satellite::ensureProtocol() {
    if (!wifi_.connected() || protocolStarted_) return;
    board_.setState(SatelliteState::ConnectingCore, "Jarvis Core");
    protocol_.begin(board_);
    protocolStarted_ = true;
}

void Satellite::startRecording() {
    if (recording_ || !protocol_.ready()) return;
    board_.audio().clearOutput();
    recording_ = true;
    recordingStartedAt_ = millis();
    protocol_.sendAudioStart();
    board_.setState(SatelliteState::Listening, "Sprich jetzt");
    Serial.printf("Aufnahme läuft (%lus)...\n", static_cast<unsigned long>(JARVIS_RECORD_MS / 1000));
}

void Satellite::stopRecording() {
    if (!recording_) return;
    recording_ = false;
    protocol_.sendAudioEnd();
    Serial.println("Sende Daten an Jarvis");
    board_.setState(SatelliteState::Processing, "Sende Daten an Jarvis");
}

void Satellite::pumpRecording() {
    if (!recording_) return;
    const size_t got = board_.audio().readPcm16(audioChunk, CHUNK_SAMPLES, 50);
    if (got > 0) protocol_.sendAudio(audioChunk, got);
    if (millis() - recordingStartedAt_ >= JARVIS_RECORD_MS) stopRecording();
}

void Satellite::onVoiceEvent(VoiceEvent event, const String& text) {
    switch (event) {
        case VoiceEvent::Connected:
            board_.setState(SatelliteState::ConnectingCore, "Handshake");
            break;
        case VoiceEvent::Disconnected:
            if (recording_) recording_ = false;
            board_.setState(SatelliteState::ConnectingCore, "Reconnect");
            break;
        case VoiceEvent::Ready:
            board_.setState(SatelliteState::Ready, "Bereit");
            break;
        case VoiceEvent::Transcript:
            board_.showTranscript(text);
            break;
        case VoiceEvent::Assistant:
            board_.showAssistant(text);
            break;
        case VoiceEvent::TtsStart:
            board_.setState(SatelliteState::Speaking, "Jarvis spricht");
            break;
        case VoiceEvent::TtsEnd:
            board_.setState(SatelliteState::Ready, "Bereit");
            break;
        case VoiceEvent::Error:
            board_.setState(SatelliteState::Error, text);
            break;
    }
}

void Satellite::loop() {
    board_.loop();
    wifi_.loop();
    ensureProtocol();

    if (protocolStarted_) protocol_.loop();

    if (board_.consumeVoiceTrigger()) {
        if (recording_) stopRecording();
        else startRecording();
    }

    pumpRecording();

    if (millis() - lastStatusAt_ >= 30000) {
        lastStatusAt_ = millis();
        Serial.printf("Status: WLAN=%s Core=%s Ready=%s Heap=%u PSRAM=%u\n",
                      wifi_.connected() ? "OK" : "OFF",
                      protocol_.connected() ? "OK" : "OFF",
                      protocol_.ready() ? "ja" : "nein",
                      ESP.getFreeHeap(),
                      ESP.getFreePsram());
    }

    delay(recording_ ? 1 : 4);
}
