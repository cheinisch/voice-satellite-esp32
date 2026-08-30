#include "core/satellite.h"
#include "build_info.h"
#include "voice_satellite_config.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <cmath>

namespace {
constexpr size_t CHUNK_SAMPLES = (VOICE_SATELLITE_AUDIO_RATE * VOICE_SATELLITE_AUDIO_CHUNK_MS) / 1000;
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

uint16_t getLe16(const uint8_t* src) {
    return static_cast<uint16_t>(src[0]) |
           (static_cast<uint16_t>(src[1]) << 8);
}

uint32_t getLe32(const uint8_t* src) {
    return static_cast<uint32_t>(src[0]) |
           (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) |
           (static_cast<uint32_t>(src[3]) << 24);
}
}

Satellite::Satellite(Board& board) : board_(board) {}

void Satellite::setUiState(SatelliteState state, const String& detail) {
    if (muted_ && state != SatelliteState::Error) {
        board_.setState(SatelliteState::Muted, "Mikrofon aus");
        return;
    }
    board_.setState(state, detail);
}

void Satellite::suspendWakeWord() {
    if (!wakeWordEnabled_ || wakeWordSuspended_) return;
    board_.audio().pauseWakeWord();
    wakeWordSuspended_ = true;
    // ESP_SR pauses its feed/detect tasks asynchronously via an event group.
    // Give the feed task one audio frame to release the shared I2S reader
    // before Voice Satellite starts direct recording or switches the bus to TX.
    delay(20);
}

void Satellite::resumeWakeWordIfIdle() {
    if (!wakeWordEnabled_ || !wakeWordSuspended_ || muted_) return;
    if (recording_ || awaitingResponse_ || ttsReceiving_ || ttsPlaybackPending_ || ttsPlaybackActive_) return;
    board_.audio().resumeWakeWord();
    wakeWordSuspended_ = false;
    if (protocol_.ready()) {
        setUiState(SatelliteState::Ready, String("Wakeword: ") + board_.audio().wakeWordName());
    }
}

void Satellite::toggleMute() {
    muted_ = !muted_;
    if (muted_) {
        suspendWakeWord();
        if (recording_) {
            recording_ = false;
            awaitingResponse_ = false;
            freeRecordingBuffer();
            Serial.println("Stumm: laufende Aufnahme lokal verworfen.");
        }
        setUiState(SatelliteState::Muted, "Mikrofon aus");
        Serial.println("Mikrofon stumm. Touch auf ZUHOEREN oder 'mute' schaltet es wieder frei.");
    } else {
        setUiState(protocol_.ready() ? SatelliteState::Ready : SatelliteState::ConnectingCore,
                   protocol_.ready() ? "Bereit" : "Reconnect");
        Serial.println("Mikrofon hört wieder zu.");
        resumeWakeWordIfIdle();
    }
}

bool Satellite::begin() {
    setUiState(SatelliteState::Booting, "Initialisiere Hardware");
    const bool boardOk = board_.begin();
    if (!boardOk) {
        setUiState(SatelliteState::Error, "Board-Initialisierung fehlgeschlagen");
        Serial.println("WARNUNG: Board-Initialisierung unvollständig; Netzwerk/Protokoll bleiben für Diagnose aktiv.");
    }

#if VOICE_SATELLITE_WAKEWORD_ENABLED
    if (boardOk) {
        wakeWordEnabled_ = board_.audio().beginWakeWord();
        wakeWordSuspended_ = false;
        if (wakeWordEnabled_) {
            Serial.printf("Wakeword bereit: '%s'. Audio bleibt lokal bis zur Erkennung.\n",
                          board_.audio().wakeWordName());
        } else {
            Serial.println("Wakeword angefordert, aber auf diesem Board nicht verfügbar/initialisierbar.");
        }
    }
#endif

    protocol_.setBinaryHandler([this](const uint8_t* data, size_t length) {
        handleTtsBinary(data, length);
    });
    protocol_.setEventHandler([this](VoiceEvent event, const String& text) {
        onVoiceEvent(event, text);
    });

    setUiState(SatelliteState::ConnectingWifi, "WLAN");
    const bool wifiOk = wifi_.begin();
    if (wifiOk) ensureProtocol();

    printConsoleHelp();
    return boardOk;
}

void Satellite::ensureProtocol() {
    if (!wifi_.connected() || protocolStarted_) return;
    setUiState(SatelliteState::ConnectingCore, "Core");
    protocol_.begin(board_);
    protocolStarted_ = true;
}

