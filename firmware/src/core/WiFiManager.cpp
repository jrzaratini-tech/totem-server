#include "core/WiFiManager.h"
#include "Config.h"

#include <WiFiManager.h>

TotemWiFiManager::TotemWiFiManager() {
    wm = new WiFiManager();
    apMode = false;
    connected = false;
    restartOnSave = false;
}

TotemWiFiManager::~TotemWiFiManager() {
    if (wm) {
        delete wm;
        wm = nullptr;
    }
}

void TotemWiFiManager::applyWiFiTuning() {
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
}

void TotemWiFiManager::begin(const String &totemId) {
    this->totemId = totemId;

    pinMode(PIN_BTN_RESET_WIFI, (PIN_BTN_RESET_WIFI >= 34) ? INPUT : INPUT_PULLUP);
    applyWiFiTuning();

    Serial.println("[WiFi] Configuring WiFiManager...");
    wm->setDebugOutput(true);
    wm->setCaptivePortalEnable(true);
    wm->setConnectTimeout(WIFI_TIMEOUT / 1000);
    wm->setConfigPortalTimeout(0);
    wm->setBreakAfterConfig(true);

    wm->setSaveConfigCallback([this]() {
        Serial.println("[WiFi] Save config callback triggered");
        this->restartOnSave = true;
    });

    wm->setAPCallback([this](WiFiManager *w) {
        (void)w;
        Serial.println("[WiFi] AP Mode activated - SSID: Totem Config");
        this->apMode = true;
        this->connected = false;
    });

    Serial.println("[WiFi] Starting autoConnect...");
    if (!startPortalIfNeeded()) {
        apMode = false;
        connected = (WiFi.status() == WL_CONNECTED);
        Serial.printf("[WiFi] AutoConnect completed - Connected: %s, IP: %s\n", 
                     connected ? "YES" : "NO", 
                     connected ? WiFi.localIP().toString().c_str() : "N/A");
        return;
    }

    apMode = false;
    connected = (WiFi.status() == WL_CONNECTED);
    Serial.printf("[WiFi] Portal finished - Connected: %s\n", connected ? "YES" : "NO");

    if (restartOnSave) {
        Serial.println("[WiFi] Restarting after config save...");
        delay(250);
        ESP.restart();
    }
}

bool TotemWiFiManager::startPortalIfNeeded() {
    const char *apName = "Totem Config";

    bool ok = wm->autoConnect(apName);
    if (!ok) {
        apMode = true;
        connected = false;
        return true;
    }

    return false;
}

void TotemWiFiManager::processResetButton() {
    // Desabilitado para evitar resets acidentais por pino flutuante
    return;
}

void TotemWiFiManager::loop() {
    processResetButton();
    connected = (WiFi.status() == WL_CONNECTED);
}

bool TotemWiFiManager::connectToSaved() {
    if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        apMode = false;
        return true;
    }

    return false;
}

bool TotemWiFiManager::connect(const String &ssid, const String &password) {
    apMode = false;
    connected = false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    const unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < (unsigned long)WIFI_TIMEOUT) {
        delay(25);
        yield();
    }

    connected = (WiFi.status() == WL_CONNECTED);
    apMode = !connected;
    return connected;
}

void TotemWiFiManager::resetWiFiSettings() {
    wm->resetSettings();
    WiFi.disconnect(true, true);
    delay(250);
    #if !defined(DISABLE_AUTO_RESTART) || (DISABLE_AUTO_RESTART == 0)
    ESP.restart();
    #endif
}

bool TotemWiFiManager::isConnected() const {
    return connected && WiFi.status() == WL_CONNECTED;
}

bool TotemWiFiManager::isAPMode() const {
    return apMode;
}

String TotemWiFiManager::getIP() const {
    if (WiFi.status() != WL_CONNECTED) return "0.0.0.0";
    return WiFi.localIP().toString();
}

int TotemWiFiManager::getRSSI() const {
    if (WiFi.status() != WL_CONNECTED) return -127;
    return WiFi.RSSI();
}
