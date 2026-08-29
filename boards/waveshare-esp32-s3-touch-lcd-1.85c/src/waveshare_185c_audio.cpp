#include "waveshare_185c_audio.h"
#include "waveshare_185c_pins.h"
#include "jarvis_config.h"
#include <ESP_I2S.h>
#include <Wire.h>
#include <AudioBoard.h>
#include <algorithm>

using namespace audio_driver;

namespace {

// ES7210 configuration follows the Waveshare V2 reference firmware for
// ESP32-S3-Touch-LCD-1.85C: 16 kHz, 16-bit, stereo, standard I2S,
// MCLK = 256 * Fs = 4.096 MHz, 2.87 V mic bias and 36 dB analog gain.
//
// We intentionally configure the ES7210 directly over the already-running
// shared Wire bus. The generic arduino-audio-driver ES7210 wrapper uses a
// different init sequence and did not produce RX samples on this board.
constexpr uint8_t ES7210 = waveshare185c::ES7210_ADDR;

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
        // Software reset / initial state
        {0x00, 0xFF},
        {0x00, 0x32},
        {0x09, 0x30},
        {0x0A, 0x30},

        // ADC1..4 high-pass filters
        {0x23, 0x2A},
        {0x22, 0x0A},
        {0x21, 0x2A},
        {0x20, 0x0A},

        // Standard I2S, 16 bit, stereo/TDM disabled
        {0x11, 0x60},
        {0x12, 0x00},

        // Analog power + microphone bias
        {0x40, 0xC3},
        {0x41, 0x70}, // MIC1/2 bias 2.87 V
        {0x42, 0x70}, // MIC3/4 bias 2.87 V

        // 36 dB analog gain. ES7210 gain code 13 OR 0x10 = 0x1D.
        {0x43, 0x1D},
        {0x44, 0x1D},
        {0x45, 0x1D},
        {0x46, 0x1D},

        // Power on MIC1..4
        {0x47, 0x08},
        {0x48, 0x08},
        {0x49, 0x08},
        {0x4A, 0x08},

        // 16 kHz with MCLK ratio 256 -> 4.096 MHz.
        // Values are the Waveshare/Espressif ES7210 coefficient table.
        {0x07, 0x20}, // OSR
        {0x02, 0xC1}, // adc_div=1, doubler=1, dll=1
        {0x04, 0x01}, // LRCK divider high
        {0x05, 0x00}, // LRCK divider low

        // Power / enable sequence
        {0x06, 0x04},
        {0x4B, 0x0F},
        {0x4C, 0x0F},
        {0x00, 0x71},
        {0x00, 0x41},

        // ADC digital volume: 0 dB (0xBF). Keep the first hardware test
        // conservative; analog microphone gain is already 36 dB.
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

} // namespace

struct Waveshare185CAudio::Impl {
    DriverPins speakerPins;
    AudioDriverES8311Class speakerDriver;
    AudioBoard speakerBoard{speakerDriver, speakerPins};
    // Match the official Waveshare V2 Arduino example: let ESP_I2S pick
    // the available controller automatically instead of forcing I2S1.
    I2SClass i2s;
    uint32_t lastRxDebugAt = 0;
    bool micStarted = false;
    bool speakerStarted = false;
    bool playbackMode = false;
    bool speakerPinsConfigured = false;
};

Waveshare185CAudio::Waveshare185CAudio() : impl_(new Impl()) {}
Waveshare185CAudio::~Waveshare185CAudio() { delete impl_; }

