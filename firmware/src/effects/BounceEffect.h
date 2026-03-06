#ifndef BOUNCE_EFFECT_H
#define BOUNCE_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class BounceEffect {
public:
    void render(CRGB *mainLeds, int mainCount, const CRGB &color, unsigned long tMs, int speed) {
        if (!mainLeds || mainCount <= 0) return;
        
        // Limpar fita
        fill_solid(mainLeds, mainCount, CRGB::Black);
        
        // Calcular posição do bounce (vai e volta)
        float speedFactor = speed / 10.0f;
        float t = (tMs / 1000.0f) * speedFactor * 1.5f;
        float bouncePos = abs(fmod(t, 2.0f) - 1.0f); // Onda triangular 0->1->0
        
        int centerLed = (int)(bouncePos * (mainCount - 1));
        
        // Tamanho do ponto que quica (10% da fita)
        int size = max(1, mainCount / 10);
        
        // Desenhar ponto com fade
        for (int i = 0; i < size; i++) {
            int offset = i - size / 2;
            int ledIndex = centerLed + offset;
            if (ledIndex >= 0 && ledIndex < mainCount) {
                float distance = abs((float)offset) / (size / 2.0f);
                float intensity = max(0.0f, 1.0f - distance);
                CRGB ledColor = color;
                ledColor.nscale8((uint8_t)(intensity * 255));
                mainLeds[ledIndex] = ledColor;
            }
        }
    }
};

#endif
