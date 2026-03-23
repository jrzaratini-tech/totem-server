#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include <functional>

class ButtonManager {
private:
    struct Btn {
        uint8_t pin;
        bool lastReading;
        bool stableState;
        unsigned long lastDebounceMs;
        unsigned long pressStartMs;
        unsigned long lastClickMs;
        bool longFired;
    };

    Btn btns[1];

    std::function<void(bool)> onTrigger;

    void updateBtn(int idx);
    void fire(int idx, bool longPress);

public:
    ButtonManager();

    void begin();
    void loop();

    void onButtonTrigger(std::function<void(bool)> cb);
};

#endif
