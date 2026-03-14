#include "core/LEDEngine.h"
#include "effects/SolidEffect.h"
#include "effects/RainbowEffect.h"
#include "effects/BlinkEffect.h"
#include "effects/BreathEffect.h"
#include "effects/RunningEffect.h"
#include "effects/HeartEffect.h"
#include "effects/MeteorEffect.h"
#include "effects/PikselEffect.h"
#include "effects/BounceEffect.h"
#include "effects/SparkleEffect.h"

static SolidEffect solidEffect;
static RainbowEffect rainbowEffect;
static BlinkEffect blinkEffect;
static BreathEffect breathEffect;
static RunningEffect runningEffect;
static HeartEffect heartEffect;
static MeteorEffect meteorEffect;
static PikselEffect pikselEffect;
static BounceEffect bounceEffect;
static SparkleEffect sparkleEffect;

LEDEngine::LEDEngine() {
    allLeds = nullptr;
    mainCount = 0;
    heartCount = 0;
    totalCount = 0;
    active = false;
    startMs = 0;

    cfg.mode = BREATH;
    cfg.color = 0xFF3366;
    cfg.speed = 50;
    cfg.duration = EFEITO_TEMPO_PADRAO;
    cfg.maxBrightness = DEFAULT_BRIGHTNESS;

    heartbeatEffectActive = false;
    heartbeatStartMs = 0;
    heartbeatDurationMs = 5000;
}

void LEDEngine::begin(int mainLedCount, int heartLedCount) {
    mainCount = mainLedCount;
    heartCount = heartLedCount;
    totalCount = mainCount + heartCount;

    Serial.printf("[LEDEngine] Initializing with %d main LEDs + %d heart LEDs = %d total\n", 
                  mainCount, heartCount, totalCount);

    if (totalCount > 0) {
        allLeds = new CRGB[totalCount];
        
        // CRITICAL FIX: Use only ONE FastLED controller for both strips
        // Main strip (198 LEDs) on GPIO 1
        // Heart strip (9 LEDs) follows immediately after in the array
        // This avoids RMT channel conflicts on ESP32-S3
        FastLED.addLeds<LED_TYPE, LED_MAIN_PIN, COLOR_ORDER>(allLeds, totalCount);
        
        Serial.printf("[LEDEngine] ✓ Single FastLED controller initialized on GPIO %d\n", LED_MAIN_PIN);
        Serial.println("[LEDEngine] Main LEDs: indices 0-197");
        Serial.println("[LEDEngine] Heart LEDs: indices 198-206");
        Serial.println("[LEDEngine] NOTE: Heart LEDs are PHYSICALLY on GPIO 9, wire accordingly");
    }

    FastLED.clear(true);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
}

void LEDEngine::startEffect(const ConfigData &config) {
    cfg = config;
    cfg.speed = max(1, cfg.speed);
    cfg.duration = max(1, cfg.duration);
    cfg.maxBrightness = constrain(cfg.maxBrightness, 0, MAX_BRIGHTNESS);

    FastLED.setBrightness(cfg.maxBrightness);

    active = true;
    startMs = millis();
}

void LEDEngine::stop() {
    active = false;
    FastLED.clear(true);
}

bool LEDEngine::isActive() const { return active; }

void LEDEngine::setBrightness(int b) {
    cfg.maxBrightness = constrain(b, 0, MAX_BRIGHTNESS);
    FastLED.setBrightness(cfg.maxBrightness);
}

void LEDEngine::setColor(uint32_t rgb) {
    cfg.color = rgb;
}

