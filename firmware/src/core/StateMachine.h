#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <Arduino.h>

enum SystemState {
    BOOT,
    IDLE,
    PLAYING,
    DOWNLOADING_AUDIO,
    UPDATING_CONFIG,
    UPDATING_FIRMWARE,
    ERROR
};

class StateMachine {
private:
    SystemState currentState;
    SystemState previousState;
    unsigned long stateStartTime;
    String lastError;

    String stateName(SystemState s) const;

public:
    StateMachine();

    bool setState(SystemState newState);
    SystemState getState() const;
    SystemState getPreviousState() const;

    String getStateName() const;
    unsigned long getStateDuration() const;

    bool isInState(SystemState s) const;

    void setError(const String &error);
    String getLastError() const;

    bool canPlay() const;
    bool canDownload() const;
    bool canOTA() const;
};

#endif
