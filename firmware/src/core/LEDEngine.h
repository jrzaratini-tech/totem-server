#ifndef LED_ENGINE_H
#define LED_ENGINE_H

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"
#include "core/ConfigManager.h"

class LEDEngine {
private:
    CRGB *allLeds;
    int mainCount;
    int heartCount;
    int totalCount;

    bool active;
    unsigned long startMs;

    ConfigData cfg;

    CRGB* getMainLeds() { return allLeds; }
    CRGB* getHeartLeds() { return allLeds + mainCount; }

public:
    LEDEngine();

    void begin(int mainLedCount, int heartLedCount);
    void loop();

    void startEffect(const ConfigData &config);
    void stop();

    bool isActive() const;

    void setBrightness(int b);
    void setColor(uint32_t rgb);
};

#endif
