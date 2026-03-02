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

    // Last Will: quando cair, broker publica offline
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
        // usa token como username quando não há user/senha fixos
        ok = client.connect(clientId.c_str(), deviceToken.c_str(), "", willTopic.c_str(), willQos, willRetain, willMessage);
    } else {
        ok = client.connect(clientId.c_str(), nullptr, nullptr, willTopic.c_str(), willQos, willRetain, willMessage);
    }

    if (!ok) {
        return false;
    }

    connected = true;
    subscribeTopics();

    // Publica online logo ao conectar
    publish(topicStatus(), "{\"online\":true}", true);
    return true;
}

void MQTTManager::subscribeTopics() {
    client.subscribe(topicTrigger().c_str());
    client.subscribe(topicConfigUpdate().c_str());
    client.subscribe(topicAudioUpdate().c_str());
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

    if (messageCallback) messageCallback(t, p);
}

void MQTTManager::onMessage(std::function<void(const String&, const String&)> cb) {
    messageCallback = cb;
}

bool MQTTManager::isConnected() const {
    return connected && client.connected();
}
