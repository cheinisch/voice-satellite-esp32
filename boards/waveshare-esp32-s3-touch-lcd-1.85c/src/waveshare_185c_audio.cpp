#include "waveshare_185c_audio.h"
#include "waveshare_185c_pins.h"
#include "jarvis_config.h"
#include <ESP_I2S.h>
#include <Wire.h>
#include <AudioBoard.h>
#include <algorithm>

using namespace audio_driver;

struct Waveshare185CAudio::Impl {
    // arduino-audio-driver v0.2.0 uses DriverPins.
    // ES8311 and ES7210 already provide the correct default 7-bit I2C
    // addresses (0x18 and 0x40 respectively) in this driver version.
    DriverPins speakerPins;
    DriverPins micPins;
    AudioDriverES8311Class speakerDriver;
    AudioDriverES7210Class micDriver;
    AudioBoard speakerBoard{speakerDriver, speakerPins};
    AudioBoard micBoard{micDriver, micPins};
    I2SClass i2s{I2S_NUM_1};
    bool started = false;
};

Waveshare185CAudio::Waveshare185CAudio() : impl_(new Impl()) {}
Waveshare185CAudio::~Waveshare185CAudio() { delete impl_; }

bool Waveshare185CAudio::begin() {
    auto& i = *impl_;
    AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Warning);

    i.speakerPins.addI2C(PinFunction::CODEC,
                         waveshare185c::I2C_SCL, waveshare185c::I2C_SDA,
                         0, 400000, Wire);
    i.speakerPins.addI2S(PinFunction::CODEC,
                         waveshare185c::I2S_MCLK, waveshare185c::I2S_BCLK,
                         waveshare185c::I2S_LRCK, waveshare185c::I2S_DOUT, -1);
    i.speakerPins.addPin(PinFunction::PA, waveshare185c::PA_CTRL, PinLogic::Output);

    i.micPins.addI2C(PinFunction::CODEC,
                     waveshare185c::I2C_SCL, waveshare185c::I2C_SDA,
                     0, 400000, Wire);
    i.micPins.addI2S(PinFunction::CODEC,
                     waveshare185c::I2S_MCLK, waveshare185c::I2S_BCLK,
                     waveshare185c::I2S_LRCK, -1, waveshare185c::I2S_DIN);

    CodecConfig outCfg;
    outCfg.input_device = ADC_INPUT_NONE;
    outCfg.output_device = DAC_OUTPUT_ALL;
    outCfg.i2s.bits = BIT_LENGTH_16BITS;
    outCfg.i2s.rate = RATE_16K;
    outCfg.i2s.channels = CHANNELS2;
    outCfg.i2s.fmt = I2S_NORMAL;
    outCfg.i2s.mode = MODE_SLAVE;

    CodecConfig inCfg;
    inCfg.input_device = ADC_INPUT_ALL;
    inCfg.output_device = DAC_OUTPUT_NONE;
    inCfg.i2s.bits = BIT_LENGTH_16BITS;
    inCfg.i2s.rate = RATE_16K;
    inCfg.i2s.channels = CHANNELS2;
    inCfg.i2s.fmt = I2S_NORMAL;
    inCfg.i2s.mode = MODE_SLAVE;

    if (!i.speakerBoard.begin(outCfg)) {
        Serial.println("ES8311 Initialisierung fehlgeschlagen.");
        return false;
    }
    i.speakerBoard.setVolume(JARVIS_WAVESHARE_SPEAKER_VOLUME);

    if (!i.micBoard.begin(inCfg)) {
        Serial.println("ES7210 Initialisierung fehlgeschlagen.");
        return false;
    }
    i.micBoard.setInputVolume(JARVIS_WAVESHARE_MIC_GAIN);

    i.i2s.setPins(waveshare185c::I2S_BCLK, waveshare185c::I2S_LRCK,
                  waveshare185c::I2S_DOUT, waveshare185c::I2S_DIN,
                  waveshare185c::I2S_MCLK);
    if (!i.i2s.begin(I2S_MODE_STD, JARVIS_AUDIO_RATE,
                     I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.printf("Waveshare I2S Initialisierung fehlgeschlagen: %d\n", i.i2s.lastError());
        return false;
    }

    pinMode(waveshare185c::PA_CTRL, OUTPUT);
    digitalWrite(waveshare185c::PA_CTRL, HIGH);
    i.started = true;
    Serial.println("Waveshare Audio: ES7210 + ES8311 + I2S bereit.");
    return true;
}

size_t Waveshare185CAudio::readPcm16(int16_t* dst, size_t samples, uint32_t) {
    if (!impl_->started || !dst || samples == 0) return 0;

    constexpr size_t MAX_MONO = 512;
    const size_t wantMono = std::min(samples, MAX_MONO);
    int16_t stereo[MAX_MONO * 2];
    const size_t bytes = impl_->i2s.readBytes(reinterpret_cast<char*>(stereo), wantMono * 2 * sizeof(int16_t));
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
