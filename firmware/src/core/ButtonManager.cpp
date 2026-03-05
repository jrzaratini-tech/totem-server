#include "core/ButtonManager.h"
#include "Config.h"

ButtonManager::ButtonManager() {
    btns[0] = {PIN_BTN_COR, true, true, 0, 0, 0, false};
    btns[1] = {PIN_BTN_MAIS, true, true, 0, 0, 0, false};
    btns[2] = {PIN_BTN_MENOS, true, true, 0, 0, 0, false};
    btns[3] = {PIN_BTN_CORACAO, true, true, 0, 0, 0, false};
}

void ButtonManager::begin() {
    pinMode(PIN_BTN_COR, INPUT_PULLUP);
    pinMode(PIN_BTN_MAIS, INPUT_PULLUP);
    pinMode(PIN_BTN_MENOS, INPUT_PULLUP);
    // TTP223 capacitive touch - lógica positiva (HIGH quando tocado)
    pinMode(PIN_BTN_CORACAO, INPUT);
}

void ButtonManager::onButtonCor(std::function<void(bool)> cb) { onCor = cb; }
void ButtonManager::onButtonMais(std::function<void(bool)> cb) { onMais = cb; }
void ButtonManager::onButtonMenos(std::function<void(bool)> cb) { onMenos = cb; }
void ButtonManager::onButtonCoracao(std::function<void(bool)> cb) { onCoracao = cb; }

void ButtonManager::loop() {
    for (int i = 0; i < 4; i++) updateBtn(i);
}

void ButtonManager::updateBtn(int idx) {
    Btn &b = btns[idx];
    bool rawReading = digitalRead(b.pin) == HIGH;
    
    // TTP223 no GPIO15 (índice 3 = botão coração/trigger) tem lógica invertida
    // TTP223: HIGH quando tocado, LOW quando não tocado
    // Botões mecânicos: LOW quando pressionado, HIGH quando não pressionado
    bool reading = (idx == 3) ? rawReading : !rawReading;
    
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
    switch (idx) {
        case 0: if (onCor) onCor(longPress); break;
        case 1: if (onMais) onMais(longPress); break;
        case 2: if (onMenos) onMenos(longPress); break;
        case 3: if (onCoracao) onCoracao(longPress); break;
        default: break;
    }
}
