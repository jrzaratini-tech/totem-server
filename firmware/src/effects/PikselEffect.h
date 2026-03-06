#ifndef PIKSEL_EFFECT_H
#define PIKSEL_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class PikselEffect {
public:
    void render(CRGB *mainLeds, int mainCount, unsigned long tMs, int speed) {
        if (!mainLeds || mainCount <= 0) return;
        
        float speedFactor = speed / 10.0f;
        float t = (tMs / 1000.0f) * speedFactor;
        
        // Efeito de fogo/chamas
        for (int i = 0; i < mainCount; i++) {
            // Simular chamas com ondas senoidais e variação por LED
            float flicker = (sin(t * 10.0f + i * 2.0f) * 0.5f + 0.5f);
            float flame = max(0.0f, sin(t * 8.0f - i * 3.0f) * flicker);
            
            uint8_t intensity = (uint8_t)(flame * 255);
            
            // Cor de fogo: laranja/vermelho (R=255, G=100, B=0)
            mainLeds[i] = CRGB(intensity, intensity / 3, 0);
        }
    }
};

#endif
