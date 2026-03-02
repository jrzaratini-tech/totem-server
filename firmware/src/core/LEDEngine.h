#ifndef LED_ENGINE_H
#define LED_ENGINE_H

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"
#include "core/ConfigManager.h"

class LEDEngine {
private:
    CRGB *mainLeds;
    CRGB *heartLeds;
    int mainCount;
    int heartCount;

    bool active;
    unsigned long startMs;

    ConfigData cfg;

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
