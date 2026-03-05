#include "core/LEDEngine.h"
#include "effects/SolidEffect.h"
#include "effects/RainbowEffect.h"
#include "effects/BlinkEffect.h"
#include "effects/BreathEffect.h"
#include "effects/RunningEffect.h"
#include "effects/HeartEffect.h"

static SolidEffect solidEffect;
static RainbowEffect rainbowEffect;
static BlinkEffect blinkEffect;
static BreathEffect breathEffect;
static RunningEffect runningEffect;
static HeartEffect heartEffect;

LEDEngine::LEDEngine() {
    mainLeds = nullptr;
    heartLeds = nullptr;
    mainCount = 0;
    heartCount = 0;
    active = false;
    startMs = 0;

    cfg.mode = BREATH;
    cfg.color = 0xFF3366;
    cfg.speed = 50;
    cfg.duration = EFEITO_TEMPO_PADRAO;
    cfg.maxBrightness = DEFAULT_BRIGHTNESS;
}

void LEDEngine::begin(int mainLedCount, int heartLedCount) {
    mainCount = mainLedCount;
    heartCount = heartLedCount;

    mainLeds = (mainCount > 0) ? new CRGB[mainCount] : nullptr;
    heartLeds = (heartCount > 0) ? new CRGB[heartCount] : nullptr;

    if (mainLeds) FastLED.addLeds<LED_TYPE, LED_MAIN_PIN, COLOR_ORDER>(mainLeds, mainCount);
    if (heartLeds) FastLED.addLeds<LED_TYPE, LED_HEART_PIN, COLOR_ORDER>(heartLeds, heartCount);

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
    if (!active) return;

    unsigned long now = millis();
    unsigned long elapsed = now - startMs;

    CRGB baseColor((uint8_t)((cfg.color >> 16) & 0xFF), (uint8_t)((cfg.color >> 8) & 0xFF), (uint8_t)(cfg.color & 0xFF));

    switch (cfg.mode) {
        case SOLID:
            solidEffect.render(mainLeds, mainCount, heartLeds, heartCount, baseColor);
            break;
        case RAINBOW: {
            uint8_t hue = (uint8_t)((elapsed / (uint32_t)max(1, cfg.speed)) & 0xFF);
            rainbowEffect.render(mainLeds, mainCount, heartLeds, heartCount, hue);
            break;
        }
        case BLINK: {
            uint32_t period = (uint32_t)max(50, cfg.speed * 10);
            bool on = (elapsed % period) < (period / 2);
            blinkEffect.render(mainLeds, mainCount, heartLeds, heartCount, baseColor, on);
            break;
        }
        case BREATH: {
            uint8_t level = breathEffect.brightnessFor(elapsed, cfg.speed);
            uint8_t scaled = (uint8_t)map(level, 0, 255, 0, cfg.maxBrightness);
            FastLED.setBrightness(scaled);
            solidEffect.render(mainLeds, mainCount, heartLeds, heartCount, baseColor);
            break;
        }
        case RUNNING: {
            int pos = (int)((elapsed / (uint32_t)max(1, cfg.speed)) % (uint32_t)max(1, mainCount));
            runningEffect.render(mainLeds, mainCount, heartLeds, heartCount, baseColor, pos);
            break;
        }
        case HEART: {
            // Apagar fita principal
            if (mainLeds && mainCount > 0) fill_solid(mainLeds, mainCount, CRGB::Black);
            
            // Aplicar batimento vermelho apenas na fita do coração
            if (heartLeds && heartCount > 0) {
                uint8_t level = heartEffect.beatLevel(elapsed);
                CRGB heartColor = CRGB::Red;
                heartColor.nscale8(level);  // Aplicar nível de batimento à cor vermelha
                fill_solid(heartLeds, heartCount, heartColor);
            }
            break;
        }
        default:
            solidEffect.render(mainLeds, mainCount, heartLeds, heartCount, baseColor);
            break;
    }

    FastLED.show();
}
