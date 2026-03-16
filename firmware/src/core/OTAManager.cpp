#include "core/OTAManager.h"
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
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

    WiFiClientSecure secure;
    if (String(ROOT_CA_PEM).length() > 0) {
        secure.setCACert(ROOT_CA_PEM);
    } else {
        #if defined(ALLOW_INSECURE_HTTPS) && (ALLOW_INSECURE_HTTPS == 1)
        secure.setInsecure();
        #else
        updating = false;
        return false;
        #endif
    }

    HTTPClient http;
    if (!http.begin(secure, url)) {
        updating = false;
        return false;
    }
    
    // Configurar timeout HTTP para OTA
    http.setTimeout(60000); // 60 segundos

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
    uint8_t buf[4096];  // Buffer maior para OTA mais rápido (4KB)

    unsigned long lastProg = millis();
    unsigned long otaStartMs = millis();
    
    Serial.println("[OTA] Starting firmware download...");
    Serial.printf("[OTA] Firmware size: %.2f MB\n", len / 1024.0 / 1024.0);
    
    while (http.connected() && written < (size_t)len) {
        // Timeout de 5 minutos para OTA completo
        if (millis() - otaStartMs > OTA_TIMEOUT) {
            Serial.println("[OTA] ERROR: OTA timeout");
            Update.abort();
            http.end();
            updating = false;
            return false;
        }
        
        size_t avail = stream->available();
        if (avail) {
            int r = stream->readBytes(buf, (size_t)min(avail, sizeof(buf)));
            if (r > 0) {
                Update.write(buf, (size_t)r);
                written += (size_t)r;
            }
        } else {
            delay(1);  // Pequeno delay se não há dados disponíveis
        }
        
        if (millis() - lastProg > 2000) {  // Atualiza progresso a cada 2s
            progress = (int)((written * 100UL) / (unsigned long)len);
            Serial.printf("[OTA] Progress: %d%% (%.2f/%.2f MB)\n", 
                         progress, 
                         written / 1024.0 / 1024.0, 
                         len / 1024.0 / 1024.0);
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
        unsigned long downloadTime = (millis() - otaStartMs) / 1000;
        float speedKBps = (written / 1024.0) / downloadTime;
        Serial.println("[OTA] ========================================");
        Serial.println("[OTA] SUCCESS! Firmware updated");
        Serial.printf("[OTA] Downloaded: %.2f MB in %lu seconds\n", written / 1024.0 / 1024.0, downloadTime);
        Serial.printf("[OTA] Average speed: %.2f KB/s\n", speedKBps);
        Serial.println("[OTA] Restarting in 2 seconds...");
        Serial.println("[OTA] ========================================");
        delay(2000);
        ESP.restart();
    } else {
        Serial.println("[OTA] ========================================");
        Serial.printf("[OTA] FAILED! Error: %s\n", Update.errorString());
        Serial.printf("[OTA] Downloaded: %d/%d bytes before failure\n", written, len);
        Serial.println("[OTA] ========================================");
    }

    return ok;
}

bool OTAManager::isUpdating() const { return updating; }
int OTAManager::getProgress() const { return progress; }
