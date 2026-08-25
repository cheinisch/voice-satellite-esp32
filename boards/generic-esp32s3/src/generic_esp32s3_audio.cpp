#include "generic_esp32s3_audio.h"
#include "jarvis_config.h"
#include <ESP_I2S.h>
#include <algorithm>

namespace {
I2SClass micI2S(I2S_NUM_0);
I2SClass spkI2S(I2S_NUM_1);
}

bool GenericEsp32S3Audio::begin() {
    micI2S.setPins(JARVIS_GENERIC_MIC_BCLK_PIN, JARVIS_GENERIC_MIC_WS_PIN, -1, JARVIS_GENERIC_MIC_DATA_PIN, -1);
    const int8_t micSlot = JARVIS_GENERIC_MIC_RIGHT_SLOT ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT;
    if (!micI2S.begin(I2S_MODE_STD, JARVIS_AUDIO_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, micSlot)) {
        Serial.printf("Generic MIC I2S Fehler: %d\n", micI2S.lastError());
        return false;
    }

    spkI2S.setPins(JARVIS_GENERIC_SPK_BCLK_PIN, JARVIS_GENERIC_SPK_WS_PIN, JARVIS_GENERIC_SPK_DATA_PIN, -1, -1);
    if (!spkI2S.begin(I2S_MODE_STD, JARVIS_AUDIO_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        Serial.printf("Generic SPK I2S Fehler: %d\n", spkI2S.lastError());
        micI2S.end();
        return false;
    }
    started_ = true;
    return true;
}

size_t GenericEsp32S3Audio::readPcm16(int16_t* dst, size_t samples, uint32_t) {
    if (!started_ || !dst || !samples) return 0;
    constexpr size_t MAX_RAW = 512;
    int32_t raw[MAX_RAW];
    const size_t want = std::min(samples, MAX_RAW);
    const size_t bytes = micI2S.readBytes(reinterpret_cast<char*>(raw), want * sizeof(int32_t));
    const size_t got = bytes / sizeof(int32_t);
    for (size_t i = 0; i < got; ++i) {
        int32_t value = raw[i] >> JARVIS_GENERIC_MIC_SHIFT;
        value = std::max<int32_t>(-32768, std::min<int32_t>(32767, value));
        dst[i] = static_cast<int16_t>(value);
    }
    return got;
}

size_t GenericEsp32S3Audio::writePcm16(const int16_t* src, size_t samples, uint32_t) {
    if (!started_ || !src || !samples) return 0;
    return spkI2S.write(reinterpret_cast<const uint8_t*>(src), samples * sizeof(int16_t)) / sizeof(int16_t);
}

void GenericEsp32S3Audio::clearOutput() {
    // ESP_I2S verwaltet die DMA-Queue selbst. Für 0.1.x ist kein harter Flush nötig.
}
