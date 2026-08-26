#include "core/satellite.h"
#include "build_info.h"
#include "jarvis_config.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace {
constexpr size_t CHUNK_SAMPLES = (JARVIS_AUDIO_RATE * JARVIS_AUDIO_CHUNK_MS) / 1000;
constexpr size_t WAV_HEADER_BYTES = 44;
static_assert(CHUNK_SAMPLES > 0, "audio chunk must contain samples");
int16_t audioChunk[CHUNK_SAMPLES];

void putLe16(uint8_t* dst, uint16_t value) {
    dst[0] = static_cast<uint8_t>(value & 0xff);
    dst[1] = static_cast<uint8_t>((value >> 8) & 0xff);
}

void putLe32(uint8_t* dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>(value & 0xff);
    dst[1] = static_cast<uint8_t>((value >> 8) & 0xff);
    dst[2] = static_cast<uint8_t>((value >> 16) & 0xff);
    dst[3] = static_cast<uint8_t>((value >> 24) & 0xff);
}
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

    printConsoleHelp();
    return true;
}

void Satellite::ensureProtocol() {
    if (!wifi_.connected() || protocolStarted_) return;
    board_.setState(SatelliteState::ConnectingCore, "Jarvis Core");
    protocol_.begin(board_);
    protocolStarted_ = true;
}

