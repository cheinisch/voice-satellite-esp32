#include "satellite1_1_audio.h"
#include "satellite1_1_pins.h"
#include <ESP_I2S.h>
#include <algorithm>

namespace {
I2SClass sat1I2S(I2S_NUM_0);
}

bool Satellite11Audio::begin() {
    // Satellite1.1 native audio path is 48 kHz, stereo, 32-bit. The Jarvis
    // protocol remains 16 kHz PCM16 mono; conversion happens in this driver.
    sat1I2S.setPins(
        SAT1_I2S_BCLK_PIN,
        SAT1_I2S_LRCLK_PIN,
        SAT1_I2S_DOUT_PIN,
        SAT1_I2S_DIN_PIN,
        SAT1_I2S_MCLK_PIN
    );
    if (!sat1I2S.begin(I2S_MODE_STD, SAT1_NATIVE_AUDIO_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.printf("Satellite1.1 I2S Fehler: %d\n", sat1I2S.lastError());
        return false;
    }
    started_ = true;
    return true;
}

size_t Satellite11Audio::readPcm16(int16_t* dst, size_t samples, uint32_t) {
    if (!started_ || !dst || samples == 0) return 0;

    // 48 kHz stereo -> 16 kHz mono: consume 3 stereo frames per output sample.
    constexpr size_t kMaxOut = 160;
    constexpr size_t kFramesPerOut = 3;
    constexpr size_t kChannels = 2;
    const size_t outWant = std::min(samples, kMaxOut);
    int32_t raw[kMaxOut * kFramesPerOut * kChannels];
    const size_t rawWords = outWant * kFramesPerOut * kChannels;
    const size_t bytes = sat1I2S.readBytes(reinterpret_cast<char*>(raw), rawWords * sizeof(int32_t));
    const size_t frames = (bytes / sizeof(int32_t)) / kChannels;
    const size_t outCount = std::min(outWant, frames / kFramesPerOut);

    for (size_t i = 0; i < outCount; ++i) {
        // Use the first XMOS channel for Jarvis voice uplink. XMOS performs the
        // microphone front-end processing before the I2S stream reaches ESP32.
        const int32_t sample32 = raw[(i * kFramesPerOut) * kChannels];
        int32_t sample16 = sample32 >> 16;
        sample16 = std::max<int32_t>(-32768, std::min<int32_t>(32767, sample16));
        dst[i] = static_cast<int16_t>(sample16);
    }
    return outCount;
}

size_t Satellite11Audio::writePcm16(const int16_t* src, size_t samples, uint32_t) {
    if (!started_ || !src || samples == 0) return 0;

    // 16 kHz mono -> 48 kHz stereo 32-bit. Repeat every sample three times and
    // mirror it to both channels. The HAT DAC/amplifier consumes this stream.
    constexpr size_t kMaxIn = 128;
    const size_t inCount = std::min(samples, kMaxIn);
    int32_t out[kMaxIn * 3 * 2];
    size_t pos = 0;
    for (size_t i = 0; i < inCount; ++i) {
        const int32_t value = static_cast<int32_t>(src[i]) << 16;
        for (int repeat = 0; repeat < 3; ++repeat) {
            out[pos++] = value;
            out[pos++] = value;
        }
    }

    const size_t bytes = sat1I2S.write(reinterpret_cast<uint8_t*>(out), pos * sizeof(int32_t));
    return (bytes / sizeof(int32_t)) / 6;
}

void Satellite11Audio::clearOutput() {
    // ESP_I2S does not expose a portable TX-buffer flush across all supported
    // Arduino-ESP32 versions. Future full-duplex work can add an IDF-level flush.
}
