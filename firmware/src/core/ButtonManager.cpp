#include "core/ButtonManager.h"
#include "Config.h"

ButtonManager::ButtonManager() {
    btns[0] = {PIN_BTN_TRIGGER, true, true, 0, 0, 0, false};
}

void ButtonManager::begin() {
    // TTP223 capacitive touch - lógica positiva (HIGH quando tocado)
    pinMode(PIN_BTN_TRIGGER, INPUT);
}

void ButtonManager::onButtonTrigger(std::function<void(bool)> cb) { onTrigger = cb; }

void ButtonManager::loop() {
    updateBtn(0);
}

void ButtonManager::updateBtn(int idx) {
    Btn &b = btns[idx];
    bool rawReading = digitalRead(b.pin) == HIGH;
    
    // TTP223: HIGH quando tocado, LOW quando não tocado
    bool reading = rawReading;
    
    unsigned long now = millis();

    if (reading != b.lastReading) {
        b.lastDebounceMs = now;
        b.lastReading = reading;
    }

    if (now - b.lastDebounceMs < DEBOUNCE_DELAY) {
        return;
    }

    if (reading != b.stableState) {
        b.stableState = reading;

        if (reading) {
            // pressed (normalizado para true = pressionado)
            b.pressStartMs = now;
            b.longFired = false;
        } else {
            // released
            if (!b.longFired) {
                unsigned long pressDur = now - b.pressStartMs;
                if (pressDur < LONG_PRESS_TIME) {
                    if (now - b.lastClickMs >= MIN_CLICK_INTERVAL) {
                        b.lastClickMs = now;
                        fire(idx, false);
                    }
                }
            }
        }
    }

    if (b.stableState && !b.longFired && (now - b.pressStartMs >= LONG_PRESS_TIME)) {
        b.longFired = true;
        if (now - b.lastClickMs >= MIN_CLICK_INTERVAL) {
            b.lastClickMs = now;
            fire(idx, true);
        }
    }
}

void ButtonManager::fire(int idx, bool longPress) {
    if (idx == 0 && onTrigger) {
        onTrigger(longPress);
    }
}
