#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "Config.h"

struct ConfigData {
    EffectMode mode;
    uint32_t color;
    int speed;
    int duration;
    int maxBrightness;
};

class ConfigManager {
private:
    Preferences prefs;
    ConfigData cfg;
    int audioVersion;

    void load();
    void save();

public:
    ConfigManager();

    void begin();

    bool updateFromJson(const String &json);

    ConfigData getEffectConfig() const;

    int getAudioVersion() const;
    void setAudioVersion(int v);

    uint32_t getColor() const;
    void cycleColor();

    int getBrightness() const;
    void setBrightness(int b);
};

#endif
