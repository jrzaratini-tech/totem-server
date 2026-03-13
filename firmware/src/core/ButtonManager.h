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

    Btn btns[5];

    std::function<void(bool)> onCor;
    std::function<void(bool)> onMais;
    std::function<void(bool)> onMenos;
    std::function<void(bool)> onCoracao;
    std::function<void(bool)> onHeartbeat;

    void updateBtn(int idx);
    void fire(int idx, bool longPress);

public:
    ButtonManager();

    void begin();
    void loop();

    void onButtonCor(std::function<void(bool)> cb);
    void onButtonMais(std::function<void(bool)> cb);
    void onButtonMenos(std::function<void(bool)> cb);
    void onButtonCoracao(std::function<void(bool)> cb);
    void onButtonHeartbeat(std::function<void(bool)> cb);
};

#endif
