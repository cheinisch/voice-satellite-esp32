#include <Arduino.h>
#include <cstring>
#include "board/board_factory.h"
#include "build_info.h"
#include "core/satellite.h"
#include "ai-voice-satellite_config.h"

namespace {
Board& board = selectedBoard();
Satellite satellite(board);
}

void setup() {
    Serial.begin(115200);
    delay(400);

    Serial.println();
    Serial.printf("Ai-Voice-Satellite ESP32 Satellite %s Build %d\n", AIVOICE-SATELLITE_SATELLITE_VERSION, AIVOICE-SATELLITE_SATELLITE_BUILD);
    Serial.printf("Client: %s\n", AIVOICE-SATELLITE_CLIENT_NAME);
    Serial.printf("ID: %s (%s)\n", AIVOICE-SATELLITE_SATELLITE_ID, AIVOICE-SATELLITE_SATELLITE_NAME);
    Serial.printf("Board: %s [%s]\n", board.model(), board.profile());
    Serial.printf("Core: %s://%s:%d%s\n", AIVOICE-SATELLITE_CORE_TLS ? "wss" : "ws", AIVOICE-SATELLITE_CORE_HOST, AIVOICE-SATELLITE_CORE_PORT, AIVOICE-SATELLITE_CORE_PATH);
    const bool tokenConfigured = strlen(AIVOICE-SATELLITE_CORE_TOKEN) > 0 && strcmp(AIVOICE-SATELLITE_CORE_TOKEN, "CHANGE_ME") != 0 && strcmp(AIVOICE-SATELLITE_CORE_TOKEN, "jv_DEIN_TOKEN") != 0;
    Serial.printf("Core auth: %s\n", tokenConfigured ? "Bearer Token konfiguriert" : "KEIN TOKEN");
    Serial.printf("Audio uplink: PCM S16LE, %d Hz, %d channel(s)\n", AIVOICE-SATELLITE_AUDIO_RATE, AIVOICE-SATELLITE_AUDIO_CHANNELS);

    if (!satellite.begin()) {
        Serial.println("Satellite konnte nicht vollständig initialisiert werden.");
    }
}

void loop() {
    satellite.loop();
}
