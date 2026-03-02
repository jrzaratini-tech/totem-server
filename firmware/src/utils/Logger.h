#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

class Logger {
public:
    static void info(const String &msg) { Serial.println(msg); }
    static void warn(const String &msg) { Serial.println(msg); }
    static void error(const String &msg) { Serial.println(msg); }
};

#endif
