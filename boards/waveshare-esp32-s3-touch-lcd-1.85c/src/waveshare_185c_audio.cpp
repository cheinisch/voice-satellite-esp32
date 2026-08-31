#include "waveshare_185c_audio.h"
#include "waveshare_185c_pins.h"
#include "voice_satellite_config.h"
#include <ESP_I2S.h>
#include <Wire.h>
#include <AudioBoard.h>
#include <algorithm>

#if VOICE_SATELLITE_WAKEWORD_ENABLED
#include <ESP_SR.h>
#endif

using namespace audio_driver;

namespace {

constexpr uint8_t ES7210 = waveshare185c::ES7210_ADDR;
Waveshare185CAudio* activeAudioInstance = nullptr;

bool wireProbe(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool writeReg(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    const uint8_t rc = Wire.endTransmission();
    if (rc != 0) {
        Serial.printf("I2C write fehlgeschlagen: addr=0x%02X reg=0x%02X value=0x%02X rc=%u\n",
                      address, reg, value, rc);
        return false;
    }
    return true;
}

bool readReg(uint8_t address, uint8_t reg, uint8_t& value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) return false;
    value = static_cast<uint8_t>(Wire.read());
    return true;
}

bool initEs7210Waveshare() {
    if (!wireProbe(ES7210)) {
        Serial.println("ES7210 nicht auf I2C 0x40 erreichbar.");
        return false;
    }

    struct RegValue { uint8_t reg; uint8_t value; };
    static constexpr RegValue init[] = {
        {0x00, 0xFF},
        {0x00, 0x32},
        {0x09, 0x30},
        {0x0A, 0x30},

        {0x23, 0x2A},
        {0x22, 0x0A},
        {0x21, 0x2A},
        {0x20, 0x0A},

        {0x11, 0x60},
        {0x12, 0x00},

        {0x40, 0xC3},
        {0x41, 0x70},
        {0x42, 0x70},

        {0x43, 0x1D},
        {0x44, 0x1D},
        {0x45, 0x1D},
        {0x46, 0x1D},

        {0x47, 0x08},
        {0x48, 0x08},
        {0x49, 0x08},
        {0x4A, 0x08},

        {0x07, 0x20},
        {0x02, 0xC1},
        {0x04, 0x01},
        {0x05, 0x00},

        {0x06, 0x04},
        {0x4B, 0x0F},
        {0x4C, 0x0F},
        {0x00, 0x71},
        {0x00, 0x41},

        {0x1B, 0xBF},
        {0x1C, 0xBF},
        {0x1D, 0xBF},
        {0x1E, 0xBF},
    };

    for (const auto& rv : init) {
        if (!writeReg(ES7210, rv.reg, rv.value)) return false;
    }
    delay(20);

    uint8_t reset = 0;
    if (readReg(ES7210, 0x00, reset)) {
        Serial.printf("ES7210 Waveshare-Profil initialisiert (REG00=0x%02X).\n", reset);
    } else {
        Serial.println("ES7210 initialisiert, Register-Readback fehlgeschlagen.");
    }
    return true;
}

#if VOICE_SATELLITE_WAKEWORD_ENABLED
Waveshare185CAudio* wakeWordOwner = nullptr;

void onWakeWordEvent(sr_event_t event, int commandId, int phraseId) {
    (void)commandId;
    (void)phraseId;
    if (!wakeWordOwner) return;
    if (event == SR_EVENT_WAKEWORD || event == SR_EVENT_WAKEWORD_CHANNEL) {
        wakeWordOwner->markWakeWordDetected();
    }
}
#endif

} // namespace

struct Waveshare185CAudio::Impl {
    DriverPins speakerPins;
    AudioDriverES8311Class speakerDriver;
    AudioBoard speakerBoard{speakerDriver, speakerPins};

    I2SClass i2s;
    uint32_t lastRxDebugAt = 0;
    bool micStarted = false;
    bool speakerStarted = false;
    bool playbackMode = false;
    bool speakerPinsConfigured = false;
    bool wakeWordStarted = false;
    bool wakeWordPaused = false;
    bool restartWakeWordAfterMedia = false;
    bool mediaSuspended = false;
    volatile bool wakeWordDetected = false;
    uint8_t desiredVolume = static_cast<uint8_t>(
        std::clamp<int>(VOICE_SATELLITE_WAVESHARE_SPEAKER_VOLUME, 0, 100)
    );
};

Waveshare185CAudio::Waveshare185CAudio() : impl_(new Impl()) {
    activeAudioInstance = this;
}

