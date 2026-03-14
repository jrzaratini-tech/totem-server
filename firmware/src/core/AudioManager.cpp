#include "core/AudioManager.h"
#include "core/AudioEqualizer.h"
#include "Config.h"
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#define METRICS_LOG_INTERVAL 10000

static AudioManager* gAudioManagerInstance = nullptr;

AudioManager::AudioManager() {
    playing = false;
    downloading = false;
    currentVersion = 0;
    downloadStartMs = 0;
    peakSample = 0;
    lastMetricsLog = 0;
    samplesProcessed = 0;
    clippedSamples = 0;
    equalizer = nullptr;
    gAudioManagerInstance = this;
}

void AudioManager::audio_info(const char *info) {
    Serial.printf("[Audio] Info: %s\n", info);
}

void AudioManager::audio_eof_mp3(const char *info) {
    Serial.printf("[Audio] EOF: %s\n", info);
    if (gAudioManagerInstance) {
        gAudioManagerInstance->playing = false;
    }
}

void AudioManager::begin(const String &totemId, const String &deviceToken) {
    this->totemId = totemId;
    this->deviceToken = deviceToken;
    
    Serial.println("[Audio] ========================================");
    Serial.println("[Audio] INITIALIZING AUDIO SYSTEM");
    Serial.println("[Audio] ========================================");
    
    equalizer = new AudioEqualizer();
    equalizer->begin();
    
    Serial.printf("[Audio] Heap before SPIFFS: %d bytes\n", ESP.getFreeHeap());
    
    if (!SPIFFS.begin(true)) {
        Serial.println("[Audio] SPIFFS init failed - CRITICAL ERROR");
        return;
    }
    Serial.println("[Audio] SPIFFS initialized");
    
    Serial.printf("[Audio] SPIFFS - Total: %d bytes, Used: %d bytes, Free: %d bytes\n",
                 SPIFFS.totalBytes(), SPIFFS.usedBytes(), 
                 SPIFFS.totalBytes() - SPIFFS.usedBytes());
    
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    Serial.println("[Audio] I2S pins configured");
    
    int mappedVol = map(equalizer->getProfile().volume, MIN_VOLUME, MAX_VOLUME, 0, 21);
    audio.setVolume(mappedVol);
    Serial.printf("[Audio] Volume set to %d/21 (profile: %d/10)\n", 
                 mappedVol, equalizer->getProfile().volume);
    
    Serial.println("[Audio] ===== I2S CONFIGURATION =====");
    Serial.println("[Audio] DAC: MAX98357A");
    Serial.printf("[Audio] BCLK (Bit Clock):   GPIO%d → MAX98357A BCLK\n", I2S_BCLK);
    Serial.printf("[Audio] LRC (Word Select):  GPIO%d → MAX98357A LRC\n", I2S_LRC);
    Serial.printf("[Audio] DOUT (Data Out):    GPIO%d → MAX98357A DIN\n", I2S_DOUT);
    Serial.println("[Audio] GAIN:               GND (9dB fixed)");
    Serial.printf("[Audio] Sample Rate:        %d Hz\n", AUDIO_SAMPLE_RATE);
    Serial.printf("[Audio] Bit Depth:          %d-bit\n", AUDIO_BITS_PER_SAMPLE);
    Serial.printf("[Audio] Channels:           %d (Stereo)\n", AUDIO_CHANNELS);
    Serial.printf("[Audio] DMA Buffers:        %d x %d bytes\n", 
                 I2S_DMA_BUFFER_COUNT, I2S_DMA_BUFFER_SIZE);
    Serial.println("[Audio] ====================================");
    
    if (SPIFFS.exists(AUDIO_FILENAME)) {
        File f = SPIFFS.open(AUDIO_FILENAME, FILE_READ);
        if (f) {
            Serial.printf("[Audio] Audio file found: %d bytes\n", f.size());
            f.close();
        }
    } else {
        Serial.println("[Audio] No audio file found - will need to download");
    }
    
    Serial.printf("[Audio] Heap after init: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[Audio] ========================================");
    Serial.println("[Audio] AUDIO SYSTEM READY");
    Serial.println("[Audio] ========================================");
}

