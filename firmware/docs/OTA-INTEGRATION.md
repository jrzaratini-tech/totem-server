# 🔧 Integração OTA no Firmware ESP32-S3

## 📋 Guia de Integração

Este documento explica como integrar o OTAManager no firmware do totem ESP32-S3.

---

## 1. Incluir o Header

```cpp
#include "OTAManager.h"
```

---

## 2. Declarar Variáveis Globais

```cpp
// MQTT Client (já existente)
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// OTA Manager
OTAManager* otaManager = nullptr;

// Configurações
const char* TOTEM_ID = "totem123";
const char* FIRMWARE_VERSION = "v4.3.0";
```

---

## 3. Inicializar no setup()

```cpp
void setup() {
    Serial.begin(115200);
    
    // Conectar WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi conectado");
    
    // Configurar MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(2048);  // Importante para payloads OTA
    
    // Inicializar OTA Manager
    otaManager = new OTAManager(&mqttClient, TOTEM_ID, FIRMWARE_VERSION);
    otaManager->begin();
    
    // Verificar se boot é válido após OTA
    otaManager->verificarBootValido();
    
    // Conectar MQTT
    conectarMQTT();
}
```

---

## 4. Callback MQTT

```cpp
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";
    
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    
    Serial.println("📩 MQTT recebido: " + topicStr);
    
    // Verificar se é mensagem OTA
    String otaTopic = "totem/" + String(TOTEM_ID) + "/ota";
    if (topicStr == otaTopic) {
        Serial.println("🚀 Mensagem OTA recebida!");
        otaManager->handleOTAMessage(payloadStr);
        return;
    }
    
    // Outros tópicos (trigger, audioUpdate, etc.)
    if (topicStr.endsWith("/trigger")) {
        // Seu código de trigger
    }
    else if (topicStr.endsWith("/audioUpdate")) {
        // Seu código de audio update
    }
}
```

---

## 5. Loop Principal

```cpp
void loop() {
    // Manter conexão MQTT
    if (!mqttClient.connected()) {
        conectarMQTT();
    }
    mqttClient.loop();
    
    // Verificar se OTA está em andamento
    if (otaManager->isOTAEmAndamento()) {
        // Não executar outras tarefas durante OTA
        delay(100);
        return;
    }
    
    // Seu código normal aqui
    // ...
}
```

---

## 6. Conectar MQTT com Inscrição OTA

```cpp
void conectarMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Conectando MQTT...");
        
        String clientId = "ESP32_" + String(TOTEM_ID);
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println(" ✅ Conectado!");
            
            // Inscrever em tópicos
            String triggerTopic = "totem/" + String(TOTEM_ID) + "/trigger";
            String audioTopic = "totem/" + String(TOTEM_ID) + "/audioUpdate";
            String otaTopic = "totem/" + String(TOTEM_ID) + "/ota";
            
            mqttClient.subscribe(triggerTopic.c_str());
            mqttClient.subscribe(audioTopic.c_str());
            mqttClient.subscribe(otaTopic.c_str());  // ← IMPORTANTE!
            
            Serial.println("✅ Inscrito em tópicos OTA");
        } else {
            Serial.print(" ❌ Falhou, rc=");
            Serial.println(mqttClient.state());
            delay(5000);
        }
    }
}
```

---

## 7. Configuração do platformio.ini

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; Configurações OTA
board_build.partitions = partitions.csv

; Bibliotecas necessárias
lib_deps = 
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^6.21.3

; Flags de compilação
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_CDC_ON_BOOT=1

