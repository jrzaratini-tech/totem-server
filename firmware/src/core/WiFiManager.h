#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager;

class TotemWiFiManager {
private:
    WiFiManager *wm;

    bool apMode;
    bool connected;
    bool restartOnSave;

    String totemId;

    bool startPortalIfNeeded();
    void processResetButton();
    void applyWiFiTuning();

public:
    TotemWiFiManager();
    ~TotemWiFiManager();

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