bool Satellite::allocateRecordingBuffer() {
    freeRecordingBuffer();

    const size_t maxSamples = (static_cast<size_t>(VOICE_SATELLITE_AUDIO_RATE) * VOICE_SATELLITE_RECORD_MS) / 1000;
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
    putLe16(h + 22, VOICE_SATELLITE_AUDIO_CHANNELS);
    putLe32(h + 24, VOICE_SATELLITE_AUDIO_RATE);
    putLe32(h + 28, VOICE_SATELLITE_AUDIO_RATE * VOICE_SATELLITE_AUDIO_CHANNELS * sizeof(int16_t));
    putLe16(h + 32, VOICE_SATELLITE_AUDIO_CHANNELS * sizeof(int16_t));
    putLe16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    putLe32(h + 40, static_cast<uint32_t>(recordingPcmBytes_));
}

void Satellite::startRecording(bool autoTts) {
    if (muted_) {
        Serial.println("Aufnahme blockiert: Mikrofon ist stumm.");
        setUiState(SatelliteState::Muted, "Mikrofon aus");
        return;
    }
    if (recording_ || !protocol_.ready()) return;

    // WakeNet consumes the same ES7210/I2S RX stream while idle. Pause it
    // before the normal recorder starts reading from I2S.
    suspendWakeWord();

    if (!allocateRecordingBuffer()) {
        resumeWakeWordIfIdle();
        if (sttTestActive_) sttTestActive_ = false;
        setUiState(SatelliteState::Error, "Kein Audiopuffer");
        return;
    }

    board_.audio().clearOutput();
    protocol_.sendSessionStart(autoTts);
    recording_ = true;
    recordingAutoTts_ = autoTts;
    awaitingResponse_ = false;
    recordingStartedAt_ = millis();
    silenceSpeechSeen_ = false;
    silenceVoicedSamples_ = 0;
    silenceLastVoiceAt_ = recordingStartedAt_;
    setUiState(SatelliteState::Listening, "Sprich jetzt");
    if (ttsTestActive_) {
        Serial.println();
        Serial.println("=== TTS ROUNDTRIP TEST ===");
        Serial.println("Sprich eine Frage. Voice Satellite soll die Antwort anschließend ausgeben.");
    } else if (sttTestActive_) {
        Serial.println();
        Serial.println("=== STT TEST ===");
        Serial.println("Sprich jetzt in das Mikrofon. Dieser Test fordert absichtlich kein TTS an.");
    }
    Serial.printf("Aufnahme läuft (max. %lums, auto_tts=%s, silence=%s)...\n",
                  static_cast<unsigned long>(VOICE_SATELLITE_RECORD_MS),
                  autoTts ? "ja" : "nein",
                  VOICE_SATELLITE_SILENCE_DETECTION ? "ja" : "nein");
}