; Monitor
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
```

---

## 8. Arquivo partitions.csv

Crie o arquivo `partitions.csv` na raiz do projeto:

```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
spiffs,   data, spiffs,  0x3D0000,0x30000,
```

**Explicação**:
- `app0`: Partição principal (1.875 MB)
- `app1`: Partição OTA (1.875 MB)
- `otadata`: Controle de qual partição usar
- `nvs`: Armazenamento não-volátil
- `spiffs`: Sistema de arquivos (opcional)

---

## 9. Exemplo Completo (main.cpp)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "OTAManager.h"

// Configurações WiFi
const char* WIFI_SSID = "SuaRedeWiFi";
const char* WIFI_PASSWORD = "SuaSenha";

// Configurações MQTT
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// Configurações Totem
const char* TOTEM_ID = "totem123";
const char* FIRMWARE_VERSION = "v4.3.0";

// Clientes
WiFiClient espClient;
PubSubClient mqttClient(espClient);
OTAManager* otaManager = nullptr;

void mqttCallback(char* topic, byte* payload, unsigned int length);
void conectarMQTT();

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=================================");
    Serial.println("🚀 TOTEM ESP32-S3 + OTA");
    Serial.println("=================================");
    Serial.printf("Versão: %s\n", FIRMWARE_VERSION);
    Serial.printf("Totem ID: %s\n", TOTEM_ID);
    
    // WiFi
    Serial.print("Conectando WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi conectado: " + WiFi.localIP().toString());
    
    // MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(2048);
    
    // OTA Manager
    otaManager = new OTAManager(&mqttClient, TOTEM_ID, FIRMWARE_VERSION);
    otaManager->begin();
    otaManager->verificarBootValido();
    
    // Conectar MQTT
    conectarMQTT();
    
    Serial.println("=================================");
    Serial.println("✅ Sistema inicializado!");
    Serial.println("=================================\n");
}

void loop() {
    // MQTT
    if (!mqttClient.connected()) {
        conectarMQTT();
    }
    mqttClient.loop();
    
    // Pausar outras tarefas durante OTA
    if (otaManager->isOTAEmAndamento()) {
        delay(100);
        return;
    }
    
    // Seu código aqui
    // ...
    
    delay(10);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    
    Serial.println("📩 MQTT: " + topicStr);
    
    // OTA
    if (topicStr == "totem/" + String(TOTEM_ID) + "/ota") {
        otaManager->handleOTAMessage(payloadStr);
        return;
    }
    
    // Trigger
    if (topicStr.endsWith("/trigger")) {
        Serial.println("🎵 Trigger recebido!");
        // Seu código de trigger
    }
    
    // Audio Update
    if (topicStr.endsWith("/audioUpdate")) {
        Serial.println("🔄 Audio update recebido!");
        // Seu código de audio update
    }
}

void conectarMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Conectando MQTT...");
        
        String clientId = "ESP32_" + String(TOTEM_ID) + "_" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println(" ✅");
            
            // Inscrever tópicos
            mqttClient.subscribe(("totem/" + String(TOTEM_ID) + "/trigger").c_str());
            mqttClient.subscribe(("totem/" + String(TOTEM_ID) + "/audioUpdate").c_str());
            mqttClient.subscribe(("totem/" + String(TOTEM_ID) + "/ota").c_str());
            
            Serial.println("✅ Inscrito em tópicos");
        } else {
            Serial.printf(" ❌ Falhou (rc=%d)\n", mqttClient.state());
            delay(5000);
        }
    }
}
```

---

## 10. Logs Esperados

### Boot Normal
```
=================================
🚀 TOTEM ESP32-S3 + OTA
=================================
Versão: v4.3.0
Totem ID: totem123
Conectando WiFi.....
✅ WiFi conectado: 192.168.1.100
OTA Manager inicializado
Versão atual: v4.3.0
Partição em execução: app0
Conectando MQTT... ✅
✅ Inscrito em tópicos
=================================
✅ Sistema inicializado!
=================================
```

### Recebendo OTA
```
📩 MQTT: totem/totem123/ota
=== NOVA ATUALIZAÇÃO OTA ===
Versão: v4.3.1
URL: https://storage.googleapis.com/...
Checksum: abc123def456
Tamanho: 1024000 bytes
Aguardando 2 segundos antes de iniciar...
Iniciando download do firmware...
Progresso: 10%
Progresso: 20%
Progresso: 30%
...
Progresso: 100%
Download concluído com sucesso!
=== OTA CONCLUÍDO ===
Reiniciando em 3 segundos...
```

### Após Reboot (Validação)
```
Primeira inicialização após OTA - Validando...
OTA validado com sucesso!
```

---

## 11. Troubleshooting Firmware

### Problema: ESP32 não recebe mensagem OTA

**Verificar**:
```cpp
// No callback, adicionar log
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("DEBUG: Topic=%s, Length=%d\n", topic, length);
    // ...
}
```

**Solução**: Verificar se está inscrito no tópico correto

### Problema: Download falha

**Verificar**:
- Conexão WiFi estável
- URL HTTPS válida
- Certificado SSL (se necessário)

**Solução**:
```cpp
// Desabilitar verificação SSL temporariamente (apenas para debug)
WiFiClientSecure client;
client.setInsecure();
```

### Problema: OTA completa mas não reinicia

**Verificar**:
```cpp
if (Update.end(true)) {
    Serial.println("OTA OK - Reiniciando...");
    delay(3000);
    ESP.restart();  // ← Verificar se chega aqui
}
```

---

## 12. Boas Práticas

1. **Sempre testar em ambiente de desenvolvimento primeiro**
2. **Manter versão no código sincronizada com nome do arquivo**
3. **Adicionar watchdog durante OTA**:
   ```cpp
   esp_task_wdt_init(30, true);  // 30 segundos
   ```
4. **Publicar status para o servidor**:
   ```cpp
   publicarStatus("downloading", "50%");
   ```
5. **Validar checksum antes de aplicar**
6. **Manter logs detalhados**

---

**Integração completa! 🚀**

Para mais informações, consulte `OTA-GUIDE.md` no servidor.