Waveshare185CAudio::~Waveshare185CAudio() {
#if VOICE_SATELLITE_WAKEWORD_ENABLED
    if (impl_ && impl_->wakeWordStarted) ESP_SR.end();
    if (wakeWordOwner == this) wakeWordOwner = nullptr;
#endif
    if (activeAudioInstance == this) activeAudioInstance = nullptr;
    delete impl_;
}

Waveshare185CAudio* Waveshare185CAudio::activeInstance() {
    return activeAudioInstance;
}

bool Waveshare185CAudio::startMicI2s() {
    auto& i = *impl_;
    if (i.i2s.txChan() || i.i2s.rxChan()) {
        i.i2s.end();
        delay(2);
    }

    i.i2s.setPins(waveshare185c::I2S_BCLK, waveshare185c::I2S_LRCK,
                  -1, waveshare185c::I2S_DIN, waveshare185c::I2S_MCLK);
    i.i2s.setTimeout(1000);
    if (!i.i2s.begin(I2S_MODE_STD, VOICE_SATELLITE_AUDIO_RATE,
                     I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.printf("Waveshare MIC I2S Initialisierung fehlgeschlagen: %d\n", i.i2s.lastError());
        i.micStarted = false;
        return false;
    }
    if (!i.i2s.rxChan()) {
        Serial.println("Waveshare MIC I2S Fehler: RX-Kanal wurde nicht angelegt.");
        i.micStarted = false;
        return false;
    }

    i.micStarted = true;
    i.playbackMode = false;
    Serial.printf("Waveshare MIC I2S: Port=%d RX=ja TX=nein %u Hz/16-bit/stereo.\n",
                  static_cast<int>(i.i2s.getPort()),
                  static_cast<unsigned>(VOICE_SATELLITE_AUDIO_RATE));
    return true;
}

bool Waveshare185CAudio::restoreMicPath() {
    auto& i = *impl_;

    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, LOW);

    i.micStarted = false;
    if (!startMicI2s()) {
        Serial.println("Waveshare MIC Restore: RX-I2S konnte nicht gestartet werden.");
        return false;
    }

    delay(20);
    if (!initEs7210Waveshare()) {
        Serial.println("Waveshare MIC Restore: ES7210 konnte nicht neu initialisiert werden.");
        i.i2s.end();
        i.micStarted = false;
        i.playbackMode = false;
        return false;
    }

    int16_t discard[64 * 2];
    i.i2s.setTimeout(2);
    for (int n = 0; n < 3; ++n) {
        i.i2s.readBytes(reinterpret_cast<char*>(discard), sizeof(discard));
    }

    i.micStarted = true;
    i.playbackMode = false;
    Serial.println("Waveshare MIC Restore: RX-I2S + ES7210 wieder aktiv.");
    return true;
}

bool Waveshare185CAudio::startSpeakerI2s() {
    auto& i = *impl_;
    if (i.i2s.txChan() || i.i2s.rxChan()) {
        i.i2s.end();
        delay(2);
    }

    i.i2s.setPins(waveshare185c::I2S_BCLK, waveshare185c::I2S_LRCK,
                  waveshare185c::I2S_DOUT, -1, waveshare185c::I2S_MCLK);
    i.i2s.setTimeout(1000);
    if (!i.i2s.begin(I2S_MODE_STD, VOICE_SATELLITE_AUDIO_RATE,
                     I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO,
                     I2S_STD_SLOT_LEFT)) {
        Serial.printf("Waveshare Speaker I2S Initialisierung fehlgeschlagen: %d\n", i.i2s.lastError());
        i.playbackMode = false;
        return false;
    }
    if (!i.i2s.txChan()) {
        Serial.println("Waveshare Speaker I2S Fehler: TX-Kanal wurde nicht angelegt.");
        i.playbackMode = false;
        return false;
    }

    i.micStarted = false;
    i.playbackMode = true;
    Serial.printf("Waveshare Speaker I2S: Port=%d TX=ja RX=nein %lu Hz/16-bit/mono-left.\n",
                  static_cast<int>(i.i2s.getPort()),
                  static_cast<unsigned long>(i.i2s.txSampleRate()));
    return true;
}