void AudioManager::loop() {
    if (playing) {
        audio.loop();
        
        static unsigned long lastWdtReset = 0;
        if (millis() - lastWdtReset > 1000) {
            esp_task_wdt_reset();
            lastWdtReset = millis();
        }
        
        if (!audio.isRunning()) {
            playing = false;
            Serial.println("[Audio] Playback finished");
        }
        
        if (millis() - lastMetricsLog > METRICS_LOG_INTERVAL) {
            Serial.printf("[Audio] Playing... | Heap: %d\n", ESP.getFreeHeap());
            lastMetricsLog = millis();
        }
    }
}

void AudioManager::play() {
    if (playing) {
        Serial.println("[Audio] Already playing - ignoring play request");
        return;
    }
    
    esp_task_wdt_reset();
    
    Serial.println("[Audio] ========================================");
    Serial.println("[Audio] STARTING PLAYBACK");
    
    if (!SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] ✗ File not found: " AUDIO_FILENAME);
        Serial.println("[Audio] ========================================");
        return;
    }
    
    File f = SPIFFS.open(AUDIO_FILENAME, FILE_READ);
    if (!f) {
        Serial.println("[Audio] ✗ Failed to open file");
        Serial.println("[Audio] ========================================");
        return;
    }
    
    size_t fileSize = f.size();
    f.close();
    
    esp_task_wdt_reset();
    
    Serial.printf("[Audio] File: %s (%d bytes)\n", AUDIO_FILENAME, fileSize);
    Serial.printf("[Audio] Heap before playback: %d bytes\n", ESP.getFreeHeap());
    
    peakSample = 0;
    samplesProcessed = 0;
    clippedSamples = 0;
    lastMetricsLog = millis();
    
    if (equalizer) {
        equalizer->resetMetrics();
    }
    
    esp_task_wdt_reset();
    
    Serial.println("[Audio] >>> Calling audio.connecttoFS() - this may take several seconds...");
    Serial.printf("[Audio] >>> Heap before connecttoFS: %d bytes\n", ESP.getFreeHeap());
    unsigned long connectStart = millis();
    
    bool success = audio.connecttoFS(SPIFFS, AUDIO_FILENAME);
    
    unsigned long connectDuration = millis() - connectStart;
    Serial.printf("[Audio] >>> connecttoFS completed in %lu ms\n", connectDuration);
    Serial.printf("[Audio] >>> Heap after connecttoFS: %d bytes\n", ESP.getFreeHeap());
    
    esp_task_wdt_reset();
    if (success) {
        playing = true;
        Serial.println("[Audio] Playback started successfully");
        Serial.printf("[Audio] Volume: %d/10 (EQ profile)\n", 
                     equalizer ? equalizer->getProfile().volume : 0);
        Serial.printf("[Audio] Amplifier Gain: %ddB\n", 
                     equalizer ? equalizer->getProfile().amplifierGain : 15);
        Serial.println("[Audio] ========================================");
    } else {
        Serial.println("[Audio] ✗ Failed to start playback");
        Serial.println("[Audio] Possible causes:");
        Serial.println("[Audio]   - Corrupted MP3 file");
        Serial.println("[Audio]   - Unsupported codec");
        Serial.println("[Audio]   - Insufficient memory");
        Serial.println("[Audio]   - I2S hardware issue");
        Serial.printf("[Audio] Heap: %d bytes\n", ESP.getFreeHeap());
        Serial.println("[Audio] ========================================");
    }
}

void AudioManager::stop() {
    if (!playing) return;
    audio.stopSong();
    playing = false;
    Serial.println("[Audio] Stopped");
}

void AudioManager::setVolume(int vol) {
    int clampedVol = constrain(vol, MIN_VOLUME, MAX_VOLUME);
    if (equalizer) {
        equalizer->setVolume(clampedVol);
    }
    int mappedVol = map(clampedVol, MIN_VOLUME, MAX_VOLUME, 0, 21);
    audio.setVolume(mappedVol);
    Serial.printf("[Audio] Volume set: %d/10 (mapped to %d/21)\n", clampedVol, mappedVol);
}

bool AudioManager::isPlaying() const { return playing; }
bool AudioManager::isDownloading() const { return downloading; }
int AudioManager::getVersion() const { return currentVersion; }
void AudioManager::setVersion(int v) { currentVersion = max(0, v); }

