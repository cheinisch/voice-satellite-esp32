#include <Arduino.h>
#include <cstring>
#include "board/board_factory.h"
#include "build_info.h"
#include "core/satellite.h"
#include "jarvis_config.h"

namespace {
Board& board = selectedBoard();
Satellite satellite(board);
}

void setup() {
    Serial.begin(115200);
    delay(400);

    Serial.println();
    Serial.printf("Jarvis ESP32 Satellite %s Build %d\n", JARVIS_SATELLITE_VERSION, JARVIS_SATELLITE_BUILD);
    Serial.printf("Client: %s\n", JARVIS_CLIENT_NAME);
    Serial.printf("ID: %s (%s)\n", JARVIS_SATELLITE_ID, JARVIS_SATELLITE_NAME);
    Serial.printf("Board: %s [%s]\n", board.model(), board.profile());
    Serial.printf("Core: %s://%s:%d%s\n", JARVIS_CORE_TLS ? "wss" : "ws", JARVIS_CORE_HOST, JARVIS_CORE_PORT, JARVIS_CORE_PATH);
    const bool tokenConfigured = strlen(JARVIS_CORE_TOKEN) > 0 && strcmp(JARVIS_CORE_TOKEN, "CHANGE_ME") != 0 && strcmp(JARVIS_CORE_TOKEN, "jv_DEIN_TOKEN") != 0;
    Serial.printf("Core auth: %s\n", tokenConfigured ? "Bearer Token konfiguriert" : "KEIN TOKEN");
    Serial.printf("Audio uplink: PCM S16LE, %d Hz, %d channel(s)\n", JARVIS_AUDIO_RATE, JARVIS_AUDIO_CHANNELS);

    if (!satellite.begin()) {
        Serial.println("Satellite konnte nicht vollständig initialisiert werden.");
    }
}

void loop() {
    satellite.loop();
}