bool Waveshare185CAudio::begin() {
    auto& i = *impl_;

    Serial.printf("Audio I2C Probe: ES8311(0x18)=%s ES7210(0x40)=%s\n",
                  wireProbe(waveshare185c::ES8311_ADDR) ? "OK" : "FEHLT",
                  wireProbe(waveshare185c::ES7210_ADDR) ? "OK" : "FEHLT");

    if (!initEs7210Waveshare()) {
        Serial.println("ES7210 Waveshare-Initialisierung fehlgeschlagen.");
        return false;
    }

    if (!startMicI2s()) return false;
    delay(20);

    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, LOW);

    Serial.printf("Waveshare MIC bereit (Port %d, RX=%s, TX=%s, %u Hz/16-bit/stereo).\n",
                  static_cast<int>(i.i2s.getPort()),
                  i.i2s.rxChan() ? "ja" : "nein",
                  i.i2s.txChan() ? "ja" : "nein",
                  static_cast<unsigned>(VOICE_SATELLITE_AUDIO_RATE));
    Serial.println("ES8311/Lautsprecher wird erst bei der ersten TTS-Ausgabe initialisiert.");
    return true;
}

bool Waveshare185CAudio::beginWakeWord() {
#if VOICE_SATELLITE_WAKEWORD_ENABLED
    auto& i = *impl_;
    if (i.wakeWordStarted) return true;
    if (!i.micStarted && !restoreMicPath()) {
        Serial.println("WakeNet: Mikrofonpfad ist nicht bereit.");
        return false;
    }

    wakeWordOwner = this;
    i.wakeWordDetected = false;
    ESP_SR.onEvent(onWakeWordEvent);

    if (!ESP_SR.begin(i.i2s, nullptr, 0, SR_CHANNELS_STEREO, SR_MODE_WAKEWORD, "MM")) {
        Serial.println("WakeNet: ESP_SR konnte nicht gestartet werden. Ist srmodels.bin geflasht?");
        wakeWordOwner = nullptr;
        return false;
    }

    i.wakeWordStarted = true;
    i.wakeWordPaused = false;
    Serial.printf("WakeNet aktiv: Wakeword '%s' (lokal auf ESP32-S3).\n", VOICE_SATELLITE_WAKEWORD_NAME);
    return true;
#else
    return false;
#endif
}

void Waveshare185CAudio::markWakeWordDetected() {
#if VOICE_SATELLITE_WAKEWORD_ENABLED
    if (!impl_ || !impl_->wakeWordStarted) return;
    if (impl_->wakeWordDetected) return;
    impl_->wakeWordDetected = true;
    pauseWakeWord();
#endif
}

void Waveshare185CAudio::pauseWakeWord() {
#if VOICE_SATELLITE_WAKEWORD_ENABLED
    auto& i = *impl_;
    if (!i.wakeWordStarted || i.wakeWordPaused) return;
    if (ESP_SR.pause()) {
        i.wakeWordPaused = true;
    } else {
        Serial.println("WakeNet Warnung: Pause fehlgeschlagen.");
    }
#endif
}

void Waveshare185CAudio::resumeWakeWord() {
#if VOICE_SATELLITE_WAKEWORD_ENABLED
    auto& i = *impl_;
    if (!i.wakeWordStarted) return;
    i.wakeWordDetected = false;
    ESP_SR.setMode(SR_MODE_WAKEWORD);
    if (i.wakeWordPaused) {
        if (ESP_SR.resume()) {
            i.wakeWordPaused = false;
        } else {
            Serial.println("WakeNet Warnung: Resume fehlgeschlagen.");
        }
    }
#endif
}

bool Waveshare185CAudio::consumeWakeWordTrigger() {
#if VOICE_SATELLITE_WAKEWORD_ENABLED
    if (!impl_ || !impl_->wakeWordDetected) return false;
    impl_->wakeWordDetected = false;
    return true;
#else
    return false;
#endif
}

bool Waveshare185CAudio::wakeWordActive() const {
#if VOICE_SATELLITE_WAKEWORD_ENABLED
    return impl_ && impl_->wakeWordStarted;
#else
    return false;
#endif
}

const char* Waveshare185CAudio::wakeWordName() const {
    return VOICE_SATELLITE_WAKEWORD_NAME;
}

