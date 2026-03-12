#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>

#include "Config.h"

#include "core/StateMachine.h"
#include "core/WiFiManager.h"
#include "core/MQTTManager.h"
#include "core/ConfigManager.h"
#include "core/StorageManager.h"
#include "core/AudioManager.h"
#include "core/LEDEngine.h"
#include "core/ButtonManager.h"
#include "core/OTAManager.h"
#include "utils/HeartbeatLED.h"

static StateMachine stateMachine;
static TotemWiFiManager wifiManager;
static MQTTManager mqttManager;
static ConfigManager configManager;
static StorageManager storageManager;
static AudioManager audioManager;
static LEDEngine ledEngine;
static ButtonManager buttonManager;
static OTAManager otaManager;
static HeartbeatLED heartbeatLED;

static bool gWdtStarted = false;

static String gTotemId;
static String gDeviceToken;

static unsigned long lastStatusMs = 0;
static unsigned long playEndMs = 0;
static unsigned long lastHeapCheck = 0;
static unsigned long triggerStartMs = 0;

static void publishStatus() {
    StaticJsonDocument<384> doc;
    doc["online"] = true;
    doc["fw"] = FIRMWARE_VERSION;
    doc["audioVersion"] = storageManager.getAudioVersion();
    doc["rssi"] = wifiManager.getRSSI();
    doc["ip"] = wifiManager.getIP();
    doc["state"] = stateMachine.getStateName();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["minFreeHeap"] = ESP.getMinFreeHeap();

    String out;
    serializeJson(doc, out);
    mqttManager.publish(mqttManager.topicStatus(), out, true);
}

static void safeEnterError(const String &err) {
    stateMachine.setError(err);
    // modo seguro: LED branco baixo brilho
    ledEngine.setBrightness(FAILSAFE_BRIGHTNESS);
    ledEngine.setColor(0xFFFFFF);
    ledEngine.startEffect(configManager.getEffectConfig());
    audioManager.stop();
}

static void setupButtonCallbacks() {
    buttonManager.onButtonCor([](bool longPress) {
        configManager.cycleColor();
        ledEngine.setColor(configManager.getColor());
    });

    buttonManager.onButtonMais([](bool longPress) {
        int cur = configManager.getBrightness();
        int step = longPress ? 25 : 10;
        int next = min(cur + step, (int)MAX_BRIGHTNESS);
        configManager.setBrightness(next);
        ledEngine.setBrightness(next);
    });

    buttonManager.onButtonMenos([](bool longPress) {
        int cur = configManager.getBrightness();
        int step = longPress ? 25 : 10;
        int next = max(cur - step, 0);
        configManager.setBrightness(next);
        ledEngine.setBrightness(next);
    });

    buttonManager.onButtonCoracao([](bool longPress) {
        (void)longPress;
        if (!stateMachine.canPlay()) return;
        if (!stateMachine.setState(PLAYING)) return;

        ledEngine.startEffect(configManager.getEffectConfig());
        audioManager.play();
        playEndMs = millis() + (unsigned long)configManager.getEffectConfig().duration * 1000UL;
    });
}

// Variáveis globais para configurações duais
static ConfigData idleConfig;
static ConfigData triggerConfig;
static int currentVolume = 10; // Volume máximo por padrão

