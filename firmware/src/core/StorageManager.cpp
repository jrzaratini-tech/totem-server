#include "core/StorageManager.h"
#include "Config.h"

StorageManager::StorageManager() {
    spiffsOk = false;
}

void StorageManager::begin() {
    spiffsOk = SPIFFS.begin(true);
}

bool StorageManager::hasSPIFFS() const { return spiffsOk; }

String StorageManager::getTotemId() {
    if (!prefs.begin("device", true)) {
        prefs.begin("device", false);
    }
    String v = prefs.getString("id", DEFAULT_TOTEM_ID);
    prefs.end();
    v.trim();
    if (v.length() == 0) v = DEFAULT_TOTEM_ID;
    return v;
}

void StorageManager::setTotemId(const String &id) {
    String v = id;
    v.trim();
    if (v.length() == 0) return;
    prefs.begin("device", false);
    prefs.putString("id", v);
    prefs.end();
}

String StorageManager::getDeviceToken() {
    if (!prefs.begin("device", true)) {
        prefs.begin("device", false);
    }
    String v = prefs.getString("token", DEVICE_TOKEN);
    prefs.end();
    v.trim();
    return v;
}

void StorageManager::setDeviceToken(const String &token) {
    String v = token;
    v.trim();
    prefs.begin("device", false);
    prefs.putString("token", v);
    prefs.end();
}

int StorageManager::getAudioVersion() {
    if (!prefs.begin("audio", true)) {
        prefs.begin("audio", false);
    }
    int v = prefs.getInt("ver", 0);
    prefs.end();
    return v;
}

void StorageManager::setAudioVersion(int v) {
    prefs.begin("audio", false);
    prefs.putInt("ver", v);
    prefs.end();
}