bool Waveshare185CAudio::suspendForMediaPlayback() {
    auto& i = *impl_;
    if (i.mediaSuspended) return true;

    Serial.println("[media] Waveshare Audio: gebe I2S fuer Media Playback frei ...");

#if VOICE_SATELLITE_WAKEWORD_ENABLED
    i.restartWakeWordAfterMedia = i.wakeWordStarted;
    if (i.wakeWordStarted) {
        ESP_SR.end();
        i.wakeWordStarted = false;
        i.wakeWordPaused = false;
        i.wakeWordDetected = false;
        if (wakeWordOwner == this) wakeWordOwner = nullptr;
    }
#endif

    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, LOW);

    if (i.i2s.txChan() || i.i2s.rxChan()) {
        i.i2s.end();
        delay(4);
    }

    i.micStarted = false;
    i.playbackMode = false;

    // If TTS already initialized the ES8311, invalidate that codec state.
    // Media Playback uses its own 44.1 kHz codec profile. Future TTS must
    // therefore initialize the 16 kHz profile again.
    if (i.speakerStarted) {
        i.speakerBoard.setMute(true);
        i.speakerDriver.end();
        i.speakerStarted = false;
    }

    i.mediaSuspended = true;
    Serial.println("[media] Waveshare Audio: I2S ist frei.");
    return true;
}

bool Waveshare185CAudio::resumeAfterMediaPlayback() {
    auto& i = *impl_;
    if (!i.mediaSuspended) return true;

    Serial.println("[media] Waveshare Audio: stelle Mikrofonpfad wieder her ...");
    i.mediaSuspended = false;

    if (!restoreMicPath()) {
        Serial.println("[media] Waveshare Audio: Mikrofon-Restore fehlgeschlagen.");
        return false;
    }

#if VOICE_SATELLITE_WAKEWORD_ENABLED
    if (i.restartWakeWordAfterMedia) {
        i.restartWakeWordAfterMedia = false;
        if (!beginWakeWord()) {
            Serial.println("[media] Waveshare Audio: WakeNet-Restore fehlgeschlagen.");
            return false;
        }
    }
#endif

    Serial.println("[media] Waveshare Audio: Mikrofon/WakeNet wieder bereit.");
    return true;
}

bool Waveshare185CAudio::ensureSpeaker() {
    auto& i = *impl_;

    if (!i.speakerStarted) {
        AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

        if (!wireProbe(waveshare185c::ES8311_ADDR)) {
            Serial.println("ES8311 nicht auf I2C 0x18 erreichbar; TTS-Ausgabe deaktiviert.");
            return false;
        }

        if (!i.speakerPinsConfigured) {
            i.speakerPins.addI2C(PinFunction::CODEC, Wire, false);
            i.speakerPins.addI2S(PinFunction::CODEC,
                                 waveshare185c::I2S_MCLK, waveshare185c::I2S_BCLK,
                                 waveshare185c::I2S_LRCK, waveshare185c::I2S_DOUT, -1);
            i.speakerPins.addPin(PinFunction::PA, waveshare185c::PA_CTRL, PinLogic::Output);
            i.speakerPinsConfigured = true;
        }

        CodecConfig outCfg;
        outCfg.input_device = ADC_INPUT_NONE;
        outCfg.output_device = DAC_OUTPUT_ALL;
        outCfg.i2s.bits = BIT_LENGTH_16BITS;
        outCfg.i2s.rate = RATE_16K;
        outCfg.i2s.channels = CHANNELS2;
        outCfg.i2s.fmt = I2S_NORMAL;
        outCfg.i2s.mode = MODE_SLAVE;

        Serial.println("ES8311: initialisiere Codec fuer 16 kHz / 16 Bit I2S ...");
        if (!i.speakerBoard.begin(outCfg)) {
            Serial.println("ES8311 Initialisierung fehlgeschlagen; Mikrofon bleibt aktiv, TTS ist deaktiviert.");
            digitalWrite(waveshare185c::PA_CTRL, LOW);
            return false;
        }

        if (!i.speakerBoard.setVolume(i.desiredVolume)) {
            Serial.println("ES8311 Warnung: Lautstaerke konnte nicht gesetzt werden.");
        }
        i.speakerStarted = true;
        Serial.printf("ES8311/Lautsprecher-Codec bereit, Volume=%u%%.\n",
                      static_cast<unsigned>(i.desiredVolume));
    }

    if (!i.playbackMode) {
        digitalWrite(waveshare185c::PA_CTRL, LOW);
        if (!startSpeakerI2s()) {
            Serial.println("Speaker I2S konnte nicht gestartet werden; versuche MIC-I2S wiederherzustellen.");
            startMicI2s();
            return false;
        }
    }

    digitalWrite(waveshare185c::PA_CTRL, HIGH);
    return true;
}