bool AudioManager::downloadFileToTemp(const String &url) {
    esp_task_wdt_reset();
    
    Serial.printf("[Audio] Download: %s\n", url.c_str());
    Serial.printf("[Audio] Heap: %d bytes\n", ESP.getFreeHeap());

    if (SPIFFS.exists(AUDIO_TEMP_FILENAME)) {
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
    }

    size_t freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    Serial.printf("[Audio] SPIFFS free: %d bytes\n", freeBytes);
    
    if (freeBytes < 1048576 && SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] Cleanup old file");
        SPIFFS.remove(AUDIO_FILENAME);
        freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    }

    if (!url.startsWith("https://")) {
        Serial.println("[Audio] URL must be HTTPS");
        return false;
    }

    WiFiClientSecure secure;
    if (String(ROOT_CA_PEM).length() > 0) {
        secure.setCACert(ROOT_CA_PEM);
    } else {
        #if defined(ALLOW_INSECURE_HTTPS) && (ALLOW_INSECURE_HTTPS == 1)
        secure.setInsecure();
        #else
        Serial.println("[Audio] ROOT_CA required");
        return false;
        #endif
    }

    HTTPClient http;
    if (!http.begin(secure, url)) {
        Serial.println("[Audio] HTTP begin failed");
        return false;
    }

    http.setTimeout(30000);
    
    if (deviceToken.length() > 0) {
        http.addHeader("X-Device-Token", deviceToken);
    }

    int code = http.GET();
    Serial.printf("[Audio] Response: %d\n", code);
    
    if (code != 200) {
        http.end();
        return false;
    }

    int len = http.getSize();
    Serial.printf("[Audio] Size: %.1fKB\n", len / 1024.0);
    
    if (len <= 0 || len > (int)MAX_AUDIO_SIZE) {
        http.end();
        Serial.println("[Audio] Invalid size");
        return false;
    }
    
    size_t freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    if ((size_t)len > freeSpace) {
        http.end();
        Serial.println("[Audio] Insufficient space");
        return false;
    }

    esp_task_wdt_reset();
    
    File tmp = SPIFFS.open(AUDIO_TEMP_FILENAME, FILE_WRITE);
    if (!tmp) {
        http.end();
        Serial.println("[Audio] Open temp failed");
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t *buf = (uint8_t*)malloc(1024);
    if (!buf) {
        tmp.close();
        http.end();
        Serial.println("[Audio] Buffer alloc failed");
        return false;
    }
    
    int total = 0;
    int lastPercent = -1;
    unsigned long lastWdtReset = millis();
    
    while (http.connected() && total < len) {
        if (millis() - lastWdtReset > 500) {
            esp_task_wdt_reset();
            lastWdtReset = millis();
        }
        
        if (millis() - downloadStartMs > DOWNLOAD_TIMEOUT) {
            tmp.close();
            http.end();
            free(buf);
            Serial.println("[Audio] Timeout");
            return false;
        }

        size_t avail = stream->available();
        if (avail) {
            size_t toRead = min(avail, (size_t)1024);
            int r = stream->readBytes(buf, toRead);
            if (r > 0) {
                tmp.write(buf, (size_t)r);
                total += r;
                
                int percent = (total * 100) / len;
                if (percent != lastPercent && percent % 25 == 0) {
                    Serial.printf("[Audio] %d%%\n", percent);
                    lastPercent = percent;
                }
            }
        } else {
            delay(10);
        }
        yield();
    }

    tmp.close();
    http.end();
    free(buf);

    if (total != len) {
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
        Serial.println("[Audio] Incomplete download");
        return false;
    }

    Serial.printf("[Audio] Downloaded %d bytes\n", total);
    return true;
}

bool AudioManager::activateTempAsCurrent() {
    if (!SPIFFS.exists(AUDIO_TEMP_FILENAME)) {
        Serial.println("[Audio] Temp file missing");
        return false;
    }

    if (SPIFFS.exists(AUDIO_FILENAME)) {
        SPIFFS.remove(AUDIO_FILENAME);
    }

    bool ok = SPIFFS.rename(AUDIO_TEMP_FILENAME, AUDIO_FILENAME);
    if (!ok) {
        Serial.println("[Audio] Rename failed");
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
        return false;
    }

    Serial.println("[Audio] Activated");
    return true;
}