void LEDEngine::loop() {
    unsigned long now = millis();

    // Verificar se o efeito de batimento cardíaco está ativo
    if (heartbeatEffectActive) {
        unsigned long heartbeatElapsed = now - heartbeatStartMs;
        
        if (heartbeatElapsed >= heartbeatDurationMs) {
            // Efeito terminou, desativar
            heartbeatEffectActive = false;
            Serial.println("[LEDEngine] Heartbeat effect finished");
        } else {
            // Renderizar efeito de batimento cardíaco na fita principal
            CRGB* mainLeds = getMainLeds();
            if (mainLeds && mainCount > 0) {
                uint8_t level = heartEffect.beatLevel(heartbeatElapsed);
                CRGB heartColor = CRGB::Red;
                heartColor.nscale8(level);
                fill_solid(mainLeds, mainCount, heartColor);
            }
            
            // Manter o efeito cardíaco nos LEDs do coração
            CRGB* heartLeds = getHeartLeds();
            if (heartLeds && heartCount > 0) {
                uint8_t level = heartEffect.beatLevel(heartbeatElapsed);
                if (level == 0) {
                    fill_solid(heartLeds, heartCount, CRGB::Black);
                } else {
                    CRGB heartColor = CRGB::Red;
                    heartColor.nscale8(level);
                    fill_solid(heartLeds, heartCount, heartColor);
                }
            }
            
            FastLED.show();
            return;
        }
    }

    if (!active) return;

    unsigned long elapsed = now - startMs;

    CRGB baseColor((uint8_t)((cfg.color >> 16) & 0xFF), (uint8_t)((cfg.color >> 8) & 0xFF), (uint8_t)(cfg.color & 0xFF));

    CRGB* mainLeds = getMainLeds();
    CRGB* heartLeds = getHeartLeds();

    // EFEITO PRINCIPAL: Aplicar nos primeiros 198 LEDs
    switch (cfg.mode) {
        case SOLID:
            if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, baseColor);
            break;
        case RAINBOW: {
            uint8_t hue = (uint8_t)((elapsed / (uint32_t)max(1, cfg.speed)) & 0xFF);
            rainbowEffect.render(mainLeds, mainCount, nullptr, 0, hue);
            break;
        }
        case BLINK: {
            uint32_t period = (uint32_t)max(50, cfg.speed * 10);
            bool on = (elapsed % period) < (period / 2);
            blinkEffect.render(mainLeds, mainCount, nullptr, 0, baseColor, on);
            break;
        }
        case BREATH: {
            uint8_t level = breathEffect.brightnessFor(elapsed, cfg.speed);
            uint8_t scaled = (uint8_t)map(level, 0, 255, 0, cfg.maxBrightness);
            FastLED.setBrightness(scaled);
            if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, baseColor);
            break;
        }
        case RUNNING: {
            int pos = (int)((elapsed / (uint32_t)max(1, cfg.speed)) % (uint32_t)max(1, mainCount));
            runningEffect.render(mainLeds, mainCount, nullptr, 0, baseColor, pos);
            break;
        }
        case HEART:
            // Modo HEART: apagar fita principal
            if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, CRGB::Black);
            break;
        case METEOR:
            meteorEffect.render(mainLeds, mainCount, baseColor, now, cfg.speed);
            break;
        case PIKSEL:
            pikselEffect.render(mainLeds, mainCount, now, cfg.speed);
            break;
        case BOUNCE:
            bounceEffect.render(mainLeds, mainCount, baseColor, now, cfg.speed);
            break;
        case SPARKLE:
            sparkleEffect.render(mainLeds, mainCount, baseColor, now, cfg.speed);
            break;
        default:
            if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, baseColor);
            break;
    }

    // EFEITO CARDÍACO: SEMPRE aplicar nos últimos 9 LEDs (contínuo)
    if (heartLeds && heartCount > 0) {
        uint8_t level = heartEffect.beatLevel(elapsed);
        if (level == 0) {
            fill_solid(heartLeds, heartCount, CRGB::Black);
        } else {
            CRGB heartColor = CRGB::Red;
            heartColor.nscale8(level);
            fill_solid(heartLeds, heartCount, heartColor);
        }
    }

    FastLED.show();
}

void LEDEngine::triggerHeartbeatEffect(unsigned long durationMs) {
    heartbeatEffectActive = true;
    heartbeatStartMs = millis();
    heartbeatDurationMs = durationMs;
    Serial.printf("[LEDEngine] Heartbeat effect triggered for %lu ms\n", durationMs);
}

bool LEDEngine::isHeartbeatEffectActive() const {
    return heartbeatEffectActive;
}
