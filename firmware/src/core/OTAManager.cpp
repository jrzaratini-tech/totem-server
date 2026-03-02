#include "core/OTAManager.h"
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include "Config.h"

OTAManager::OTAManager() {
    updating = false;
    progress = 0;
}

void OTAManager::begin() {
}

void OTAManager::loop() {
}

bool OTAManager::startUpdateFromUrl(const String &url) {
    if (updating) return false;
    updating = true;
    progress = 0;

    if (!url.startsWith("https://")) {
        updating = false;
        return false;
    }
    if (String(ROOT_CA_PEM).length() == 0) {
        updating = false;
        return false;
    }

    WiFiClientSecure secure;
    secure.setCACert(ROOT_CA_PEM);

    HTTPClient http;
    if (!http.begin(secure, url)) {
        updating = false;
        return false;
    }

    int code = http.GET();
    if (code != 200) {
        http.end();
        updating = false;
        return false;
    }

    int len = http.getSize();
    if (len <= 0) {
        http.end();
        updating = false;
        return false;
    }

    if (!Update.begin(len)) {
        http.end();
        updating = false;
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buf[1024];

    unsigned long lastProg = millis();
    while (http.connected() && written < (size_t)len) {
        size_t avail = stream->available();
        if (avail) {
            int r = stream->readBytes(buf, (size_t)min(avail, sizeof(buf)));
            if (r > 0) {
                Update.write(buf, (size_t)r);
                written += (size_t)r;
            }
        }
        if (millis() - lastProg > 250) {
            progress = (int)((written * 100UL) / (unsigned long)len);
            lastProg = millis();
            yield();
        }
    }

    bool ok = false;
    if (Update.end()) {
        ok = Update.isFinished();
    }

    http.end();

    updating = false;
    progress = ok ? 100 : 0;

    if (ok) {
        ESP.restart();
    }

    return ok;
}

bool OTAManager::isUpdating() const { return updating; }
int OTAManager::getProgress() const { return progress; }