void Satellite::stopRecording() {
    if (!recording_) return;
    recording_ = false;

    if (!recordingBuffer_ || recordingPcmBytes_ == 0) {
        Serial.println("STT: Keine Audiodaten aufgenommen.");
        freeRecordingBuffer();
        awaitingResponse_ = false;
        setUiState(SatelliteState::Ready, "Bereit");
        if (sttTestActive_) sttTestActive_ = false;
        resumeWakeWordIfIdle();
        return;
    }

    finalizeWavHeader();
    const size_t wavBytes = WAV_HEADER_BYTES + recordingPcmBytes_;
    Serial.printf("Sende WAV an Voice Satellite: %u Bytes, %.2f s\n",
                  static_cast<unsigned>(wavBytes),
                  static_cast<double>(recordingPcmBytes_) / (VOICE_SATELLITE_AUDIO_RATE * VOICE_SATELLITE_AUDIO_CHANNELS * sizeof(int16_t)));

    if (!protocol_.sendWav(recordingBuffer_, wavBytes)) {
        Serial.println("STT: WAV konnte nicht gesendet werden.");
        freeRecordingBuffer();
        awaitingResponse_ = false;
        setUiState(SatelliteState::Error, "Audio senden fehlgeschlagen");
        if (sttTestActive_) sttTestActive_ = false;
        resumeWakeWordIfIdle();
        return;
    }

    protocol_.sendAudioCommit();
    awaitingResponse_ = true;
    Serial.println("Sende Daten an Voice Satellite");
    freeRecordingBuffer();
    setUiState(SatelliteState::Processing, "Sende Daten an Voice Satellite");
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

#if VOICE_SATELLITE_SILENCE_DETECTION
        const uint32_t now = millis();
        if (now - recordingStartedAt_ >= VOICE_SATELLITE_SILENCE_ARM_MS) {
            uint64_t sumAbs = 0;
            for (size_t n = 0; n < got; ++n) {
                const int32_t sample = audioChunk[n];
                sumAbs += static_cast<uint32_t>(sample < 0 ? -sample : sample);
            }
            const uint32_t meanAbs = got ? static_cast<uint32_t>(sumAbs / got) : 0;
            if (meanAbs >= VOICE_SATELLITE_SILENCE_THRESHOLD) {
                silenceSpeechSeen_ = true;
                silenceVoicedSamples_ += static_cast<uint32_t>(got);
                silenceLastVoiceAt_ = now;
            }

            const uint32_t voicedMs = static_cast<uint32_t>(
                (static_cast<uint64_t>(silenceVoicedSamples_) * 1000ULL) / VOICE_SATELLITE_AUDIO_RATE);
            if (silenceSpeechSeen_ &&
                voicedMs >= VOICE_SATELLITE_SILENCE_MIN_SPEECH_MS &&
                now - silenceLastVoiceAt_ >= VOICE_SATELLITE_SILENCE_TIMEOUT_MS) {
                Serial.printf("Silence Detection: %lums Stille nach %lums Sprache -> Aufnahme Ende.\n",
                              static_cast<unsigned long>(now - silenceLastVoiceAt_),
                              static_cast<unsigned long>(voicedMs));
                stopRecording();
                return;
            }
        }
#endif

        if (copyBytes < bytes || WAV_HEADER_BYTES + recordingPcmBytes_ >= recordingCapacityBytes_) {
            stopRecording();
            return;
        }
    }
    if (millis() - recordingStartedAt_ >= VOICE_SATELLITE_RECORD_MS) stopRecording();
}

