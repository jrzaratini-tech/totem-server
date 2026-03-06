#ifndef SPARKLE_EFFECT_H
#define SPARKLE_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class SparkleEffect {
public:
    void render(CRGB *mainLeds, int mainCount, const CRGB &color, unsigned long tMs, int speed) {
        if (!mainLeds || mainCount <= 0) return;
        
        float speedFactor = speed / 10.0f;
        float t = (tMs / 1000.0f) * speedFactor;
        
        // Cintilações aleatórias
        for (int i = 0; i < mainCount; i++) {
            // Usar seno com seed baseado no LED para criar padrão pseudo-aleatório
            float sparkleSeed = sin(i * 10.0f + t * 20.0f) * 0.5f + 0.5f;
            
            // Threshold para determinar se LED cintila (70% do tempo apagado)
            float threshold = 0.7f;
            
            if (sparkleSeed > threshold) {
                // LED cintila com brilho máximo
                mainLeds[i] = color;
            } else {
                // Brilho de fundo baixo (10%)
                CRGB dimColor = color;
                dimColor.nscale8(25); // 10% de brilho
                mainLeds[i] = dimColor;
            }
        }
    }
};

#endif
