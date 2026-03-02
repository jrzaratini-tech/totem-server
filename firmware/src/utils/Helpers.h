#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>

inline uint32_t parseHexColor(const String &s, uint32_t fallback) {
    if (s.length() == 0) return fallback;
    String v = s;
    if (v.startsWith("#")) v = v.substring(1);
    if (v.length() != 6) return fallback;
    char *end = nullptr;
    uint32_t c = strtoul(v.c_str(), &end, 16);
    if (end == v.c_str()) return fallback;
    return c;
}

inline String ipToString(const IPAddress &ip) {
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

#endif