static void setupMQTTCallbacks() {
    mqttManager.onMessage([](const String &topic, const String &payload) {
        Serial.printf("[MAIN] MQTT callback - Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str());
        
        // Sistema Dual v4.1.0 - Config Idle
        if (topic.endsWith("/config/idle")) {
            Serial.println("[MAIN] Received IDLE config");
            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, payload) == DeserializationError::Ok) {
                idleConfig.mode = configManager.modeFromString(doc["mode"] | "BREATH");
                idleConfig.color = configManager.parseColor(doc["color"] | "#ff3366");
                idleConfig.speed = doc["speed"] | 50;
                idleConfig.maxBrightness = doc["maxBrightness"] | 120;
                idleConfig.duration = 0; // Idle roda continuamente
                Serial.printf("[MAIN] Idle config updated - Mode: %d, Color: 0x%06X, Brightness: %d\n",
                             idleConfig.mode, idleConfig.color, idleConfig.maxBrightness);
                
                // Aplicar idle config se não estiver tocando
                if (stateMachine.getState() == IDLE) {
                    ledEngine.setBrightness(idleConfig.maxBrightness);
                    ledEngine.setColor(idleConfig.color);
                    ledEngine.startEffect(idleConfig);
                }
            }
            return;
        }
        
        // Sistema Dual v4.1.0 - Config Trigger
        if (topic.endsWith("/config/trigger")) {
            Serial.println("[MAIN] Received TRIGGER config");
            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, payload) == DeserializationError::Ok) {
                triggerConfig.mode = configManager.modeFromString(doc["mode"] | "RAINBOW");
                triggerConfig.color = configManager.parseColor(doc["color"] | "#00ff00");
                triggerConfig.speed = doc["speed"] | 70;
                triggerConfig.maxBrightness = doc["maxBrightness"] | 150;
                triggerConfig.duration = doc["duration"] | 30;
                Serial.printf("[MAIN] Trigger config updated - Mode: %d, Color: 0x%06X, Duration: %d\n",
                             triggerConfig.mode, triggerConfig.color, triggerConfig.duration);
                
                // AUTO-PLAY: Disparar efeito automaticamente quando configuração de trigger é atualizada
                if (stateMachine.canPlay() && SPIFFS.exists(AUDIO_FILENAME)) {
                    Serial.println("[MAIN] *** AUTO-PLAY: Trigger config updated, playing automatically ***");
                    if (stateMachine.setState(PLAYING)) {
                        ledEngine.startEffect(triggerConfig);
                        audioManager.play();
                        playEndMs = millis() + (unsigned long)triggerConfig.duration * 1000UL;
                    }
                }
            }
            return;
        }
        
        // Sistema Dual v4.1.0 - Volume
        if (topic.endsWith("/config/volume")) {
            Serial.println("[MAIN] ========================================");
            Serial.println("[MAIN] MQTT VOLUME UPDATE RECEIVED");
            Serial.printf("[MAIN] Topic: %s\n", topic.c_str());
            Serial.printf("[MAIN] Payload: '%s'\n", payload.c_str());
            
            int vol = payload.toInt();
            Serial.printf("[MAIN] Parsed volume: %d\n", vol);
            
            if (vol >= 0 && vol <= 10) {
                currentVolume = vol;
                Serial.printf("[MAIN] Calling audioManager.setVolume(%d)...\n", vol);
                audioManager.setVolume(vol);
                Serial.printf("[MAIN] ✓ Volume successfully set to %d/10\n", vol);
            } else {
                Serial.printf("[MAIN] ✗ Invalid volume value: %d (must be 0-10)\n", vol);
            }
            
            Serial.println("[MAIN] ========================================");
            return;
        }
        
        // Audio Equalizer Config
        if (topic.endsWith("/audioConfig")) {
            Serial.println("[MAIN] ========================================");
            Serial.println("[MAIN] AUDIO EQUALIZER CONFIG RECEIVED");
            Serial.printf("[MAIN] Topic: %s\n", topic.c_str());
            Serial.printf("[MAIN] Payload: %s\n", payload.c_str());
            
            if (audioManager.getEqualizer()->profileFromJSON(payload)) {
                Serial.println("[MAIN] ✓ Audio equalizer profile updated successfully");
                audioManager.getEqualizer()->printProfile();
            } else {
                Serial.println("[MAIN] ✗ Failed to parse equalizer config JSON");
            }
            
            Serial.println("[MAIN] ========================================");
            return;
        }
        
        if (topic.endsWith("/trigger")) {
            triggerStartMs = millis();
            Serial.println("[MAIN] ========================================");
            Serial.println("[MAIN] TRIGGER COMMAND RECEIVED");
            Serial.printf("[MAIN] Payload: '%s'\n", payload.c_str());
            Serial.printf("[MAIN] Heap - Free: %d bytes, Min: %d bytes\n", 
                         ESP.getFreeHeap(), ESP.getMinFreeHeap());
            Serial.println("[MAIN] ========================================");
            
            if (payload == "play") {
                Serial.println("[MAIN] ✓ Play command detected");
                Serial.printf("[MAIN] Current state: %s\n", stateMachine.getStateName());
                
                if (!stateMachine.canPlay()) {
                    Serial.printf("[MAIN] ✗ Cannot play - current state: %s\n", stateMachine.getStateName());
                    return;
                }
                
                Serial.println("[MAIN] ✓ State allows playback");
                
                if (!stateMachine.setState(PLAYING)) {
                    Serial.println("[MAIN] ✗ Failed to set PLAYING state");
                    return;
                }
                
                Serial.println("[MAIN] ✓ State changed to PLAYING");
                
                // Usar triggerConfig se disponível, senão usar config padrão
                ConfigData cfg = (triggerConfig.duration > 0) ? triggerConfig : configManager.getEffectConfig();
                Serial.printf("[MAIN] Trigger config - Mode: %d, Color: 0x%06X, Speed: %d, Duration: %d, Brightness: %d\n",
                             cfg.mode, cfg.color, cfg.speed, cfg.duration, cfg.maxBrightness);
                
                Serial.println("[MAIN] Starting LED effect (trigger mode)...");
                ledEngine.startEffect(cfg);
                Serial.println("[MAIN] ✓ LED effect started");

                // Verificar se arquivo de áudio existe
                Serial.printf("[MAIN] Checking for audio file: %s\n", AUDIO_FILENAME);
                if (!SPIFFS.exists(AUDIO_FILENAME)) {
                    Serial.println("[MAIN] ✗ Audio file missing, attempting download...");
                    String err;
                    bool ok = audioManager.checkAndDownloadFromServer(&err);
                    if (!ok) {
                        Serial.printf("[MAIN] ✗ Download failed: %s\n", err.c_str());
                        safeEnterError("audio_missing_and_download_failed:" + err);
                        return;
                    }
                    storageManager.setAudioVersion(audioManager.getVersion());
                    configManager.setAudioVersion(audioManager.getVersion());
                    Serial.println("[MAIN] ✓ Audio downloaded successfully");
                } else {
                    File f = SPIFFS.open(AUDIO_FILENAME, FILE_READ);
                    if (f) {
                        Serial.printf("[MAIN] ✓ Audio file exists - Size: %d bytes\n", f.size());
                        f.close();
                    }
                }

                Serial.println("[MAIN] Starting audio playback...");
                audioManager.play();
                playEndMs = millis() + (unsigned long)cfg.duration * 1000UL;
                unsigned long latency = millis() - triggerStartMs;
                Serial.printf("[MAIN] ✓ Playback started - Latency: %lu ms\n", latency);
                Serial.printf("[MAIN] ✓ Will end at: %lu ms (duration: %d sec)\n", 
                             playEndMs, cfg.duration);
                if (latency > 300) {
                    Serial.printf("[MAIN] ⚠ WARNING: High latency detected (%lu ms > 300ms target)\n", latency);
                }
                Serial.println("[MAIN] ========================================");
            } else {
                Serial.printf("[MAIN] ✗ Unknown trigger payload: '%s'\n", payload.c_str());
            }
            return;
        }

        if (topic.endsWith("/configUpdate")) {
            if (configManager.isDuplicateConfigUpdate(payload)) {
                return;
            }
            if (!stateMachine.setState(UPDATING_CONFIG)) return;

            bool ok = configManager.updateFromJson(payload);
            if (!ok) {
                safeEnterError("json_invalid_config");
                return;
            }

            configManager.rememberConfigUpdate(payload);

            ledEngine.setBrightness(configManager.getBrightness());
            ledEngine.setColor(configManager.getColor());

            stateMachine.setState(IDLE);
            return;
        }

        if (topic.endsWith("/audioUpdate")) {
            if (!stateMachine.setState(DOWNLOADING_AUDIO)) return;

            String err;
            bool ok = audioManager.checkAndDownloadFromServer(&err);
            if (!ok) {
                safeEnterError("audio_download_failed:" + err);
                return;
            }

            storageManager.setAudioVersion(audioManager.getVersion());
            configManager.setAudioVersion(audioManager.getVersion());

            // AUTO-PLAY: Reproduzir automaticamente quando novo áudio é baixado
            Serial.println("[MAIN] *** AUTO-PLAY: New audio downloaded, playing automatically ***");
            if (stateMachine.setState(PLAYING)) {
                ConfigData cfg = (triggerConfig.duration > 0) ? triggerConfig : configManager.getEffectConfig();
                ledEngine.startEffect(cfg);
                audioManager.play();
                playEndMs = millis() + (unsigned long)cfg.duration * 1000UL;
                Serial.printf("[MAIN] Auto-play started - Duration: %d sec\n", cfg.duration);
            } else {
                stateMachine.setState(IDLE);
            }
            return;
        }

        if (topic.endsWith("/firmwareUpdate")) {
            if (!stateMachine.setState(UPDATING_FIRMWARE)) return;

            bool ok = otaManager.startUpdateFromUrl(payload);
            if (!ok) {
                safeEnterError("ota_failed");
                return;
            }
            return;
        }
    });
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("[BOOT] ========================================");
    Serial.println("[BOOT] ESP32 IoT Totem - Production Firmware");
    Serial.println("[BOOT] ========================================");
    Serial.printf("[BOOT] Firmware version: %s\n", FIRMWARE_VERSION);
    Serial.printf("[BOOT] Totem ID: %s\n", TOTEM_ID);
    Serial.printf("[BOOT] Reset reason: %d\n", (int)esp_reset_reason());
    Serial.printf("[BOOT] Heap at boot - Free: %d bytes, Min: %d bytes\n", 
                 ESP.getFreeHeap(), ESP.getMinFreeHeap());
    Serial.printf("[BOOT] Flash size: %d bytes\n", ESP.getFlashChipSize());
    Serial.printf("[BOOT] CPU frequency: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.flush();

    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("[BOOT] OTA rollback cancelled");

    stateMachine.setState(BOOT);
    Serial.println("[BOOT] State machine initialized");

    storageManager.begin();
    Serial.println("[BOOT] Storage initialized");
    
    configManager.begin();
    Serial.println("[BOOT] Config initialized");

    if (String(FORCE_TOTEM_ID).length() > 0) {
        storageManager.setTotemId(String(FORCE_TOTEM_ID));
    }

    gTotemId = storageManager.getTotemId();
    gDeviceToken = storageManager.getDeviceToken();
    Serial.printf("[BOOT] TotemID=%s\n", gTotemId.c_str());

    audioManager.setVersion(storageManager.getAudioVersion());
    Serial.println("[BOOT] Audio version set");

    ledEngine.begin(NUM_LEDS_MAIN, NUM_LEDS_HEART);
    ledEngine.setBrightness(configManager.getBrightness());
    ledEngine.setColor(configManager.getColor());
    Serial.println("[BOOT] LED engine initialized");

    buttonManager.begin();
    setupButtonCallbacks();
    Serial.println("[BOOT] Button manager initialized");

    Serial.println("[BOOT] Starting audio manager...");
    audioManager.begin(gTotemId, gDeviceToken);
    audioManager.setVolume(10);  // Volume máximo
    Serial.println("[BOOT] Audio manager initialized with max volume");

    Serial.println("[BOOT] Starting WiFi manager...");
    wifiManager.begin(gTotemId);
    Serial.println("[BOOT] WiFi manager initialized");

    Serial.println("[BOOT] Initializing watchdog...");
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_SECONDS * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
    gWdtStarted = true;
    Serial.println("[BOOT] Watchdog started");

    Serial.println("[BOOT] Starting MQTT manager...");
    mqttManager.begin(gTotemId, gDeviceToken);
    setupMQTTCallbacks();
    Serial.println("[BOOT] MQTT manager initialized");

    otaManager.begin();
    Serial.println("[BOOT] OTA manager initialized");

    heartbeatLED.begin(1, 2);
    Serial.println("[BOOT] Standalone heartbeat LED initialized on GPIO1 and GPIO2 (ESP32-S3)");

    stateMachine.setState(IDLE);
    lastStatusMs = 0;
    lastHeapCheck = millis();
    
    Serial.println("[BOOT] ========================================");
    Serial.println("[BOOT] Setup complete - entering IDLE state");
    Serial.printf("[BOOT] Heap after setup - Free: %d bytes, Min: %d bytes\n", 
                 ESP.getFreeHeap(), ESP.getMinFreeHeap());
    
    // Verificar se heap mínimo está acima do limite seguro
    if (ESP.getMinFreeHeap() < 120000) {
        Serial.printf("[BOOT] ⚠ WARNING: Low minimum free heap (%d < 120KB)\n", 
                     ESP.getMinFreeHeap());
    } else {
        Serial.println("[BOOT] ✓ Heap check passed (>120KB free)");
    }
    
    Serial.println("[BOOT] ========================================");
    Serial.flush();
}

