#include "core/AudioManager.h"
#include "Config.h"
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include "utils/ES8388.h"

AudioManager::AudioManager() {
    playing = false;
    downloading = false;
    currentVersion = 0;
    downloadStartMs = 0;
}

void AudioManager::begin(const String &totemId, const String &deviceToken) {
    this->totemId = totemId;
    this->deviceToken = deviceToken;
    
    Serial.println("[Audio] Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("[Audio] ERROR: SPIFFS initialization failed!");
    } else {
        Serial.println("[Audio] ✓ SPIFFS initialized");
        size_t totalBytes = SPIFFS.totalBytes();
        size_t usedBytes = SPIFFS.usedBytes();
        Serial.printf("[Audio] SPIFFS - Total: %d bytes, Used: %d bytes, Free: %d bytes\n", 
                     totalBytes, usedBytes, totalBytes - usedBytes);
    }

    Serial.println("[Audio] Enabling Power Amplifier...");
    pinMode(PA_ENABLE_PIN, OUTPUT);
    digitalWrite(PA_ENABLE_PIN, HIGH);
    Serial.printf("[Audio] PA_EN (GPIO %d) set to HIGH\n", PA_ENABLE_PIN);
    
    Serial.println("[Audio] Initializing ES8388 codec...");
    Serial.printf("[Audio] I2C Pins - SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);
    ES8388::begin(I2C_SDA, I2C_SCL);
    ES8388::setVolume(25); // Volume 0-33 (25 = alto)
    
    Serial.println("[Audio] Initializing I2S audio...");
    Serial.printf("[Audio] I2S Pins - BCLK=%d, LRC=%d, DOUT=%d\n", I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(DEFAULT_VOLUME);
    Serial.printf("[Audio] ✓ I2S initialized - Volume set to %d\n", DEFAULT_VOLUME);
}

void AudioManager::loop() {
    if (playing) {
        audio.loop();
        if (!audio.isRunning()) {
            playing = false;
        }
    }
}

void AudioManager::play() {
    Serial.println("[Audio] play() called");
    
    if (playing) {
        Serial.println("[Audio] Already playing, ignoring");
        return;
    }
    
    if (!SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] ERROR: Audio file not found in SPIFFS!");
        Serial.printf("[Audio] Looking for: %s\n", AUDIO_FILENAME);
        return;
    }

    Serial.printf("[Audio] Attempting to play: %s\n", AUDIO_FILENAME);
    if (audio.connecttoFS(SPIFFS, AUDIO_FILENAME)) {
        playing = true;
        Serial.println("[Audio] Playback started successfully!");
    } else {
        Serial.println("[Audio] ERROR: Failed to start playback!");
    }
}

void AudioManager::stop() {
    if (!playing) return;
    Serial.println("[Audio] Stopping playback");
    audio.stopSong();
    playing = false;
}

void AudioManager::setVolume(int vol) {
    // vol: 0-21 (ES8388 range)
    int clampedVol = constrain(vol, 0, 21);
    ES8388::setVolume(clampedVol);
    audio.setVolume(clampedVol);
    Serial.printf("[Audio] Volume set to %d\n", clampedVol);
}

bool AudioManager::isPlaying() const { return playing; }
bool AudioManager::isDownloading() const { return downloading; }

int AudioManager::getVersion() const { return currentVersion; }
void AudioManager::setVersion(int v) { currentVersion = max(0, v); }

bool AudioManager::downloadFileToTemp(const String &url) {
    Serial.printf("[Audio] downloadFileToTemp() - URL: %s\n", url.c_str());

    // Limpar arquivo temporário anterior se existir
    if (SPIFFS.exists(AUDIO_TEMP_FILENAME)) {
        Serial.println("[Audio] Removing old temp file...");
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
    }

    // CRÍTICO: Deletar áudio antigo ANTES de baixar o novo para liberar espaço
    if (SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] Removing old audio file to free space...");
        SPIFFS.remove(AUDIO_FILENAME);
        size_t freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
        Serial.printf("[Audio] SPIFFS free space after cleanup: %d bytes\n", freeBytes);
    }

    if (!url.startsWith("https://")) {
        Serial.println("[Audio] ERROR: URL must be HTTPS");
        return false;
    }

    WiFiClientSecure secure;
    if (String(ROOT_CA_PEM).length() > 0) {
        Serial.println("[Audio] Using ROOT_CA_PEM for HTTPS");
        secure.setCACert(ROOT_CA_PEM);
    } else {
        #if defined(ALLOW_INSECURE_HTTPS) && (ALLOW_INSECURE_HTTPS == 1)
        Serial.println("[Audio] Using insecure HTTPS (no certificate validation)");
        secure.setInsecure();
        #else
        Serial.println("[Audio] ERROR: ROOT_CA_PEM required but not configured");
        return false;
        #endif
    }

    HTTPClient http;
    if (!http.begin(secure, url)) {
        Serial.println("[Audio] ERROR: HTTP begin failed");
        return false;
    }

    if (deviceToken.length() > 0) {
        http.addHeader("X-Device-Token", deviceToken);
        Serial.println("[Audio] Added device token to request");
    }

    Serial.println("[Audio] Sending GET request...");
    int code = http.GET();
    Serial.printf("[Audio] Download response code: %d\n", code);
    
    if (code != 200) {
        http.end();
        Serial.printf("[Audio] ERROR: Expected 200, got %d\n", code);
        return false;
    }

    int len = http.getSize();
    Serial.printf("[Audio] File size: %d bytes (%.2f KB)\n", len, len / 1024.0);
    
    if (len <= 0 || len > (int)MAX_AUDIO_SIZE) {
        http.end();
        Serial.printf("[Audio] ERROR: Invalid file size (max: %d bytes)\n", MAX_AUDIO_SIZE);
        return false;
    }

    Serial.printf("[Audio] Opening temp file: %s\n", AUDIO_TEMP_FILENAME);
    File tmp = SPIFFS.open(AUDIO_TEMP_FILENAME, FILE_WRITE);
    if (!tmp) {
        http.end();
        Serial.println("[Audio] ERROR: Failed to open temp file for writing");
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    
    // Alocar buffer no heap para evitar stack overflow
    uint8_t *buf = (uint8_t*)malloc(2048);
    if (!buf) {
        tmp.close();
        http.end();
        Serial.println("[Audio] ERROR: Failed to allocate buffer");
        return false;
    }
    
    int total = 0;
    int lastPercent = -1;

    Serial.println("[Audio] Starting download...");
    unsigned long lastWdtReset = millis();
    unsigned long lastProgressLog = millis();
    
    while (http.connected() && total < len) {
        // Reset watchdog a cada 500ms durante download
        if (millis() - lastWdtReset > 500) {
            esp_task_wdt_reset();
            lastWdtReset = millis();
        }
        
        if (millis() - downloadStartMs > DOWNLOAD_TIMEOUT) {
            tmp.close();
            http.end();
            free(buf);
            Serial.println("[Audio] ERROR: Download timeout");
            return false;
        }

        size_t avail = stream->available();
        if (avail) {
            size_t toRead = min(avail, (size_t)2048);
            int r = stream->readBytes(buf, toRead);
            if (r > 0) {
                tmp.write(buf, (size_t)r);
                total += r;
                
                int percent = (total * 100) / len;
                if (percent != lastPercent && percent % 10 == 0) {
                    unsigned long elapsed = (millis() - downloadStartMs) / 1000;
                    float speed = (total / 1024.0) / max(1UL, elapsed);
                    Serial.printf("[Audio] Download progress: %d%% (%d/%d bytes, %.1f KB/s)\n", 
                                 percent, total, len, speed);
                    lastPercent = percent;
                }
                
                // Log a cada 5 segundos mesmo sem mudança de %
                if (millis() - lastProgressLog > 5000) {
                    unsigned long elapsed = (millis() - downloadStartMs) / 1000;
                    float speed = (total / 1024.0) / max(1UL, elapsed);
                    Serial.printf("[Audio] Downloaded: %d bytes (%.1f KB/s)\n", total, speed);
                    lastProgressLog = millis();
                }
            }
        } else {
            delay(10);  // Pequeno delay se não há dados disponíveis
        }
        yield();
    }

    tmp.close();
    http.end();
    free(buf);

    Serial.printf("[Audio] Download finished - Total: %d bytes, Expected: %d bytes\n", total, len);

    if (total != len) {
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
        Serial.println("[Audio] ERROR: Incomplete download, removed temp file");
        return false;
    }

    Serial.println("[Audio] Download successful!");
    return true;
}

bool AudioManager::activateTempAsCurrent() {
    Serial.println("[Audio] activateTempAsCurrent() called");
    
    if (!SPIFFS.exists(AUDIO_TEMP_FILENAME)) {
        Serial.println("[Audio] ERROR: Temp file does not exist");
        return false;
    }

    File tmp = SPIFFS.open(AUDIO_TEMP_FILENAME, FILE_READ);
    if (tmp) {
        Serial.printf("[Audio] Temp file size: %d bytes\n", tmp.size());
        tmp.close();
    }

    if (SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] Removing old audio file");
        SPIFFS.remove(AUDIO_FILENAME);
    }

    Serial.printf("[Audio] Renaming %s to %s\n", AUDIO_TEMP_FILENAME, AUDIO_FILENAME);
    bool ok = SPIFFS.rename(AUDIO_TEMP_FILENAME, AUDIO_FILENAME);
    
    if (!ok) {
        Serial.println("[Audio] ERROR: Failed to rename temp file");
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
        return false;
    }

    Serial.println("[Audio] Audio file activated successfully!");
    return true;
}

