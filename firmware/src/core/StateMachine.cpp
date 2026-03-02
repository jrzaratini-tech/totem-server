#include "core/StateMachine.h"

StateMachine::StateMachine() {
    currentState = BOOT;
    previousState = BOOT;
    stateStartTime = millis();
}

String StateMachine::stateName(SystemState s) const {
    switch (s) {
        case BOOT: return "BOOT";
        case IDLE: return "IDLE";
        case PLAYING: return "PLAYING";
        case DOWNLOADING_AUDIO: return "DOWNLOADING_AUDIO";
        case UPDATING_CONFIG: return "UPDATING_CONFIG";
        case UPDATING_FIRMWARE: return "UPDATING_FIRMWARE";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

bool StateMachine::setState(SystemState newState) {
    // Regras principais
    if (newState == PLAYING) {
        if (currentState == PLAYING) return false;
        if (currentState != IDLE && currentState != BOOT) return false;
    }

    if (newState == DOWNLOADING_AUDIO) {
        if (currentState == PLAYING) return false;
        if (currentState == UPDATING_FIRMWARE) return false;
        if (currentState != IDLE && currentState != UPDATING_CONFIG) return false;
    }

    if (newState == UPDATING_FIRMWARE) {
        if (currentState != IDLE) return false;
    }

    previousState = currentState;
    currentState = newState;
    stateStartTime = millis();

    Serial.printf("[STATE] %s -> %s\n", stateName(previousState).c_str(), stateName(currentState).c_str());
    return true;
}

SystemState StateMachine::getState() const { return currentState; }
SystemState StateMachine::getPreviousState() const { return previousState; }

String StateMachine::getStateName() const { return stateName(currentState); }

unsigned long StateMachine::getStateDuration() const {
    return millis() - stateStartTime;
}

bool StateMachine::isInState(SystemState s) const { return currentState == s; }

void StateMachine::setError(const String &error) {
    lastError = error;
    setState(ERROR);
    Serial.printf("[ERROR] %s\n", lastError.c_str());
}

String StateMachine::getLastError() const { return lastError; }

bool StateMachine::canPlay() const {
    return currentState == IDLE;
}

bool StateMachine::canDownload() const {
    return currentState == IDLE || currentState == UPDATING_CONFIG;
}

bool StateMachine::canOTA() const {
    return currentState == IDLE;
}
