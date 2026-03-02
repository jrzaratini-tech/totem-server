#ifndef BREATH_EFFECT_H
#define BREATH_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class BreathEffect {
public:
    uint8_t brightnessFor(unsigned long tMs, int speed) {
        float w = (float)max(1, speed) / 50.0f;
        float s = sinf((tMs / 1000.0f) * 2.0f * PI * w);
        float f = (s + 1.0f) * 0.5f;
        return (uint8_t)(255.0f * f);
    }
};

#endif