void Satellite::onVoiceEvent(VoiceEvent event, const String& text) {
    switch (event) {
        case VoiceEvent::Connected:
            setUiState(SatelliteState::ConnectingCore, "Handshake");
            break;
        case VoiceEvent::Disconnected:
            if (recording_) recording_ = false;
            freeRecordingBuffer();
            // A TTS response may be fully received immediately before the Core
            // closes/recycles the WebSocket. Preserve already-buffered audio
            // and play it locally instead of throwing it away.
            if (ttsBufferBytes_ > 0 && !ttsPlaybackActive_) {
                ttsReceiving_ = false;
                ttsPlaybackPending_ = true;
                Serial.printf("Core getrennt nach %u TTS-Bytes; lokale Wiedergabe wird fortgesetzt.\n",
                              static_cast<unsigned>(ttsBufferBytes_));
                if (ttsExpectedBytes_ > ttsBufferBytes_) {
                    Serial.printf("TTS UNVOLLSTÄNDIG: erwartet %u, empfangen %u, fehlen %u Bytes.\n",
                                  static_cast<unsigned>(ttsExpectedBytes_),
                                  static_cast<unsigned>(ttsBufferBytes_),
                                  static_cast<unsigned>(ttsExpectedBytes_ - ttsBufferBytes_));
                }
            } else if (!ttsPlaybackActive_) {
                awaitingResponse_ = false;
                if (ttsReceiving_ && ttsBufferBytes_ == 0) {
                    Serial.println("TTS Diagnose: Core trennte vor dem ersten Binär-Callback.");
                    Serial.println("TTS Diagnose: arduinoWebSockets begrenzt eingehende Frames standardmäßig auf 15 KiB;");
                    Serial.println("der Voice Satellite-Build patcht große TTS-Frames deshalb auf PSRAM-Unterstützung.");
                }
                ttsReceiving_ = false;
                ttsPlaybackPending_ = false;
                board_.audio().clearOutput();
            }
            setUiState(SatelliteState::ConnectingCore, "Reconnect");
            break;
        case VoiceEvent::Ready:
            setUiState(SatelliteState::Ready, "Bereit");
            resumeWakeWordIfIdle();
            break;
        case VoiceEvent::Transcript:
            board_.showTranscript(text);
            if (!recordingAutoTts_) {
                awaitingResponse_ = false;
                resumeWakeWordIfIdle();
            }
            if (ttsTestActive_) {
                Serial.printf("TTS Test - STT erkannt: %s\n", text.c_str());
            } else if (sttTestActive_) {
                Serial.println("------------------------------");
                Serial.printf("STT Ergebnis: %s\n", text.c_str());
                Serial.println("STT Test: erfolgreich empfangen");
                Serial.println("==============================");
                sttTestActive_ = false;
            }
            break;
        case VoiceEvent::Assistant:
            board_.showAssistant(text);
            if (ttsTestActive_) Serial.printf("TTS Test - Antworttext: %s\n", text.c_str());
            break;
        case VoiceEvent::TtsStart:
            resetTtsPlayback(protocol_.ttsSampleRate(), protocol_.ttsChannels());
            ttsExpectedBytes_ = protocol_.ttsExpectedBytes();
            ttsChunkSequence_ = 0;
            if (!allocateTtsBuffer()) {
                ttsReceiving_ = false;
                setUiState(SatelliteState::Error, "Kein TTS-Puffer");
                break;
            }
            ttsReceiving_ = true;
            ttsPlaybackPending_ = false;
            ttsPlaybackActive_ = false;
            board_.audio().clearOutput();
            setUiState(SatelliteState::Speaking, "TTS empfangen");
            if (ttsTestActive_) Serial.println("TTS Test - Audio vom Core startet ...");
            break;
        case VoiceEvent::TtsEnd:
            ttsReceiving_ = false;
            if (ttsExpectedBytes_ > 0 && ttsBufferBytes_ != ttsExpectedBytes_) {
                const size_t missing = ttsExpectedBytes_ > ttsBufferBytes_
                    ? ttsExpectedBytes_ - ttsBufferBytes_ : 0;
                Serial.printf("TTS Transferprüfung: erwartet %u, empfangen %u, fehlen %u Bytes.\n",
                              static_cast<unsigned>(ttsExpectedBytes_),
                              static_cast<unsigned>(ttsBufferBytes_),
                              static_cast<unsigned>(missing));
            }
            if (ttsBufferBytes_ > 0) {
                ttsPlaybackPending_ = true;
                Serial.printf("TTS vollständig empfangen: %u Bytes in %lu Chunk(s). Wiedergabe startet.\n",
                              static_cast<unsigned>(ttsBufferBytes_),
                              static_cast<unsigned long>(ttsChunkSequence_));
            } else {
                Serial.println("TTS beendet, aber es wurden keine Binärdaten empfangen.");
                finishTtsPlayback();
            }
            break;
        case VoiceEvent::Error:
            if (sttTestActive_) {
                Serial.printf("STT Test fehlgeschlagen: %s\n", text.c_str());
                sttTestActive_ = false;
            }
            if (ttsTestActive_) {
                Serial.printf("TTS Test fehlgeschlagen: %s\n", text.c_str());
                ttsTestActive_ = false;
            }
            awaitingResponse_ = false;
            setUiState(SatelliteState::Error, text);
            resumeWakeWordIfIdle();
            break;
    }
}


void Satellite::resetTtsPlayback(uint32_t sampleRate, uint8_t channels) {
    ttsInputRate_ = (sampleRate >= 8000 && sampleRate <= 96000) ? sampleRate : VOICE_SATELLITE_AUDIO_RATE;
    ttsInputChannels_ = (channels == 2) ? 2 : 1;
    ttsResampleAccumulator_ = 0;
    ttsInputBytes_ = 0;
    ttsOutputSamples_ = 0;
    ttsChunkSequence_ = 0;
    ttsPcmOffset_ = 0;
    ttsPcmEnd_ = 0;
}

