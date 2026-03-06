#ifndef RUNNING_EFFECT_H
#define RUNNING_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class RunningEffect {
public:
    void render(CRGB *mainLeds, int mainCount, CRGB *heartLeds, int heartCount, const CRGB &color, int pos) {
        if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, CRGB::Black);

        if (mainCount <= 0) return;
        for (int i = 0; i < 4; i++) {
            int idx = (pos - i + mainCount) % mainCount;
            CRGB c = color;
            c.fadeToBlackBy(i * 50);
            if (mainLeds) mainLeds[idx] = c;
        }
    }
};

#endif
