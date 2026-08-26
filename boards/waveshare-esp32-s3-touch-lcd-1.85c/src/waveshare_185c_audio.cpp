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
    bool started = false;
};

Waveshare185CAudio::Waveshare185CAudio() : impl_(new Impl()) {}
Waveshare185CAudio::~Waveshare185CAudio() { delete impl_; }

bool Waveshare185CAudio::begin() {
    auto& i = *impl_;
    AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Warning);

    Serial.printf("Audio I2C Probe: ES8311(0x18)=%s ES7210(0x40)=%s\n",
                  wireProbe(waveshare185c::ES8311_ADDR) ? "OK" : "FEHLT",
                  wireProbe(waveshare185c::ES7210_ADDR) ? "OK" : "FEHLT");

    // Speaker remains on the generic ES8311 wrapper for now. It reuses the
    // shared Wire instance and must not call Wire.begin()/Wire.end().
    i.speakerPins.addI2C(PinFunction::CODEC, Wire, false);
    i.speakerPins.addI2S(PinFunction::CODEC,
                         waveshare185c::I2S_MCLK, waveshare185c::I2S_BCLK,
                         waveshare185c::I2S_LRCK, waveshare185c::I2S_DOUT, -1);
    i.speakerPins.addPin(PinFunction::PA, waveshare185c::PA_CTRL, PinLogic::Output);

    CodecConfig outCfg;
    outCfg.input_device = ADC_INPUT_NONE;
    outCfg.output_device = DAC_OUTPUT_ALL;
    outCfg.i2s.bits = BIT_LENGTH_16BITS;
    outCfg.i2s.rate = RATE_16K;
    outCfg.i2s.channels = CHANNELS2;
    outCfg.i2s.fmt = I2S_NORMAL;
    outCfg.i2s.mode = MODE_SLAVE; // codec slave, ESP32-S3 is I2S clock master

    if (!i.speakerBoard.begin(outCfg)) {
        Serial.println("ES8311 Initialisierung fehlgeschlagen.");
        return false;
    }
    i.speakerBoard.setVolume(JARVIS_WAVESHARE_SPEAKER_VOLUME);

    // Use the exact Waveshare V2 microphone codec profile instead of the
    // generic audio-driver ES7210 wrapper.
    if (!initEs7210Waveshare()) {
        Serial.println("ES7210 Waveshare-Initialisierung fehlgeschlagen.");
        return false;
    }

    i.i2s.setPins(waveshare185c::I2S_BCLK, waveshare185c::I2S_LRCK,
                  waveshare185c::I2S_DOUT, waveshare185c::I2S_DIN,
                  waveshare185c::I2S_MCLK);
    i.i2s.setTimeout(1000);
    if (!i.i2s.begin(I2S_MODE_STD, JARVIS_AUDIO_RATE,
                     I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.printf("Waveshare I2S Initialisierung fehlgeschlagen: %d\n", i.i2s.lastError());
        return false;
    }

    if (!i.i2s.rxChan()) {
        Serial.println("Waveshare I2S Fehler: RX-Kanal wurde nicht angelegt.");
        return false;
    }

    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, HIGH);
    i.started = true;
    Serial.printf("Waveshare Audio: ES7210(Waveshare) + ES8311 + I2S bereit (Port %d, RX=%s, TX=%s, %u Hz/16-bit/stereo).\n",
                  static_cast<int>(i.i2s.getPort()),
                  i.i2s.rxChan() ? "ja" : "nein",
                  i.i2s.txChan() ? "ja" : "nein",
                  static_cast<unsigned>(JARVIS_AUDIO_RATE));
    return true;
}

size_t Waveshare185CAudio::readPcm16(int16_t* dst, size_t samples, uint32_t timeoutMs) {
    if (!impl_->started || !dst || samples == 0) return 0;

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

size_t Waveshare185CAudio::writePcm16(const int16_t* src, size_t samples, uint32_t) {
    if (!impl_->started || !src || samples == 0) return 0;

    constexpr size_t BLOCK = 256;
    int16_t stereo[BLOCK * 2];
    size_t total = 0;
    while (total < samples) {
        const size_t count = std::min(samples - total, BLOCK);
        for (size_t n = 0; n < count; ++n) {
            const int16_t s = src[total + n];
            stereo[n * 2] = s;
            stereo[n * 2 + 1] = s;
        }
        const size_t written = impl_->i2s.write(reinterpret_cast<const uint8_t*>(stereo), count * 2 * sizeof(int16_t));
        const size_t monoWritten = written / (2 * sizeof(int16_t));
        total += monoWritten;
        if (monoWritten < count) break;
    }
    return total;
}

void Waveshare185CAudio::clearOutput() {
    // Kein öffentlicher DMA-Flush in ESP_I2S notwendig. TTS-Frames werden sequenziell geschrieben.
}
