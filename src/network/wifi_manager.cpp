#include "network/wifi_manager.h"
#include <WiFi.h>
#include "jarvis_config.h"
#include <time.h>

bool WifiManager::connect(uint32_t timeoutMs) {
    if (String(JARVIS_WIFI_SSID) == "CHANGE_ME") {
        Serial.println("WLAN nicht konfiguriert: include/local_config.h anlegen.");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(JARVIS_WIFI_SSID, JARVIS_WIFI_PASSWORD);

    Serial.printf("WLAN: verbinde mit %s", JARVIS_WIFI_SSID);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WLAN verbunden: %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        configTzTime(JARVIS_TIMEZONE_POSIX, JARVIS_NTP_SERVER);
        Serial.printf("Zeitabgleich: NTP=%s TZ=%s\n", JARVIS_NTP_SERVER, JARVIS_TIMEZONE_POSIX);
        return true;
    }

    Serial.println("WLAN-Verbindung fehlgeschlagen; erneuter Versuch folgt.");
    WiFi.disconnect(false, false);
    return false;
}

bool WifiManager::begin() {
    return connect(JARVIS_WIFI_TIMEOUT_MS);
}

void WifiManager::loop() {
    if (connected()) return;
    if (millis() < nextRetryAt_) return;
    nextRetryAt_ = millis() + JARVIS_RECONNECT_MS;
    connect(5000);
}

bool WifiManager::connected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WifiManager::ip() const {
    return connected() ? WiFi.localIP().toString() : String();
}
