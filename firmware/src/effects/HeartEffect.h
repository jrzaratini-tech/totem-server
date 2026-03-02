#ifndef HEART_EFFECT_H
#define HEART_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class HeartEffect {
public:
    uint8_t beatLevel(unsigned long tMs) {
        unsigned long p = tMs % 1000;
        if (p < 120) return (uint8_t)map(p, 0, 120, 30, 255);
        if (p < 220) return (uint8_t)map(p, 120, 220, 255, 40);
        if (p < 420) return 40;
        if (p < 520) return (uint8_t)map(p, 420, 520, 40, 220);
        if (p < 650) return (uint8_t)map(p, 520, 650, 220, 30);
        return 30;
    }
};

#endif
