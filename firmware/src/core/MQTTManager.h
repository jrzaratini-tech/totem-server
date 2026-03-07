#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <functional>

class MQTTManager {
private:
    WiFiClient net;
    PubSubClient client;

    bool connected;
    unsigned long lastReconnectAttempt;

    std::function<void(const String&, const String&)> messageCallback;

    String totemId;
    String deviceToken;

    String topicBase() const;

    void internalCallback(char *topic, byte *payload, unsigned int length);

public:
    MQTTManager();

    void begin(const String &totemId, const String &deviceToken);
    void loop();

    bool connect();
    void subscribeTopics();

    bool isConnected();

    bool publish(const String &topic, const String &payload, bool retained = false);

    String topicTrigger() const;
    String topicConfigUpdate() const;
    String topicConfigIdle() const;
    String topicConfigTrigger() const;
    String topicConfigVolume() const;
    String topicAudioUpdate() const;
    String topicAudioConfig() const;
    String topicFirmwareUpdate() const;
    String topicStatus() const;

    void onMessage(std::function<void(const String&, const String&)> cb);
};

#endif
