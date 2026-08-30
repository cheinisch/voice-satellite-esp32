#include "network/wifi_manager.h"
#include <WiFi.h>
#include "ai-voice-satellite_config.h"
#include <time.h>

bool WifiManager::connect(uint32_t timeoutMs) {
    if (String(AIVOICE-SATELLITE_WIFI_SSID) == "CHANGE_ME") {
        Serial.println("WLAN nicht konfiguriert: include/local_config.h anlegen.");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(AIVOICE-SATELLITE_WIFI_SSID, AIVOICE-SATELLITE_WIFI_PASSWORD);

    Serial.printf("WLAN: verbinde mit %s", AIVOICE-SATELLITE_WIFI_SSID);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WLAN verbunden: %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        configTzTime(AIVOICE-SATELLITE_TIMEZONE_POSIX, AIVOICE-SATELLITE_NTP_SERVER);
        Serial.printf("Zeitabgleich: NTP=%s TZ=%s\n", AIVOICE-SATELLITE_NTP_SERVER, AIVOICE-SATELLITE_TIMEZONE_POSIX);
        return true;
    }

    Serial.println("WLAN-Verbindung fehlgeschlagen; erneuter Versuch folgt.");
    WiFi.disconnect(false, false);
    return false;
}

bool WifiManager::begin() {
    return connect(AIVOICE-SATELLITE_WIFI_TIMEOUT_MS);
}

void WifiManager::loop() {
    if (connected()) return;
    if (millis() < nextRetryAt_) return;
    nextRetryAt_ = millis() + AIVOICE-SATELLITE_RECONNECT_MS;
    connect(5000);
}

bool WifiManager::connected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WifiManager::ip() const {
    return connected() ? WiFi.localIP().toString() : String();
}