bool Satellite::allocateTtsBuffer() {
    freeTtsBuffer();

    // TTS must never be rendered directly from the WebSocket callback. A full
    // WAV response can take several seconds to play and would starve ws_.loop(),
    // causing heartbeat timeouts/disconnects. Keep it in PSRAM and play it from
    // the normal main loop instead.
    constexpr size_t PSRAM_BYTES = 2U * 1024U * 1024U;
    constexpr size_t HEAP_BYTES = 256U * 1024U;
    const size_t wanted = psramFound() ? PSRAM_BYTES : HEAP_BYTES;

    if (psramFound()) {
        ttsBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(wanted, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (!ttsBuffer_) {
        ttsBuffer_ = static_cast<uint8_t*>(malloc(wanted));
    }
    if (!ttsBuffer_) {
        Serial.printf("TTS: %u Bytes Empfangspuffer konnten nicht reserviert werden.\n",
                      static_cast<unsigned>(wanted));
        return false;
    }

    ttsBufferCapacity_ = wanted;
    ttsBufferBytes_ = 0;
    Serial.printf("TTS Empfangspuffer: %u Bytes (%s)\n",
                  static_cast<unsigned>(ttsBufferCapacity_),
                  psramFound() ? "PSRAM bevorzugt" : "Heap");
    return true;
}

void Satellite::freeTtsBuffer() {
    if (ttsBuffer_) {
        free(ttsBuffer_);
        ttsBuffer_ = nullptr;
    }
    ttsBufferCapacity_ = 0;
    ttsBufferBytes_ = 0;
    ttsPcmOffset_ = 0;
    ttsPcmEnd_ = 0;
}

bool Satellite::appendTtsData(const uint8_t* data, size_t length) {
    if (!data || !length) return true;
    if (!ttsBuffer_ && !allocateTtsBuffer()) return false;

    const size_t available = ttsBufferCapacity_ > ttsBufferBytes_
        ? ttsBufferCapacity_ - ttsBufferBytes_ : 0;
    if (length > available) {
        Serial.printf("TTS Empfangspuffer voll: %u + %u > %u Bytes. Audio wird verworfen.\n",
                      static_cast<unsigned>(ttsBufferBytes_),
                      static_cast<unsigned>(length),
                      static_cast<unsigned>(ttsBufferCapacity_));
        return false;
    }

    memcpy(ttsBuffer_ + ttsBufferBytes_, data, length);
    ttsBufferBytes_ += length;
    ttsInputBytes_ += length;
    return true;
}

bool Satellite::prepareTtsPlayback() {
    if (!ttsBuffer_ || ttsBufferBytes_ < sizeof(int16_t)) return false;

    uint32_t rate = ttsInputRate_;
    uint8_t channels = ttsInputChannels_;
    size_t pcmOffset = 0;
    size_t pcmBytes = ttsBufferBytes_;

    // A complete WAV may have arrived as one frame or as multiple WebSocket
    // fragments. At this point all fragments are contiguous in PSRAM, so WAV
    // parsing is independent of WebSocket frame boundaries.
    if (ttsBufferBytes_ >= 12 &&
        !memcmp(ttsBuffer_, "RIFF", 4) &&
        !memcmp(ttsBuffer_ + 8, "WAVE", 4)) {
        size_t offset = 12;
        uint16_t audioFormat = 0;
        uint16_t wavChannels = 0;
        uint16_t bits = 0;
        uint32_t wavRate = 0;
        bool foundData = false;

        while (offset + 8 <= ttsBufferBytes_) {
            const uint8_t* chunk = ttsBuffer_ + offset;
            const uint32_t chunkSize = getLe32(chunk + 4);
            const size_t payloadOffset = offset + 8;
            if (payloadOffset > ttsBufferBytes_) break;
            const size_t available = ttsBufferBytes_ - payloadOffset;
            const size_t actualSize = chunkSize <= available ? chunkSize : available;

            if (!memcmp(chunk, "fmt ", 4) && actualSize >= 16) {
                audioFormat = getLe16(ttsBuffer_ + payloadOffset + 0);
                wavChannels = getLe16(ttsBuffer_ + payloadOffset + 2);
                wavRate = getLe32(ttsBuffer_ + payloadOffset + 4);
                bits = getLe16(ttsBuffer_ + payloadOffset + 14);
            } else if (!memcmp(chunk, "data", 4)) {
                pcmOffset = payloadOffset;
                pcmBytes = actualSize;
                foundData = true;
                break;
            }

            const size_t padded = static_cast<size_t>(chunkSize) + (chunkSize & 1U);
            if (padded > ttsBufferBytes_ - payloadOffset) break;
            offset = payloadOffset + padded;
        }

        if (!foundData || audioFormat != 1 || bits != 16 ||
            (wavChannels != 1 && wavChannels != 2) ||
            wavRate < 8000 || wavRate > 96000) {
            Serial.printf("TTS WAV nicht unterstützt: format=%u rate=%lu channels=%u bits=%u data=%u\n",
                          audioFormat, static_cast<unsigned long>(wavRate), wavChannels, bits,
                          static_cast<unsigned>(pcmBytes));
            return false;
        }

        rate = wavRate;
        channels = static_cast<uint8_t>(wavChannels);
        Serial.printf("TTS WAV gepuffert: %lu Hz, %u Kanal/Kanäle, PCM16, %u Bytes Audio.\n",
                      static_cast<unsigned long>(rate), channels,
                      static_cast<unsigned>(pcmBytes));
    } else {
        if (protocol_.ttsBitsPerSample() != 16) {
            Serial.printf("TTS Raw-Audio ignoriert: %u Bit werden noch nicht unterstützt.\n",
                          protocol_.ttsBitsPerSample());
            return false;
        }
        Serial.printf("TTS Raw-PCM gepuffert: %lu Hz, %u Kanal/Kanäle, %u Bytes.\n",
                      static_cast<unsigned long>(rate), channels,
                      static_cast<unsigned>(pcmBytes));
    }

    const size_t frameBytes = sizeof(int16_t) * channels;
    pcmBytes -= pcmBytes % frameBytes;
    if (!pcmBytes) return false;

    ttsInputRate_ = rate;
    ttsInputChannels_ = channels;
    ttsResampleAccumulator_ = 0;
    ttsPcmOffset_ = pcmOffset;
    ttsPcmEnd_ = pcmOffset + pcmBytes;
    ttsPlaybackPending_ = false;
    ttsPlaybackActive_ = true;
    setUiState(SatelliteState::Speaking, "Voice Satellite spricht");
    return true;
}

void Satellite::pumpTtsPlayback() {
    if (ttsPlaybackPending_ && !ttsPlaybackActive_) {
        if (!prepareTtsPlayback()) {
            Serial.println("TTS: Gepufferte Audiodaten konnten nicht für die Wiedergabe vorbereitet werden.");
            finishTtsPlayback();
            return;
        }
    }
    if (!ttsPlaybackActive_ || !ttsBuffer_) return;

    const size_t frameBytes = sizeof(int16_t) * ttsInputChannels_;
    if (ttsPcmOffset_ >= ttsPcmEnd_ || ttsPcmEnd_ - ttsPcmOffset_ < frameBytes) {
        finishTtsPlayback();
        return;
    }

    // Process only a short block per main-loop iteration. This keeps Wi-Fi and
    // WebSocket heartbeats alive while the ESP32 is speaking.
    constexpr size_t INPUT_FRAMES = 256;
    constexpr size_t OUTPUT_SAMPLES = 512; // max 2x upsample (8 kHz -> 16 kHz)
    int16_t out[OUTPUT_SAMPLES];
    size_t outCount = 0;

    const size_t remainingFrames = (ttsPcmEnd_ - ttsPcmOffset_) / frameBytes;
    const size_t frames = remainingFrames < INPUT_FRAMES ? remainingFrames : INPUT_FRAMES;

    for (size_t frame = 0; frame < frames; ++frame) {
        const uint8_t* src = ttsBuffer_ + ttsPcmOffset_ + frame * frameBytes;
        const int16_t left = static_cast<int16_t>(getLe16(src));
        int32_t mono = left;
        if (ttsInputChannels_ == 2) {
            const int16_t right = static_cast<int16_t>(getLe16(src + sizeof(int16_t)));
            mono = (static_cast<int32_t>(left) + static_cast<int32_t>(right)) / 2;
        }

        ttsResampleAccumulator_ += VOICE_SATELLITE_AUDIO_RATE;
        while (ttsResampleAccumulator_ >= ttsInputRate_ && outCount < OUTPUT_SAMPLES) {
            ttsResampleAccumulator_ -= ttsInputRate_;
            out[outCount++] = static_cast<int16_t>(mono);
        }
    }

    ttsPcmOffset_ += frames * frameBytes;

    if (outCount) {
        const size_t written = board_.audio().writePcm16(out, outCount, 100);
        ttsOutputSamples_ += written;
        if (written != outCount) {
            Serial.printf("TTS Speaker: nur %u/%u Samples geschrieben.\n",
                          static_cast<unsigned>(written), static_cast<unsigned>(outCount));
        }
    }

    if (ttsPcmOffset_ >= ttsPcmEnd_) finishTtsPlayback();
}

void Satellite::finishTtsPlayback() {
    const bool hadAudio = ttsOutputSamples_ > 0;
    board_.audio().clearOutput();
    ttsReceiving_ = false;
    ttsPlaybackPending_ = false;
    ttsPlaybackActive_ = false;

    Serial.printf("TTS Audio: %u Eingangsbytes -> %u PCM-Samples ausgegeben.\n",
                  static_cast<unsigned>(ttsInputBytes_),
                  static_cast<unsigned>(ttsOutputSamples_));

    freeTtsBuffer();
    ttsExpectedBytes_ = 0;
    ttsChunkSequence_ = 0;
    awaitingResponse_ = false;
    setUiState(protocol_.ready() ? SatelliteState::Ready : SatelliteState::ConnectingCore,
                    protocol_.ready() ? "Bereit" : "Reconnect");
    resumeWakeWordIfIdle();

    if (ttsTestActive_) {
        Serial.println(hadAudio
            ? "TTS Test: Audiodaten wurden an den Lautsprecher ausgegeben."
            : "TTS Test: KEINE TTS-Audiodaten ausgegeben.");
        Serial.println("==============================");
        ttsTestActive_ = false;
    }
}

void Satellite::handleTtsBinary(const uint8_t* data, size_t length) {
    if (!data || !length) return;

    if (!ttsReceiving_) {
        resetTtsPlayback(protocol_.ttsSampleRate(), protocol_.ttsChannels());
        if (!allocateTtsBuffer()) return;
        ttsReceiving_ = true;
        ttsPlaybackPending_ = false;
        ttsPlaybackActive_ = false;
        setUiState(SatelliteState::Speaking, "TTS empfangen");
        Serial.println("TTS: Binäraudio ohne Start-Event empfangen; wird im PSRAM gepuffert.");
    }

    if (!appendTtsData(data, length)) {
        Serial.println("TTS: Audiofragment konnte nicht gepuffert werden; kein ACK wird gesendet.");
        return;
    }

    ++ttsChunkSequence_;
    if (protocol_.ttsAckRequired()) {
        // ACK only after the complete frame is safely in PSRAM. The Core will
        // not send the next chunk until this acknowledgement arrives.
        protocol_.sendTtsAck(ttsChunkSequence_, ttsBufferBytes_);
    }

    // Keep the WebSocket callback deliberately short. Playback happens later
    // in pumpTtsPlayback(), never from inside arduinoWebSockets' event handler.
    if (ttsBufferBytes_ == length) {
        Serial.printf("TTS: erstes Audiofragment empfangen (%u Bytes).\n",
                      static_cast<unsigned>(length));
    }
}

void Satellite::runSpeakerTest() {
    if (recording_) {
        Serial.println("Speaker-Test nicht möglich: Aufnahme läuft.");
        return;
    }

    suspendWakeWord();
    Serial.println();
    Serial.println("=== SPEAKER TEST ===");
    Serial.println("Spiele 1 kHz Testton für 1 Sekunde ...");
    setUiState(SatelliteState::Speaking, "Speaker-Test");
    board_.audio().clearOutput();

    constexpr float TEST_TONE_TWO_PI = 6.28318530717958647692f;
    constexpr float FREQ = 1000.0f;
    constexpr int16_t AMPLITUDE = 9000;
    constexpr size_t BLOCK = 256;
    int16_t tone[BLOCK];
    const size_t totalSamples = VOICE_SATELLITE_AUDIO_RATE;
    size_t generated = 0;
    size_t writtenTotal = 0;

    while (generated < totalSamples) {
        const size_t count = (totalSamples - generated) < BLOCK ? (totalSamples - generated) : BLOCK;
        for (size_t n = 0; n < count; ++n) {
            const float phase = TEST_TONE_TWO_PI * FREQ * static_cast<float>(generated + n) / static_cast<float>(VOICE_SATELLITE_AUDIO_RATE);
            tone[n] = static_cast<int16_t>(sinf(phase) * AMPLITUDE);
        }
        const size_t written = board_.audio().writePcm16(tone, count, 1000);
        writtenTotal += written;
        if (written != count) {
            Serial.printf("Speaker-Test: nur %u/%u Samples geschrieben.\n",
                          static_cast<unsigned>(written), static_cast<unsigned>(count));
            break;
        }
        generated += count;
        if (protocolStarted_) protocol_.loop();
        wifi_.loop();
    }

    board_.audio().clearOutput();
    setUiState(SatelliteState::Ready, "Bereit");
    if (writtenTotal > 0) {
        Serial.printf("Speaker-Test: %u Samples an I2S/ES8311 ausgegeben.\n", static_cast<unsigned>(writtenTotal));
        Serial.println("Wenn nichts hörbar war, liegt der nächste Test bei ES8311/PA/Lautsprecher - nicht beim Core.");
    } else {
        Serial.println("Speaker-Test: FEHLER - AudioIO hat 0 Samples angenommen.");
    }
    Serial.println("====================");
    Serial.println();
    resumeWakeWordIfIdle();
}

void Satellite::runMicTest() {
    if (recording_) {
        Serial.println("MIC Test nicht möglich: Aufnahme läuft bereits.");
        return;
    }

    suspendWakeWord();
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
    resumeWakeWordIfIdle();
}

void Satellite::printStatus() const {
    Serial.printf("Status: WLAN=%s Core=%s Ready=%s Aufnahme=%s Mute=%s Wake=%s Heap=%u PSRAM=%u\n",
                  wifi_.connected() ? "OK" : "OFF",
                  protocol_.connected() ? "OK" : "OFF",
                  protocol_.ready() ? "ja" : "nein",
                  recording_ ? "ja" : "nein",
                  muted_ ? "ja" : "nein",
                  wakeWordEnabled_ ? (wakeWordSuspended_ ? "pause" : "an") : "aus",
                  ESP.getFreeHeap(),
                  ESP.getFreePsram());
}

void Satellite::printConsoleHelp() const {
    Serial.println();
    Serial.println("Serielle Testkonsole:");
    Serial.println("  mic        + ENTER  -> 2s Mikrofon/I2S lokal testen");
    Serial.println("  spk        + ENTER  -> 1s Testton lokal über ES8311/Lautsprecher");
    Serial.println("  stt        + ENTER  -> STT-only Test (kein TTS vom Core)");
    Serial.println("  tts        + ENTER  -> kompletter STT -> Voice Satellite -> TTS Roundtrip");
    Serial.println("  stop       + ENTER  -> Aufnahme vorzeitig beenden");
    Serial.println("  mute       + ENTER  -> Mikrofon stumm / zuhören umschalten");
    Serial.println("  wake       + ENTER  -> Wakeword-Status anzeigen");
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
        } else if (serialCommand_ == "spk" || serialCommand_ == "speaker") {
            runSpeakerTest();
        } else if (serialCommand_ == "stt" || serialCommand_ == "r") {
            if (!protocol_.ready()) {
                Serial.println("STT Test nicht möglich: Core ist noch nicht bereit.");
            } else if (recording_) {
                Serial.println("STT Test nicht gestartet: Aufnahme läuft bereits.");
            } else {
                sttTestActive_ = true;
                ttsTestActive_ = false;
                startRecording(false);
            }
        } else if (serialCommand_ == "tts") {
            if (!protocol_.ready()) {
                Serial.println("TTS Test nicht möglich: Core ist noch nicht bereit.");
            } else if (recording_) {
                Serial.println("TTS Test nicht gestartet: Aufnahme läuft bereits.");
            } else {
                sttTestActive_ = false;
                ttsTestActive_ = true;
                startRecording(true);
            }
        } else if (serialCommand_ == "stop") {
            if (recording_) stopRecording();
            else Serial.println("Keine Aufnahme aktiv.");
        } else if (serialCommand_ == "mute" || serialCommand_ == "unmute") {
            toggleMute();
        } else if (serialCommand_ == "wake") {
            Serial.printf("Wakeword: %s%s\n",
                          wakeWordEnabled_ ? board_.audio().wakeWordName() : "aus",
                          wakeWordEnabled_ && wakeWordSuspended_ ? " (pausiert)" : "");
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

    if (board_.consumeMuteToggle()) toggleMute();

    if (wakeWordEnabled_ && board_.audio().consumeWakeWordTrigger()) {
        wakeWordSuspended_ = true; // callback pauses ESP_SR before handing over I2S
        if (muted_) {
            Serial.println("Wakeword erkannt, aber Mikrofon ist stumm.");
        } else if (!protocol_.ready()) {
            Serial.println("Wakeword erkannt, aber Core ist nicht bereit.");
            awaitingResponse_ = false;
            resumeWakeWordIfIdle();
        } else if (recording_ || awaitingResponse_ || ttsReceiving_ || ttsPlaybackActive_) {
            Serial.println("Wakeword ignoriert: Sprachrunde läuft bereits.");
        } else {
            Serial.printf("Wakeword erkannt: %s\n", board_.audio().wakeWordName());
            // The ESP_SR callback has already requested PAUSE_FEED. Allow the
            // background feed task to stop reading the shared I2S stream.
            delay(20);
            startRecording(VOICE_SATELLITE_AUTO_TTS != 0);
        }
    }

    if (board_.consumeVoiceTrigger()) {
        if (recording_) stopRecording();
        else startRecording(VOICE_SATELLITE_AUTO_TTS != 0);
    }

    pumpRecording();
    pumpTtsPlayback();

    if (millis() - lastStatusAt_ >= 30000) {
        lastStatusAt_ = millis();
        printStatus();
    }

    delay(recording_ ? 1 : 4);
}