void loop() {
    if (gWdtStarted) esp_task_wdt_reset();

    wifiManager.loop();
    mqttManager.loop();
    buttonManager.loop();
    ledEngine.loop();
    audioManager.loop();
    otaManager.loop();
    heartbeatLED.update();

    if (stateMachine.getState() == PLAYING) {
        static unsigned long lastPlayingLog = 0;
        if (millis() - lastPlayingLog > 5000) {
            unsigned long remaining = (playEndMs > millis()) ? (playEndMs - millis()) / 1000 : 0;
            Serial.printf("[LOOP] PLAYING - Audio: %s, Remaining: %lu sec\n", 
                         audioManager.isPlaying() ? "YES" : "NO", remaining);
            lastPlayingLog = millis();
        }
        
        if ((playEndMs != 0 && millis() > playEndMs) || !audioManager.isPlaying()) {
            Serial.println("[LOOP] Playback finished - stopping audio and returning to idle mode");
            audioManager.stop();
            playEndMs = 0;
            stateMachine.setState(IDLE);
            
            // Retornar ao modo idle
            if (idleConfig.maxBrightness > 0) {
                Serial.println("[LOOP] Switching to IDLE LED mode");
                ledEngine.setBrightness(idleConfig.maxBrightness);
                ledEngine.setColor(idleConfig.color);
                ledEngine.startEffect(idleConfig);
            } else {
                ledEngine.stop();
            }
            Serial.println("[LOOP] Returned to IDLE state");
        }
    }

    if (millis() - lastStatusMs >= STATUS_INTERVAL) {
        publishStatus();
        lastStatusMs = millis();
    }
    
    // Monitoramento de heap a cada 30 segundos
    if (millis() - lastHeapCheck >= 30000) {
        size_t freeHeap = ESP.getFreeHeap();
        size_t minFreeHeap = ESP.getMinFreeHeap();
        
        if (freeHeap < 80000) {
            Serial.printf("[HEAP] ⚠ WARNING: Low free heap: %d bytes\n", freeHeap);
        }
        
        if (minFreeHeap < 120000) {
            Serial.printf("[HEAP] ⚠ WARNING: Low minimum free heap: %d bytes\n", minFreeHeap);
        }
        
        lastHeapCheck = millis();
    }

    // recuperação simples do ERROR
    if (stateMachine.getState() == ERROR) {
        if (wifiManager.isConnected() && mqttManager.isConnected()) {
            ledEngine.stop();
            stateMachine.setState(IDLE);
        }
    }

    yield();
}
