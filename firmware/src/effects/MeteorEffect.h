#ifndef METEOR_EFFECT_H
#define METEOR_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class MeteorEffect {
public:
    void render(CRGB *mainLeds, int mainCount, const CRGB &color, unsigned long tMs, int speed) {
        if (!mainLeds || mainCount <= 0) return;
        
        // Limpar fita
        fill_solid(mainLeds, mainCount, CRGB::Black);
        
        // Calcular posição do meteoro (0.0 a 1.0)
        float speedFactor = speed / 10.0f;
        float meteorPos = fmod((tMs / 1000.0f) * speedFactor * 1.5f, 1.0f);
        int centerLed = (int)(meteorPos * mainCount);
        
        // Comprimento do rastro (20% da fita)
        int trailLength = max(1, mainCount / 5);
        
        // Desenhar meteoro com rastro
        for (int i = 0; i < trailLength; i++) {
            int ledIndex = (centerLed - i + mainCount) % mainCount;
            float intensity = 1.0f - ((float)i / trailLength);
            CRGB ledColor = color;
            ledColor.nscale8((uint8_t)(intensity * 255));
            mainLeds[ledIndex] = ledColor;
        }
    }
};

#endif
