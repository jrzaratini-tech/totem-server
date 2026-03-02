#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

class WiFiManager {
private:
    AsyncWebServer server;
    DNSServer dnsServer;
    Preferences preferences;

    bool apMode;
    bool connected;
    unsigned long lastReconnectAttempt;

    bool connecting;
    unsigned long connectStartMs;
    String pendingSsid;
    String pendingPass;

    String totemId;

    void startAPMode();
    void setupWebServer();

    void startConnect(const String &ssid, const String &password);

public:
    WiFiManager();

    void begin(const String &totemId);
    void loop();

    bool connectToSaved();
    bool connect(const String &ssid, const String &password);

    void resetWiFiSettings();

    bool isConnected() const;
    bool isAPMode() const;

    String getIP() const;
    int getRSSI() const;
};

#endif
