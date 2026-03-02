#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <SPIFFS.h>

class StorageManager {
private:
    Preferences prefs;
    bool spiffsOk;

public:
    StorageManager();

    void begin();

    bool hasSPIFFS() const;

    String getTotemId();
    void setTotemId(const String &id);

    String getDeviceToken();
    void setDeviceToken(const String &token);

    int getAudioVersion();
    void setAudioVersion(int v);
};

#endif
