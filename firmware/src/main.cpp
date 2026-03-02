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
static WiFiManager wifiManager;
static MQTTManager mqttManager;
static ConfigManager configManager;
static StorageManager storageManager;
static AudioManager audioManager;
static LEDEngine ledEngine;
static ButtonManager buttonManager;
static OTAManager otaManager;

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
        if (topic.endsWith("/trigger")) {
            if (payload == "play") {
                if (!stateMachine.canPlay()) return;
                if (!stateMachine.setState(PLAYING)) return;

                ledEngine.startEffect(configManager.getEffectConfig());
                audioManager.play();
                playEndMs = millis() + (unsigned long)configManager.getEffectConfig().duration * 1000UL;
            }
            return;
        }

        if (topic.endsWith("/configUpdate")) {
            if (!stateMachine.setState(UPDATING_CONFIG)) return;

            bool ok = configManager.updateFromJson(payload);
            if (!ok) {
                safeEnterError("json_invalid_config");
                return;
            }

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

    // Watchdog
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);

    // Rollback OTA: se firmware está em modo de verificação, marca como válido no boot
    // (evita ficar preso em loop de rollback quando o app está saudável)
    esp_ota_mark_app_valid_cancel_rollback();

    stateMachine.setState(BOOT);

    storageManager.begin();
    configManager.begin();

    gTotemId = storageManager.getTotemId();
    gDeviceToken = storageManager.getDeviceToken();

    audioManager.setVersion(storageManager.getAudioVersion());

    ledEngine.begin(NUM_LEDS_MAIN, NUM_LEDS_HEART);
    ledEngine.setBrightness(configManager.getBrightness());
    ledEngine.setColor(configManager.getColor());

    buttonManager.begin();
    setupButtonCallbacks();

    audioManager.begin(gTotemId, gDeviceToken);

    wifiManager.begin(gTotemId);

    mqttManager.begin(gTotemId, gDeviceToken);
    setupMQTTCallbacks();

    otaManager.begin();

    stateMachine.setState(IDLE);
    lastStatusMs = 0;
}

void loop() {
    esp_task_wdt_reset();

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
