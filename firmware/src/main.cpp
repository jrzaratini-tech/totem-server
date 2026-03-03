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

static StateMachine stateMachine;
static TotemWiFiManager wifiManager;
static MQTTManager mqttManager;
static ConfigManager configManager;
static StorageManager storageManager;
static AudioManager audioManager;
static LEDEngine ledEngine;
static ButtonManager buttonManager;
static OTAManager otaManager;

static bool gWdtStarted = false;

static String gTotemId;
static String gDeviceToken;

static unsigned long lastStatusMs = 0;
static unsigned long playEndMs = 0;

static void publishStatus() {
    StaticJsonDocument<384> doc;
    doc["online"] = true;
    doc["fw"] = FIRMWARE_VERSION;
    doc["audioVersion"] = storageManager.getAudioVersion();
    doc["rssi"] = wifiManager.getRSSI();
    doc["ip"] = wifiManager.getIP();
    doc["state"] = stateMachine.getStateName();

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
        if (longPress) {
            wifiManager.resetWiFiSettings();
            return;
        }
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

static void setupMQTTCallbacks() {
    mqttManager.onMessage([](const String &topic, const String &payload) {
        Serial.printf("[MAIN] MQTT callback - Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str());
        
        if (topic.endsWith("/trigger")) {
            Serial.println("[MAIN] Trigger command received");
            if (payload == "play") {
                Serial.println("[MAIN] Play command detected");
                
                if (!stateMachine.canPlay()) {
                    Serial.printf("[MAIN] Cannot play - current state: %s\n", stateMachine.getStateName());
                    return;
                }
                
                if (!stateMachine.setState(PLAYING)) {
                    Serial.println("[MAIN] Failed to set PLAYING state");
                    return;
                }
                
                Serial.println("[MAIN] State changed to PLAYING");
                ledEngine.startEffect(configManager.getEffectConfig());

                if (!SPIFFS.exists(AUDIO_FILENAME)) {
                    Serial.println("[MAIN] Audio file missing, attempting download...");
                    String err;
                    bool ok = audioManager.checkAndDownloadFromServer(&err);
                    if (!ok) {
                        Serial.printf("[MAIN] Download failed: %s\n", err.c_str());
                        safeEnterError("audio_missing_and_download_failed:" + err);
                        return;
                    }
                    storageManager.setAudioVersion(audioManager.getVersion());
                    configManager.setAudioVersion(audioManager.getVersion());
                    Serial.println("[MAIN] Audio downloaded successfully");
                }

                Serial.println("[MAIN] Starting audio playback...");
                audioManager.play();
                playEndMs = millis() + (unsigned long)configManager.getEffectConfig().duration * 1000UL;
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

            stateMachine.setState(IDLE);
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
    Serial.println("[BOOT] app start");
    Serial.printf("[BOOT] reset_reason=%d\n", (int)esp_reset_reason());
    Serial.printf("[BOOT] fw=%s\n", FIRMWARE_VERSION);
    Serial.printf("[BOOT] totem_id=%s\n", TOTEM_ID);
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
    Serial.println("[BOOT] Audio manager initialized");

    Serial.println("[BOOT] Starting WiFi manager...");
    wifiManager.begin(gTotemId);
    Serial.println("[BOOT] WiFi manager initialized");

    Serial.println("[BOOT] Initializing watchdog...");
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);
    gWdtStarted = true;
    Serial.println("[BOOT] Watchdog started");

    Serial.println("[BOOT] Starting MQTT manager...");
    mqttManager.begin(gTotemId, gDeviceToken);
    setupMQTTCallbacks();
    Serial.println("[BOOT] MQTT manager initialized");

    otaManager.begin();
    Serial.println("[BOOT] OTA manager initialized");

    stateMachine.setState(IDLE);
    lastStatusMs = 0;
    Serial.println("[BOOT] Setup complete - entering IDLE state");
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

    if (stateMachine.getState() == PLAYING) {
        if ((playEndMs != 0 && millis() > playEndMs) || !audioManager.isPlaying()) {
            ledEngine.stop();
            audioManager.stop();
            playEndMs = 0;
            stateMachine.setState(IDLE);
        }
    }

    if (millis() - lastStatusMs >= STATUS_INTERVAL) {
        publishStatus();
        lastStatusMs = millis();
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
