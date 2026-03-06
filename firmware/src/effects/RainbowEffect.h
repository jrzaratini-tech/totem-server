#ifndef RAINBOW_EFFECT_H
#define RAINBOW_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class RainbowEffect {
public:
    void render(CRGB *mainLeds, int mainCount, CRGB *heartLeds, int heartCount, uint8_t baseHue) {
        if (mainLeds && mainCount > 0) fill_rainbow(mainLeds, mainCount, baseHue, max(1, 255 / max(1, mainCount)));
    }
};

#endif
