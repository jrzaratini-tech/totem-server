#ifndef BLINK_EFFECT_H
#define BLINK_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class BlinkEffect {
public:
    void render(CRGB *mainLeds, int mainCount, CRGB *heartLeds, int heartCount, const CRGB &color, bool on) {
        if (on) {
            if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, color);
        } else {
            if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, CRGB::Black);
        }
    }
};

#endif
