#ifndef SOLID_EFFECT_H
#define SOLID_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class SolidEffect {
public:
    void render(CRGB *mainLeds, int mainCount, CRGB *heartLeds, int heartCount, const CRGB &color) {
        if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, color);
    }
};

#endif