bool Satellite::allocateRecordingBuffer() {
    freeRecordingBuffer();

    const size_t maxSamples = (static_cast<size_t>(JARVIS_AUDIO_RATE) * JARVIS_RECORD_MS) / 1000;
    recordingCapacityBytes_ = WAV_HEADER_BYTES + maxSamples * sizeof(int16_t);

    if (psramFound()) {
        recordingBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(recordingCapacityBytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (!recordingBuffer_) {
        recordingBuffer_ = static_cast<uint8_t*>(malloc(recordingCapacityBytes_));
    }
    if (!recordingBuffer_) {
        recordingCapacityBytes_ = 0;
        Serial.printf("Aufnahmefehler: %u Bytes Audiopuffer konnten nicht reserviert werden.\n",
                      static_cast<unsigned>(WAV_HEADER_BYTES + maxSamples * sizeof(int16_t)));
        return false;
    }

    recordingPcmBytes_ = 0;
    memset(recordingBuffer_, 0, WAV_HEADER_BYTES);
    Serial.printf("Audiopuffer: %u Bytes (%s)\n",
                  static_cast<unsigned>(recordingCapacityBytes_),
                  psramFound() ? "PSRAM bevorzugt" : "Heap");
    return true;
}

void Satellite::freeRecordingBuffer() {
    if (recordingBuffer_) {
        free(recordingBuffer_);
        recordingBuffer_ = nullptr;
    }
    recordingCapacityBytes_ = 0;
    recordingPcmBytes_ = 0;
}

void Satellite::finalizeWavHeader() {
    if (!recordingBuffer_) return;

    uint8_t* h = recordingBuffer_;
    memcpy(h + 0, "RIFF", 4);
    putLe32(h + 4, static_cast<uint32_t>(36 + recordingPcmBytes_));
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    putLe32(h + 16, 16);
    putLe16(h + 20, 1); // PCM
    putLe16(h + 22, JARVIS_AUDIO_CHANNELS);
    putLe32(h + 24, JARVIS_AUDIO_RATE);
    putLe32(h + 28, JARVIS_AUDIO_RATE * JARVIS_AUDIO_CHANNELS * sizeof(int16_t));
    putLe16(h + 32, JARVIS_AUDIO_CHANNELS * sizeof(int16_t));
    putLe16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    putLe32(h + 40, static_cast<uint32_t>(recordingPcmBytes_));
}

void Satellite::startRecording() {
    if (recording_ || !protocol_.ready()) return;
    if (!allocateRecordingBuffer()) {
        if (sttTestActive_) sttTestActive_ = false;
        board_.setState(SatelliteState::Error, "Kein Audiopuffer");
        return;
    }

    board_.audio().clearOutput();
    protocol_.sendSessionStart();
    recording_ = true;
    recordingStartedAt_ = millis();
    board_.setState(SatelliteState::Listening, "Sprich jetzt");
    if (sttTestActive_) {
        Serial.println();
        Serial.println("=== STT TEST ===");
        Serial.println("Sprich jetzt in das Mikrofon.");
    }
    Serial.printf("Aufnahme läuft (%lus)...\n", static_cast<unsigned long>(JARVIS_RECORD_MS / 1000));
}

void Satellite::stopRecording() {
    if (!recording_) return;
    recording_ = false;

    if (!recordingBuffer_ || recordingPcmBytes_ == 0) {
        Serial.println("STT: Keine Audiodaten aufgenommen.");
        freeRecordingBuffer();
        board_.setState(SatelliteState::Ready, "Bereit");
        if (sttTestActive_) sttTestActive_ = false;
        return;
    }

    finalizeWavHeader();
    const size_t wavBytes = WAV_HEADER_BYTES + recordingPcmBytes_;
    Serial.printf("Sende WAV an Jarvis: %u Bytes, %.2f s\n",
                  static_cast<unsigned>(wavBytes),
                  static_cast<double>(recordingPcmBytes_) / (JARVIS_AUDIO_RATE * JARVIS_AUDIO_CHANNELS * sizeof(int16_t)));

    if (!protocol_.sendWav(recordingBuffer_, wavBytes)) {
        Serial.println("STT: WAV konnte nicht gesendet werden.");
        freeRecordingBuffer();
        board_.setState(SatelliteState::Error, "Audio senden fehlgeschlagen");
        if (sttTestActive_) sttTestActive_ = false;
        return;
    }

    protocol_.sendAudioCommit();
    Serial.println("Sende Daten an Jarvis");
    freeRecordingBuffer();
    board_.setState(SatelliteState::Processing, "Sende Daten an Jarvis");
}

void Satellite::pumpRecording() {
    if (!recording_) return;
    const size_t got = board_.audio().readPcm16(audioChunk, CHUNK_SAMPLES, 50);
    if (got > 0 && recordingBuffer_) {
        const size_t bytes = got * sizeof(int16_t);
        const size_t used = WAV_HEADER_BYTES + recordingPcmBytes_;
        const size_t available = recordingCapacityBytes_ > used ? recordingCapacityBytes_ - used : 0;
        const size_t copyBytes = bytes < available ? bytes : available;
        if (copyBytes > 0) {
            memcpy(recordingBuffer_ + WAV_HEADER_BYTES + recordingPcmBytes_, audioChunk, copyBytes);
            recordingPcmBytes_ += copyBytes;
        }
        if (copyBytes < bytes || WAV_HEADER_BYTES + recordingPcmBytes_ >= recordingCapacityBytes_) {
            stopRecording();
            return;
        }
    }
    if (millis() - recordingStartedAt_ >= JARVIS_RECORD_MS) stopRecording();
}

void Satellite::onVoiceEvent(VoiceEvent event, const String& text) {
    switch (event) {
        case VoiceEvent::Connected:
            board_.setState(SatelliteState::ConnectingCore, "Handshake");
            break;
        case VoiceEvent::Disconnected:
            if (recording_) recording_ = false;
            freeRecordingBuffer();
            board_.setState(SatelliteState::ConnectingCore, "Reconnect");
            break;
        case VoiceEvent::Ready:
            board_.setState(SatelliteState::Ready, "Bereit");
            break;
        case VoiceEvent::Transcript:
            board_.showTranscript(text);
            if (sttTestActive_) {
                Serial.println("------------------------------");
                Serial.printf("STT Ergebnis: %s\n", text.c_str());
                Serial.println("STT Test: erfolgreich empfangen");
                Serial.println("==============================");
                sttTestActive_ = false;
            }
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
            if (sttTestActive_) {
                Serial.printf("STT Test fehlgeschlagen: %s\n", text.c_str());
                sttTestActive_ = false;
            }
            board_.setState(SatelliteState::Error, text);
            break;
    }
}


void Satellite::runMicTest() {
    if (recording_) {
        Serial.println("MIC Test nicht möglich: Aufnahme läuft bereits.");
        return;
    }

    Serial.println();
    Serial.println("=== MIC TEST ===");
    Serial.println("Lese 2 Sekunden direkt vom Board-Audioeingang ...");

    constexpr uint32_t TEST_MS = 2000;
    uint32_t started = millis();
    uint32_t blocks = 0;
    uint32_t samples = 0;
    int16_t minSample = 32767;
    int16_t maxSample = -32768;
    uint64_t sumAbs = 0;
    uint32_t nonZero = 0;

    while (millis() - started < TEST_MS) {
        const size_t got = board_.audio().readPcm16(audioChunk, CHUNK_SAMPLES, 100);
        if (!got) {
            delay(1);
            continue;
        }
        ++blocks;
        samples += static_cast<uint32_t>(got);
        for (size_t i = 0; i < got; ++i) {
            const int16_t v = audioChunk[i];
            if (v < minSample) minSample = v;
            if (v > maxSample) maxSample = v;
            const int32_t av = v < 0 ? -static_cast<int32_t>(v) : static_cast<int32_t>(v);
            sumAbs += static_cast<uint32_t>(av);
            if (v != 0) ++nonZero;
        }
        if (protocolStarted_) protocol_.loop();
        wifi_.loop();
    }

    if (samples == 0) {
        Serial.println("MIC Test: FEHLER - kein einziges I2S-Sample empfangen.");
        Serial.println("Damit liegt der Fehler lokal bei I2S/ES7210 und nicht bei STT/WebSocket.");
    } else {
        const uint32_t meanAbs = static_cast<uint32_t>(sumAbs / samples);
        Serial.printf("MIC Test: OK - %u Samples in %u Blöcken\n", samples, blocks);
        Serial.printf("Pegel: min=%d max=%d meanAbs=%u nonZero=%u/%u\n",
                      minSample, maxSample, meanAbs, nonZero, samples);
        if (meanAbs < 8) {
            Serial.println("Hinweis: Daten kommen an, Pegel ist aber extrem niedrig/nahe Null.");
        }
    }
    Serial.println("================");
    Serial.println();
}

void Satellite::printStatus() const {
    Serial.printf("Status: WLAN=%s Core=%s Ready=%s Aufnahme=%s Heap=%u PSRAM=%u\n",
                  wifi_.connected() ? "OK" : "OFF",
                  protocol_.connected() ? "OK" : "OFF",
                  protocol_.ready() ? "ja" : "nein",
                  recording_ ? "ja" : "nein",
                  ESP.getFreeHeap(),
                  ESP.getFreePsram());
}

void Satellite::printConsoleHelp() const {
    Serial.println();
    Serial.println("Serielle Testkonsole:");
    Serial.println("  mic    + ENTER  -> 2s Mikrofon/I2S lokal testen");
    Serial.println("  stt    + ENTER  -> STT-Test starten");
    Serial.println("  stop   + ENTER  -> Aufnahme vorzeitig beenden");
    Serial.println("  status + ENTER  -> Verbindungsstatus anzeigen");
    Serial.println("  help   + ENTER  -> diese Hilfe anzeigen");
    Serial.println();
}

void Satellite::pollSerialConsole() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());

        if (c == '\r') continue;
        if (c != '\n') {
            if (serialCommand_.length() < 64) serialCommand_ += c;
            continue;
        }

        serialCommand_.trim();
        serialCommand_.toLowerCase();

        if (!serialCommand_.length()) continue;

        if (serialCommand_ == "mic") {
            runMicTest();
        } else if (serialCommand_ == "stt" || serialCommand_ == "r") {
            if (!protocol_.ready()) {
                Serial.println("STT Test nicht möglich: Jarvis Core ist noch nicht bereit.");
            } else if (recording_) {
                Serial.println("STT Test nicht gestartet: Aufnahme läuft bereits.");
            } else {
                sttTestActive_ = true;
                startRecording();
            }
        } else if (serialCommand_ == "stop") {
            if (recording_) stopRecording();
            else Serial.println("Keine Aufnahme aktiv.");
        } else if (serialCommand_ == "status") {
            printStatus();
        } else if (serialCommand_ == "help" || serialCommand_ == "?") {
            printConsoleHelp();
        } else {
            Serial.printf("Unbekannter Konsolenbefehl: %s\n", serialCommand_.c_str());
            Serial.println("Mit 'help' werden die Befehle angezeigt.");
        }

        serialCommand_ = "";
    }
}

void Satellite::loop() {
    pollSerialConsole();
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
        printStatus();
    }

    delay(recording_ ? 1 : 4);
}