size_t Waveshare185CAudio::readPcm16(int16_t* dst, size_t samples, uint32_t timeoutMs) {
    if (!dst || samples == 0) return 0;
    if (!impl_->micStarted) {
        const uint32_t now = millis();
        if (now - impl_->lastRxDebugAt >= 1000) {
            impl_->lastRxDebugAt = now;
            Serial.println("Waveshare MIC RX: Mikrofon nicht aktiv; Wiederherstellung wird versucht ...");
            if (!restoreMicPath()) {
                Serial.println("Waveshare MIC RX: Wiederherstellung fehlgeschlagen.");
                return 0;
            }
        } else {
            return 0;
        }
    }

    constexpr size_t MAX_MONO = 512;
    const size_t wantMono = std::min(samples, MAX_MONO);
    int16_t stereo[MAX_MONO * 2];
    impl_->i2s.setTimeout(timeoutMs > 0 ? timeoutMs : 1);
    const size_t bytes = impl_->i2s.readBytes(
        reinterpret_cast<char*>(stereo),
        wantMono * 2 * sizeof(int16_t)
    );
    if (bytes == 0) {
        const uint32_t now = millis();
        if (now - impl_->lastRxDebugAt >= 1000) {
            impl_->lastRxDebugAt = now;
            uint8_t reg00 = 0xFF;
            const bool codecRead = readReg(ES7210, 0x00, reg00);
            Serial.printf(
                "Waveshare MIC RX: 0 Bytes (Port=%d RX=%s available=%d lastError=%d ES7210=%s REG00=0x%02X)\n",
                static_cast<int>(impl_->i2s.getPort()),
                impl_->i2s.rxChan() ? "ja" : "nein",
                impl_->i2s.available(),
                impl_->i2s.lastError(),
                codecRead ? "OK" : "I2C-FEHLER",
                reg00
            );
        }
        return 0;
    }
    const size_t frames = bytes / (2 * sizeof(int16_t));

    for (size_t n = 0; n < frames; ++n) {
        const int32_t left = stereo[n * 2];
        const int32_t right = stereo[n * 2 + 1];
        dst[n] = static_cast<int16_t>((left + right) / 2);
    }
    return frames;
}

size_t Waveshare185CAudio::writePcm16(
    const int16_t* src,
    size_t samples,
    uint32_t timeoutMs
) {
    if (!src || samples == 0) return 0;
    if (!ensureSpeaker()) return 0;

    auto& i = *impl_;
    i.i2s.setTimeout(timeoutMs > 0 ? timeoutMs : 1);
    const size_t bytesRequested = samples * sizeof(int16_t);
    const size_t bytesWritten = i.i2s.write(
        static_cast<const void*>(src),
        bytesRequested
    );
    if (bytesWritten == 0) {
        Serial.printf(
            "Waveshare Speaker TX: 0 Bytes (Port=%d TX=%s rate=%lu bits=%u mode=%d lastError=%d)\n",
            static_cast<int>(i.i2s.getPort()),
            i.i2s.txChan() ? "ja" : "nein",
            static_cast<unsigned long>(i.i2s.txSampleRate()),
            static_cast<unsigned>(i.i2s.txDataWidth()),
            static_cast<int>(i.i2s.txSlotMode()),
            i.i2s.lastError()
        );
        return 0;
    }
    return bytesWritten / sizeof(int16_t);
}

bool Waveshare185CAudio::setVolume(uint8_t percent) {
    auto& i = *impl_;
    const uint8_t next = static_cast<uint8_t>(
        std::clamp<int>(percent, 0, 100)
    );
    i.desiredVolume = next;

    if (!i.speakerStarted) {
        Serial.printf("Lautstaerke vorgemerkt: %u%%.\n", static_cast<unsigned>(next));
        return true;
    }

    if (!i.speakerBoard.setVolume(next)) {
        Serial.printf("ES8311: Lautstaerke %u%% konnte nicht gesetzt werden.\n",
                      static_cast<unsigned>(next));
        return false;
    }
    Serial.printf("Lautstaerke: %u%%.\n", static_cast<unsigned>(next));
    return true;
}

uint8_t Waveshare185CAudio::volume() const {
    return impl_
        ? impl_->desiredVolume
        : static_cast<uint8_t>(VOICE_SATELLITE_WAVESHARE_SPEAKER_VOLUME);
}

void Waveshare185CAudio::clearOutput() {
    auto& i = *impl_;
    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, LOW);

    if (i.playbackMode || !i.micStarted) {
        delay(2);
        if (!restoreMicPath()) {
            Serial.println(
                "WARNUNG: Mikrofonpfad konnte nach Speaker-Wiedergabe nicht wiederhergestellt werden."
            );
        }
    }
}
