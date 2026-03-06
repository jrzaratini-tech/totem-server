#ifndef HEARTBEAT_LED_H
#define HEARTBEAT_LED_H

#include <Arduino.h>
#include <FastLED.h>

class HeartbeatLED {
private:
    CRGB *leds;
    int numLeds;
    unsigned long lastBeat;
    int beatPhase;
    
public:
    HeartbeatLED() : leds(nullptr), numLeds(0), lastBeat(0), beatPhase(0) {}
    
    void begin(int ledCount, int pin) {
        numLeds = ledCount;
        leds = new CRGB[numLeds];
        FastLED.addLeds<WS2812B, 18, GRB>(leds, numLeds).setCorrection(TypicalLEDStrip);
        FastLED.setBrightness(255);
        lastBeat = millis();
    }
    
    void update() {
        unsigned long now = millis();
        unsigned long elapsed = now - lastBeat;
        
        // Batimento cardíaco: 60 BPM = 1000ms por batida
        // Padrão: lub-dub (duas pulsações rápidas)
        const unsigned long BEAT_CYCLE = 1000;
        const unsigned long FIRST_PULSE = 100;
        const unsigned long PAUSE_SHORT = 150;
        const unsigned long SECOND_PULSE = 80;
        const unsigned long PAUSE_LONG = 670;
        
        uint8_t brightness = 0;
        
        if (elapsed < FIRST_PULSE) {
            // Primeira pulsação (lub)
            float progress = (float)elapsed / FIRST_PULSE;
            if (progress < 0.5) {
                brightness = (uint8_t)(progress * 2.0 * 255);
            } else {
                brightness = (uint8_t)((1.0 - (progress - 0.5) * 2.0) * 255);
            }
        } else if (elapsed < FIRST_PULSE + PAUSE_SHORT) {
            // Pausa curta
            brightness = 0;
        } else if (elapsed < FIRST_PULSE + PAUSE_SHORT + SECOND_PULSE) {
            // Segunda pulsação (dub)
            unsigned long pulseTime = elapsed - (FIRST_PULSE + PAUSE_SHORT);
            float progress = (float)pulseTime / SECOND_PULSE;
            if (progress < 0.5) {
                brightness = (uint8_t)(progress * 2.0 * 200);
            } else {
                brightness = (uint8_t)((1.0 - (progress - 0.5) * 2.0) * 200);
            }
        } else {
            // Pausa longa
            brightness = 0;
        }
        
        // Reset do ciclo
        if (elapsed >= BEAT_CYCLE) {
            lastBeat = now;
        }
        
        // Vermelho intenso
        CRGB color = CRGB(brightness, 0, 0);
        fill_solid(leds, numLeds, color);
        FastLED.show();
    }
};

#endif