bool Waveshare185CAudio::startMicI2s() {
    auto& i = *impl_;
    if (i.i2s.txChan() || i.i2s.rxChan()) {
        i.i2s.end();
        delay(2);
    }

    // STT uses an RX-only I2S channel. Keeping TX detached here avoids
    // full-duplex channel interactions in Arduino-ESP32 3.3.x.
    i.i2s.setPins(waveshare185c::I2S_BCLK, waveshare185c::I2S_LRCK,
                  -1, waveshare185c::I2S_DIN, waveshare185c::I2S_MCLK);
    i.i2s.setTimeout(1000);
    if (!i.i2s.begin(I2S_MODE_STD, JARVIS_AUDIO_RATE,
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
                  static_cast<unsigned>(JARVIS_AUDIO_RATE));
    return true;
}

bool Waveshare185CAudio::restoreMicPath() {
    auto& i = *impl_;

    // Speaker playback changes the shared BCLK/LRCK/MCLK timing from the
    // microphone's stereo receive format to mono TX.  Merely recreating the
    // RX channel is not sufficient on the ES7210 after that transition: the
    // ADC must see stable clocks and then be programmed again.
    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, LOW);

    i.micStarted = false;
    if (!startMicI2s()) {
        Serial.println("Waveshare MIC Restore: RX-I2S konnte nicht gestartet werden.");
        return false;
    }

    // Give MCLK/BCLK/LRCK a moment to become stable before touching the ADC.
    delay(20);
    if (!initEs7210Waveshare()) {
        Serial.println("Waveshare MIC Restore: ES7210 konnte nicht neu initialisiert werden.");
        i.i2s.end();
        i.micStarted = false;
        i.playbackMode = false;
        return false;
    }

    // Discard stale DMA data accumulated while the codec was being brought up.
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

    // Waveshare's playback reference uses a dedicated TX path with the
    // left I2S slot. We use the Jarvis 16 kHz rate so no extra resampling
    // is needed for TTS or the local speaker test.
    i.i2s.setPins(waveshare185c::I2S_BCLK, waveshare185c::I2S_LRCK,
                  waveshare185c::I2S_DOUT, -1, waveshare185c::I2S_MCLK);
    i.i2s.setTimeout(1000);
    if (!i.i2s.begin(I2S_MODE_STD, JARVIS_AUDIO_RATE,
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

    // IMPORTANT: bring up the microphone path first and independently.
    // A speaker/ES8311 problem must never prevent STT microphone capture.
    // The ES8311 is therefore initialized lazily on the first TTS frame.
    if (!initEs7210Waveshare()) {
        Serial.println("ES7210 Waveshare-Initialisierung fehlgeschlagen.");
        return false;
    }

    if (!startMicI2s()) return false;
    delay(20);

    // Keep the power amplifier muted until the first actual playback.
    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, LOW);

    Serial.printf("Waveshare MIC bereit (Port %d, RX=%s, TX=%s, %u Hz/16-bit/stereo).\n",
                  static_cast<int>(i.i2s.getPort()),
                  i.i2s.rxChan() ? "ja" : "nein",
                  i.i2s.txChan() ? "ja" : "nein",
                  static_cast<unsigned>(JARVIS_AUDIO_RATE));
    Serial.println("ES8311/Lautsprecher wird erst bei der ersten TTS-Ausgabe initialisiert.");
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
            // Reuse the shared Wire bus. Do not call Wire.begin()/Wire.end().
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
        outCfg.i2s.mode = MODE_SLAVE; // ESP32-S3 provides BCLK/LRCK/MCLK

        Serial.println("ES8311: initialisiere Codec fuer 16 kHz / 16 Bit I2S ...");
        if (!i.speakerBoard.begin(outCfg)) {
            Serial.println("ES8311 Initialisierung fehlgeschlagen; Mikrofon bleibt aktiv, TTS ist deaktiviert.");
            digitalWrite(waveshare185c::PA_CTRL, LOW);
            return false;
        }

        if (!i.speakerBoard.setVolume(JARVIS_WAVESHARE_SPEAKER_VOLUME)) {
            Serial.println("ES8311 Warnung: Lautstaerke konnte nicht gesetzt werden.");
        }
        i.speakerStarted = true;
        Serial.printf("ES8311/Lautsprecher-Codec bereit, Volume=%d%%.\n", JARVIS_WAVESHARE_SPEAKER_VOLUME);
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
    const size_t bytes = impl_->i2s.readBytes(reinterpret_cast<char*>(stereo), wantMono * 2 * sizeof(int16_t));
    if (bytes == 0) {
        const uint32_t now = millis();
        if (now - impl_->lastRxDebugAt >= 1000) {
            impl_->lastRxDebugAt = now;
            uint8_t reg00 = 0xFF;
            const bool codecRead = readReg(ES7210, 0x00, reg00);
            Serial.printf("Waveshare MIC RX: 0 Bytes (Port=%d RX=%s available=%d lastError=%d ES7210=%s REG00=0x%02X)\n",
                          static_cast<int>(impl_->i2s.getPort()),
                          impl_->i2s.rxChan() ? "ja" : "nein",
                          impl_->i2s.available(),
                          impl_->i2s.lastError(),
                          codecRead ? "OK" : "I2C-FEHLER",
                          reg00);
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

size_t Waveshare185CAudio::writePcm16(const int16_t* src, size_t samples, uint32_t timeoutMs) {
    if (!src || samples == 0) return 0;
    if (!ensureSpeaker()) return 0;

    auto& i = *impl_;
    i.i2s.setTimeout(timeoutMs > 0 ? timeoutMs : 1);
    const size_t bytesRequested = samples * sizeof(int16_t);
    const size_t bytesWritten = i.i2s.write(static_cast<const void*>(src), bytesRequested);
    if (bytesWritten == 0) {
        Serial.printf("Waveshare Speaker TX: 0 Bytes (Port=%d TX=%s rate=%lu bits=%u mode=%d lastError=%d)\n",
                      static_cast<int>(i.i2s.getPort()),
                      i.i2s.txChan() ? "ja" : "nein",
                      static_cast<unsigned long>(i.i2s.txSampleRate()),
                      static_cast<unsigned>(i.i2s.txDataWidth()),
                      static_cast<int>(i.i2s.txSlotMode()),
                      i.i2s.lastError());
        return 0;
    }
    return bytesWritten / sizeof(int16_t);
}

void Waveshare185CAudio::clearOutput() {
    auto& i = *impl_;
    // Mute the external NS4150B PA between speech turns to reduce noise/echo.
    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, LOW);

    // Playback uses a dedicated TX-only channel.  Restore both the RX
    // transport *and* the ES7210 register state.  Also repair the microphone
    // if a previous transition failed, so the next STT/TTS recording does not
    // start in a permanently broken state.
    if (i.playbackMode || !i.micStarted) {
        delay(2);
        if (!restoreMicPath()) {
            Serial.println("WARNUNG: Mikrofonpfad konnte nach Speaker-Wiedergabe nicht wiederhergestellt werden.");
        }
    }
}