bool AudioManager::checkAndDownloadFromServer(String *outError) {
    Serial.println("[Audio] checkAndDownloadFromServer() called");
    
    if (downloading) {
        Serial.println("[Audio] Download already in progress");
        if (outError) *outError = "download_in_progress";
        return false;
    }

    downloading = true;
    downloadStartMs = millis();
    Serial.println("[Audio] Starting download process...");

    String apiUrl = String(SERVER_URL) + "/api/audio/" + totemId;

    if (!apiUrl.startsWith("https://")) {
        downloading = false;
        if (outError) *outError = "server_url_not_https";
        return false;
    }
    WiFiClientSecure secure;
    if (String(ROOT_CA_PEM).length() > 0) {
        secure.setCACert(ROOT_CA_PEM);
    } else {
        #if defined(ALLOW_INSECURE_HTTPS) && (ALLOW_INSECURE_HTTPS == 1)
        secure.setInsecure();
        #else
        downloading = false;
        if (outError) *outError = "root_ca_missing";
        return false;
        #endif
    }

    HTTPClient http;
    if (!http.begin(secure, apiUrl)) {
        downloading = false;
        if (outError) *outError = "http_begin_failed";
        return false;
    }

    if (deviceToken.length() > 0) {
        http.addHeader("X-Device-Token", deviceToken);
    }

    int code = http.GET();
    Serial.printf("[Audio] API response code: %d\n", code);
    if (code != 200) {
        http.end();
        downloading = false;
        if (outError) *outError = String("api_http_") + code;
        Serial.printf("[Audio] ERROR: API returned code %d\n", code);
        return false;
    }

    String body = http.getString();
    http.end();
    Serial.printf("[Audio] API response body: %s\n", body.c_str());

    StaticJsonDocument<768> doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        downloading = false;
        if (outError) *outError = "json_invalid";
        Serial.printf("[Audio] ERROR: Invalid JSON - %s\n", err.c_str());
        return false;
    }

    String url = "";
    if (doc.containsKey("url") && !doc["url"].isNull()) {
        url = doc["url"].as<String>();
    }
    String versao = doc["versao"] | "";

    Serial.printf("[Audio] Parsed - URL: %s, Versao: %s\n", 
                  url.c_str(), versao.c_str());

    if (url.length() == 0) {
        downloading = false;
        Serial.println("[Audio] No audio URL provided - keeping current audio");
        return true;
    }

    // Converte versao para um inteiro simples (hash) para comparação estável
    // Se vier ISO string, usa um hash; se vier número, usa direto.
    int newVersion = 0;
    if (doc["audioVersion"].is<int>()) {
        newVersion = doc["audioVersion"].as<int>();
    } else if (doc["versao"].is<const char*>()) {
        // hash simples
        for (size_t i = 0; i < versao.length(); i++) newVersion = (newVersion * 31) + versao[i];
    }

    Serial.printf("[Audio] Version check - Current: %d, New: %d\n", currentVersion, newVersion);

    if (newVersion != 0 && newVersion == currentVersion) {
        downloading = false;
        Serial.println("[Audio] Audio already up to date - skipping download");
        return true;
    }

    // Baixa para arquivo temporário e ativa
    Serial.printf("[Audio] Starting download from: %s\n", url.c_str());
    if (!downloadFileToTemp(url)) {
        downloading = false;
        if (outError) *outError = "download_failed";
        Serial.println("[Audio] ERROR: Download failed");
        return false;
    }

    Serial.println("[Audio] Download completed, activating file...");
    if (!activateTempAsCurrent()) {
        downloading = false;
        if (outError) *outError = "activate_failed";
        Serial.println("[Audio] ERROR: Failed to activate downloaded file");
        return false;
    }

    lastUrl = String(url);
    if (newVersion != 0) currentVersion = newVersion;

    downloading = false;
    Serial.printf("[Audio] SUCCESS: Audio updated to version %d\n", newVersion);
    return true;
}
