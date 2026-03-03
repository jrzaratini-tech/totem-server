#include "core/MQTTManager.h"
#include "Config.h"

MQTTManager::MQTTManager() : client(net) {
    connected = false;
    lastReconnectAttempt = 0;
}

String MQTTManager::topicBase() const {
    return String("totem/") + totemId;
}

String MQTTManager::topicTrigger() const { return topicBase() + "/trigger"; }
String MQTTManager::topicConfigUpdate() const { return topicBase() + "/configUpdate"; }
String MQTTManager::topicAudioUpdate() const { return topicBase() + "/audioUpdate"; }
String MQTTManager::topicFirmwareUpdate() const { return topicBase() + "/firmwareUpdate"; }
String MQTTManager::topicStatus() const { return topicBase() + "/status"; }

void MQTTManager::begin(const String &totemId, const String &deviceToken) {
    this->totemId = totemId;
    this->deviceToken = deviceToken;

    Serial.printf("[MQTT] Connecting to broker: %s:%d\n", MQTT_SERVER, MQTT_PORT);
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setKeepAlive(60);

    client.setCallback([this](char *t, byte *p, unsigned int l) {
        this->internalCallback(t, p, l);
    });

    connect();
}

void MQTTManager::loop() {
    if (!client.connected()) {
        connected = false;
        if (millis() - lastReconnectAttempt > 5000) {
            connect();
            lastReconnectAttempt = millis();
        }
        return;
    }

    connected = true;
    client.loop();
}

bool MQTTManager::connect() {
    String clientId = String("totem-") + totemId + "-" + String((uint32_t)esp_random(), HEX);

    Serial.printf("[MQTT] Client ID: %s\n", clientId.c_str());

    const String willTopic = topicStatus();
    const char *willMessage = "{\"online\":false}";
    const int willQos = 0;
    const bool willRetain = true;

    const bool hasUser = String(MQTT_USER).length() > 0;
    const bool hasToken = deviceToken.length() > 0;

    bool ok = false;
    if (hasUser) {
        ok = client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD, willTopic.c_str(), willQos, willRetain, willMessage);
    } else if (hasToken) {
        ok = client.connect(clientId.c_str(), deviceToken.c_str(), "", willTopic.c_str(), willQos, willRetain, willMessage);
    } else {
        ok = client.connect(clientId.c_str(), nullptr, nullptr, willTopic.c_str(), willQos, willRetain, willMessage);
    }

    if (!ok) {
        Serial.printf("[MQTT] Connection FAILED! State: %d\n", client.state());
        return false;
    }

    Serial.println("[MQTT] Connected successfully!");
    connected = true;
    subscribeTopics();

    publish(topicStatus(), "{\"online\":true}", true);
    Serial.println("[MQTT] Published online status");
    return true;
}

void MQTTManager::subscribeTopics() {
    Serial.println("[MQTT] Subscribing to topics:");
    Serial.printf("  - %s\n", topicTrigger().c_str());
    client.subscribe(topicTrigger().c_str());
    Serial.printf("  - %s\n", topicConfigUpdate().c_str());
    client.subscribe(topicConfigUpdate().c_str());
    Serial.printf("  - %s\n", topicAudioUpdate().c_str());
    client.subscribe(topicAudioUpdate().c_str());
    Serial.printf("  - %s\n", topicFirmwareUpdate().c_str());
    client.subscribe(topicFirmwareUpdate().c_str());
}

bool MQTTManager::publish(const String &topic, const String &payload, bool retained) {
    if (!client.connected()) return false;
    return client.publish(topic.c_str(), payload.c_str(), retained);
}

void MQTTManager::internalCallback(char *topic, byte *payload, unsigned int length) {
    String t(topic);
    String p;
    p.reserve(length);
    for (unsigned int i = 0; i < length; i++) p += (char)payload[i];

    Serial.printf("[MQTT] Message received on topic: %s\n", t.c_str());
    Serial.printf("[MQTT] Payload (%d bytes): %s\n", length, p.c_str());

    if (messageCallback) messageCallback(t, p);
}

void MQTTManager::onMessage(std::function<void(const String&, const String&)> cb) {
    messageCallback = cb;
}

bool MQTTManager::isConnected() {
    return connected && client.connected();
}
