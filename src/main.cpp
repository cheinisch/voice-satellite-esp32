#include <Arduino.h>
#include <cstring>
#include "board/board_factory.h"
#include "build_info.h"
#include "core/satellite.h"
#include "voice_satellite_config.h"

namespace {
Board& board = selectedBoard();
Satellite satellite(board);
}

void setup() {
    Serial.begin(115200);
    delay(400);

    Serial.println();
    Serial.printf("Voice Satellite %s Build %d\n", VOICE_SATELLITE_VERSION, VOICE_SATELLITE_BUILD);
    Serial.printf("Client: %s\n", VOICE_SATELLITE_CLIENT_NAME);
    Serial.printf("ID: %s (%s)\n", VOICE_SATELLITE_ID, VOICE_SATELLITE_NAME);
    Serial.printf("Board: %s [%s]\n", board.model(), board.profile());
    Serial.printf("Core: %s://%s:%d%s\n", VOICE_SATELLITE_CORE_TLS ? "wss" : "ws", VOICE_SATELLITE_CORE_HOST, VOICE_SATELLITE_CORE_PORT, VOICE_SATELLITE_CORE_PATH);
    const bool tokenConfigured = strlen(VOICE_SATELLITE_CORE_TOKEN) > 0 && strcmp(VOICE_SATELLITE_CORE_TOKEN, "CHANGE_ME") != 0 && strcmp(VOICE_SATELLITE_CORE_TOKEN, "jv_DEIN_TOKEN") != 0;
    Serial.printf("Core auth: %s\n", tokenConfigured ? "Bearer Token konfiguriert" : "KEIN TOKEN");
    Serial.printf("Audio uplink: PCM S16LE, %d Hz, %d channel(s)\n", VOICE_SATELLITE_AUDIO_RATE, VOICE_SATELLITE_AUDIO_CHANNELS);

    if (!satellite.begin()) {
        Serial.println("Satellite konnte nicht vollständig initialisiert werden.");
    }
}

void loop() {
    satellite.loop();
}