bool AudioManager::checkAndDownloadFromServer(String *outError) {
    if (downloading) {
        if (outError) *outError = "download_in_progress";
        return false;
    }

    esp_task_wdt_reset();
    
    downloading = true;
    downloadStartMs = millis();
    Serial.println("[Audio] Checking server...");

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

    esp_task_wdt_reset();
    
    int code = http.GET();
    Serial.printf("[Audio] API: %d\n", code);
    if (code != 200) {
        http.end();
        downloading = false;
        if (outError) *outError = String("api_http_") + code;
        return false;
    }

    String body = http.getString();
    http.end();

    StaticJsonDocument<768> doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        downloading = false;
        if (outError) *outError = "json_invalid";
        return false;
    }

    String url = "";
    if (doc.containsKey("url") && !doc["url"].isNull()) {
        url = doc["url"].as<String>();
    }
    String versao = doc["versao"] | "";

    if (url.length() == 0) {
        downloading = false;
        Serial.println("[Audio] No URL");
        return true;
    }

    int newVersion = 0;
    if (doc["audioVersion"].is<int>()) {
        newVersion = doc["audioVersion"].as<int>();
    } else if (doc["versao"].is<const char*>()) {
        for (size_t i = 0; i < versao.length(); i++) newVersion = (newVersion * 31) + versao[i];
    }

    Serial.printf("[Audio] Ver: %d -> %d\n", currentVersion, newVersion);

    if (newVersion != 0 && newVersion == currentVersion) {
        downloading = false;
        Serial.println("[Audio] Up to date");
        return true;
    }

    esp_task_wdt_reset();
    
    if (!downloadFileToTemp(url)) {
        downloading = false;
        if (outError) *outError = "download_failed";
        return false;
    }

    if (!activateTempAsCurrent()) {
        downloading = false;
        if (outError) *outError = "activate_failed";
        return false;
    }

    lastUrl = String(url);
    if (newVersion != 0) currentVersion = newVersion;

    downloading = false;
    Serial.printf("[Audio] Updated to v%d\n", newVersion);
    return true;
}

void AudioManager::playTestTone(int durationMs) {
    Serial.println("[Audio] ========================================");
    Serial.println("[Audio] GENERATING TEST TONE FOR HARDWARE DIAGNOSTICS");
    Serial.printf("[Audio] Frequency: %d Hz\n", AUDIO_TEST_TONE_FREQ);
    Serial.printf("[Audio] Duration: %d ms\n", durationMs);
    Serial.printf("[Audio] Sample Rate: %d Hz\n", AUDIO_SAMPLE_RATE);
    Serial.println("[Audio] ========================================");
    
    if (playing) {
        Serial.println("[Audio] Stopping current playback...");
        stop();
        delay(100);
    }
    
    const int sampleRate = AUDIO_SAMPLE_RATE;
    const int frequency = AUDIO_TEST_TONE_FREQ;
    const int amplitude = 16000;
    const int numSamples = (sampleRate * durationMs) / 1000;
    
    Serial.printf("[Audio] Generating %d samples...\n", numSamples);
    
    int16_t *samples = (int16_t*)malloc(numSamples * 4);
    if (!samples) {
        Serial.println("[Audio] Failed to allocate memory for test tone");
        return;
    }
    
    for (int i = 0; i < numSamples; i++) {
        float angle = 2.0f * PI * frequency * i / sampleRate;
        int16_t sample = (int16_t)(amplitude * sin(angle));
        samples[i * 2] = sample;
        samples[i * 2 + 1] = sample;
    }
    
    Serial.println("[Audio] Test tone generated successfully");
    Serial.println("[Audio] NOTE: ESP32-audioI2S library doesn't support raw I2S write");
    Serial.println("[Audio] To test hardware:");
    Serial.println("[Audio] 1. Upload a 1kHz test tone MP3 file");
    Serial.println("[Audio] 2. Use audio.connecttoFS() to play it");
    Serial.println("[Audio] 3. Check I2S signals with oscilloscope:");
    Serial.printf("[Audio]    - BCLK (GPIO%d): ~1.4 MHz square wave\n", I2S_BCLK);
    Serial.printf("[Audio]    - LRC (GPIO%d): 44.1 kHz square wave\n", I2S_LRC);
    Serial.printf("[Audio]    - DOUT (GPIO%d): Variable data during playback\n", I2S_DOUT);
    Serial.println("[Audio]    - GAIN: Connected to GND (9dB fixed gain)");
    Serial.println("[Audio] 4. MAX98357A connections:");
    Serial.println("[Audio]    - VIN: 5V power supply");
    Serial.println("[Audio]    - GND: Ground");
    Serial.println("[Audio]    - Speaker: 4-8Ω between OUT+ and OUT-");
    Serial.println("[Audio] ========================================");
    
    free(samples);
}

AudioEqualizer* AudioManager::getEqualizer() {
    return equalizer;
}