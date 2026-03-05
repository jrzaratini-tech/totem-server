#ifndef HEART_EFFECT_H
#define HEART_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

class HeartEffect {
public:
    uint8_t beatLevel(unsigned long tMs) {
        unsigned long p = tMs % 1200;  // Ciclo de 1.2 segundos
        
        // Primeiro batimento: PUM (0-150ms)
        if (p < 80) return (uint8_t)map(p, 0, 80, 0, 255);        // Subida rápida
        if (p < 150) return (uint8_t)map(p, 80, 150, 255, 20);    // Descida rápida
        
        // Pausa curta entre batimentos (150-200ms)
        if (p < 200) return 10;
        
        // Segundo batimento: PUM (200-350ms)
        if (p < 280) return (uint8_t)map(p, 200, 280, 0, 255);    // Subida rápida
        if (p < 350) return (uint8_t)map(p, 280, 350, 255, 20);   // Descida rápida
        
        // Pausa longa (350-1200ms) - coração em repouso
        return 5;  // Brilho mínimo durante a pausa
    }
};

#endif
