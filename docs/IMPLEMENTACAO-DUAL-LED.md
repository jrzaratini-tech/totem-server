# Implementação do Sistema Dual de Iluminação (Idle/Trigger) + Volume

## ✅ Mudanças Implementadas

### 1. Backend (Node.js/Express) ✅

**Arquivo modificado:** `server/server.js`

#### Rota `/cliente/config/:id` atualizada:
- Agora aceita configurações duais: `idle`, `trigger` e `volume`
- Mantém compatibilidade retroativa com formato antigo
- Publica em 3 tópicos MQTT separados (retained):
  - `totem/{id}/config/idle` - Configuração do modo idle
  - `totem/{id}/config/trigger` - Configuração do modo trigger
  - `totem/{id}/config/volume` - Volume (0-10)

#### Modelo de dados Firestore:
```javascript
{
  idleConfig: {
    mode: "BREATH",
    color: "#FF3366",
    speed: 50,
    maxBrightness: 120,
    updatedAt: "2026-03-04T10:30:00.000Z"
  },
  triggerConfig: {
    mode: "RAINBOW",
    color: "#00FF00",
    speed: 70,
    duration: 30,
    maxBrightness: 150,
    updatedAt: "2026-03-04T10:30:00.000Z"
  },
  volume: 8  // 0 a 10
}
```

### 2. Frontend (Dashboard do Cliente) ✅

**Arquivo criado:** `server/views/cliente-dashboard.html` (substituiu o antigo)
**Backup:** `server/views/cliente-dashboard-old.html`

#### Novas funcionalidades:
- **Duas seções de LED independentes:**
  - **Iluminação em espera (idle):** Modo contínuo quando o totem não está acionado
  - **Iluminação ao disparar (trigger):** Executado junto com o áudio
  
- **Controle de volume:** Slider de 0 (mudo) a 10 (máximo)

- **Controles para cada modo:**
  - Efeito (BREATH, SOLID, RAINBOW, BLINK, RUNNING, HEART)
  - Cor (seletor de cor)
  - Brilho (0-180)
  - Velocidade (1-100)
  - Duração (apenas para trigger, em segundos)

- **Simulador visual:** Prévia em tempo real do modo idle

- **Envio unificado:** Botão "Aplicar configurações" envia idle + trigger + volume em uma única requisição

### 3. Firmware ESP32 (Parcialmente implementado) ⚠️

**Arquivo modificado:** `firmware/include/Config.h`

#### Mudanças no Config.h:
- `NUM_LEDS_MAIN`: 60 → **220** (fita principal)
- `NUM_LEDS_HEART`: 16 → **15** (coração)
- `LED_MAIN_PIN`: 12 → **19** (GPIO19)
- `LED_HEART_PIN`: 13 → **18** (GPIO18)
- `PIN_BTN_CORACAO`: 17 → **5** (botão de disparo)
- `PIN_BTN_RESET_WIFI`: 5 → **22** (botão reset Wi-Fi)
- `LONG_PRESS_TIME`: 1000 → **5000** (5 segundos para reset Wi-Fi)
- Adicionado: `MIN_VOLUME` e `MAX_VOLUME` para controle de volume

---

## 🔧 Próximas Etapas Necessárias

### 1. Firmware ESP32 - Implementação Completa

#### A. Atualizar `main.cpp` para suportar modos idle/trigger:

**Variáveis globais a adicionar:**
```cpp
static ConfigData idleConfig;
static ConfigData triggerConfig;
static int currentVolume = DEFAULT_VOLUME;
static bool isIdle = true;
```

**Modificar `setupMQTTCallbacks()` para assinar novos tópicos:**
```cpp
// Assinar tópicos de configuração
mqttManager.subscribe("totem/" + gTotemId + "/config/idle");
mqttManager.subscribe("totem/" + gTotemId + "/config/trigger");
mqttManager.subscribe("totem/" + gTotemId + "/config/volume");
```

**Adicionar handlers para novos tópicos:**
```cpp
if (topic.endsWith("/config/idle")) {
    // Parse JSON e atualiza idleConfig
    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);
    idleConfig.mode = parseMode(doc["mode"]);
    idleConfig.color = parseColor(doc["color"]);
    idleConfig.speed = doc["speed"] | 50;
    idleConfig.maxBrightness = doc["maxBrightness"] | 120;
    
    // Se estiver em idle, aplica imediatamente
    if (isIdle) {
        ledEngine.startEffect(idleConfig);
    }
    return;
}

if (topic.endsWith("/config/trigger")) {
    // Parse JSON e atualiza triggerConfig
    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);
    triggerConfig.mode = parseMode(doc["mode"]);
    triggerConfig.color = parseColor(doc["color"]);
    triggerConfig.speed = doc["speed"] | 70;
    triggerConfig.duration = doc["duration"] | 30;
    triggerConfig.maxBrightness = doc["maxBrightness"] | 150;
    return;
}

if (topic.endsWith("/config/volume")) {
    // Atualiza volume (0-10 do servidor → 0-21 do ESP32)
    int vol = payload.toInt();
    currentVolume = map(constrain(vol, 0, 10), 0, 10, MIN_VOLUME, MAX_VOLUME);
    audioManager.setVolume(currentVolume);
    return;
}
```

**Modificar handler de trigger para usar triggerConfig:**
```cpp
if (topic.endsWith("/trigger")) {
    if (payload == "play") {
        if (!stateMachine.canPlay()) return;
        if (!stateMachine.setState(PLAYING)) return;
        
        isIdle = false;
        
        // Usa triggerConfig ao invés de configManager
        ledEngine.startEffect(triggerConfig);
        audioManager.play();
        playEndMs = millis() + (unsigned long)triggerConfig.duration * 1000UL;
    }
    return;
}
```

**Modificar loop() para retornar ao idle após trigger:**
```cpp
if (stateMachine.getState() == PLAYING) {
    if ((playEndMs != 0 && millis() > playEndMs) || !audioManager.isPlaying()) {
        ledEngine.stop();
        audioManager.stop();
        playEndMs = 0;
        
        // Retorna ao modo idle
        isIdle = true;
        ledEngine.startEffect(idleConfig);
        
        stateMachine.setState(IDLE);
    }
}
```

**Inicializar idle no setup():**
```cpp
void setup() {
    // ... código existente ...
    
    // Inicializa configurações padrão
    idleConfig.mode = BREATH;
    idleConfig.color = 0xFF3366;
    idleConfig.speed = 50;
    idleConfig.maxBrightness = 120;
    idleConfig.duration = 0; // Idle não tem duração
    
    triggerConfig.mode = RAINBOW;
    triggerConfig.color = 0x00FF00;
    triggerConfig.speed = 70;
    triggerConfig.duration = 30;
    triggerConfig.maxBrightness = 150;
    
    // Inicia em modo idle
    isIdle = true;
    ledEngine.startEffect(idleConfig);
    
    stateMachine.setState(IDLE);
}
```

#### B. Implementar LEDs do coração independente

**Criar task FreeRTOS para coração pulsante:**
```cpp
void heartTask(void* parameter) {
    CRGB* heartLeds = (CRGB*)parameter;
    
    while (true) {
        // Efeito vermelho pulsante fixo
        unsigned long t = millis();
        uint8_t brightness = beatsin8(60, 30, 180, 0, 0); // 60 BPM
        
        for (int i = 0; i < NUM_LEDS_HEART; i++) {
            heartLeds[i] = CRGB::Red;
        }
        
        FastLED[1].showLeds(brightness); // Assume que heart é o segundo controlador
        
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

// No setup(), criar a task:
xTaskCreatePinnedToCore(
    heartTask,
    "HeartTask",
    2048,
    ledEngine.getHeartLeds(), // Precisa adicionar getter
    1,
    NULL,
    0 // Core 0
);
```

#### C. Implementar botão de reset Wi-Fi

**Modificar `setupButtonCallbacks()` no main.cpp:**
```cpp
buttonManager.onButtonResetWifi([](bool longPress) {
    if (longPress) {
        Serial.println("[MAIN] Reset Wi-Fi solicitado - apagando credenciais");
        wifiManager.resetWiFiSettings();
        ESP.restart();
    }
});
```

**Adicionar método no ButtonManager.cpp:**
```cpp
void ButtonManager::onButtonResetWifi(ButtonCallback callback) {
    callbackResetWifi = callback;
}

// No loop(), adicionar leitura do botão:
void ButtonManager::loop() {
    // ... código existente ...
    
    // Botão Reset Wi-Fi
    int readingReset = digitalRead(PIN_BTN_RESET_WIFI);
    if (readingReset != lastStateResetWifi) {
        lastDebounceResetWifi = millis();
    }
    
    if ((millis() - lastDebounceResetWifi) > DEBOUNCE_DELAY) {
        if (readingReset != stateResetWifi) {
            stateResetWifi = readingReset;
            
            if (stateResetWifi == LOW) {
                pressStartResetWifi = millis();
            } else {
                unsigned long pressDuration = millis() - pressStartResetWifi;
                if (pressDuration >= LONG_PRESS_TIME && callbackResetWifi) {
                    callbackResetWifi(true);
                }
            }
        }
    }
    
    lastStateResetWifi = readingReset;
}
```

### 2. Wi-Fi Manager em Português

**Opção 1: Usar WiFiManager com customização**
```cpp
#include <WiFiManager.h>

void setupWiFiPortal() {
    WiFiManager wm;
    
    // Textos em português
    wm.setTitle("Print Pixel");
    wm.setConfigPortalTimeout(180);
    
    // Customizar strings
    const char* menu[] = {"wifi", "info", "exit"};
    wm.setMenu(menu, 3);
    
    // Portal AP
    String apName = "Totem-" + gTotemId;
    wm.autoConnect(apName.c_str());
}
```

**Opção 2: Portal customizado com ESPAsyncWebServer**
```cpp
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

const char* portalHTML = R"(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <title>Print Pixel - Configurar Wi-Fi</title>
    <style>
        body { font-family: Arial; background: #0b0f18; color: white; padding: 20px; }
        h1 { color: #4f83f5; }
        input { width: 100%; padding: 10px; margin: 10px 0; border-radius: 5px; }
        button { background: #4f83f5; color: black; padding: 12px 24px; border: none; border-radius: 5px; cursor: pointer; }
    </style>
</head>
<body>
    <h1>🖨️ Print Pixel</h1>
    <h2>Configurar Wi-Fi</h2>
    <form action="/save" method="POST">
        <label>Nome da rede (SSID):</label>
        <input type="text" name="ssid" required>
        <label>Senha:</label>
        <input type="password" name="password">
        <button type="submit">Conectar</button>
    </form>
</body>
</html>
)";
```

### 3. Atualizar AudioManager para suportar volume

**Adicionar no AudioManager.h:**
```cpp
void setVolume(int volume); // 0-21
int getVolume();
```

**Implementar no AudioManager.cpp:**
```cpp
void AudioManager::setVolume(int volume) {
    volume = constrain(volume, MIN_VOLUME, MAX_VOLUME);
    if (audio) {
        audio->setVolume(volume);
    }
}

int AudioManager::getVolume() {
    return audio ? audio->getVolume() : DEFAULT_VOLUME;
}
```

### 4. Atualizar LEDEngine para suportar dois conjuntos independentes

**Modificar LEDEngine.h:**
```cpp
class LEDEngine {
public:
    void startIdleEffect(const ConfigData &config);
    void startTriggerEffect(const ConfigData &config);
    CRGB* getHeartLeds(); // Para a task do coração
    
private:
    ConfigData idleCfg;
    ConfigData triggerCfg;
    bool isIdleMode;
};
```

---

## 📋 Checklist de Implementação

### Backend ✅
- [x] Rota `/cliente/config/:id` atualizada para suportar idle/trigger/volume
- [x] Publicação MQTT em tópicos separados
- [x] Modelo de dados Firestore atualizado
- [x] Compatibilidade retroativa mantida

### Frontend ✅
- [x] Dashboard com duas seções de LED
- [x] Controle de volume (slider 0-10)
- [x] Simulador visual do modo idle
- [x] Envio unificado de configurações
- [x] Interface responsiva e moderna

### Firmware ESP32 ⚠️
- [x] Config.h atualizado com novos pinos e constantes
- [ ] main.cpp modificado para suportar idle/trigger
- [ ] Handlers MQTT para novos tópicos
- [ ] LEDs do coração independente (task FreeRTOS)
- [ ] Botão de reset Wi-Fi implementado
- [ ] Wi-Fi Manager em português
- [ ] Controle de volume no AudioManager
- [ ] LEDEngine com suporte a dois modos

### Documentação 📝
- [ ] README.md atualizado com novas funcionalidades
- [ ] Pinagem documentada
- [ ] Fluxos de operação atualizados
- [ ] Exemplos de uso

---

## 🎯 Resumo das Funcionalidades

### Para o Cliente:
1. **Dois modos de iluminação configuráveis:**
   - Modo espera (idle): Sempre ativo quando o totem não está em uso
   - Modo disparo (trigger): Executado junto com o áudio

2. **Controle de volume:** 0 (mudo) a 10 (máximo)

3. **Configuração completa de cada modo:**
   - 6 efeitos disponíveis
   - Cor personalizável
   - Brilho ajustável
   - Velocidade configurável
   - Duração (apenas para trigger)

### Para o Totem (ESP32):
1. **Operação dual:**
   - Idle contínuo quando não acionado
   - Trigger + áudio quando acionado

2. **Coração pulsante:**
   - 15 LEDs independentes
   - Vermelho pulsante fixo
   - Não configurável pelo cliente

3. **Botões físicos:**
   - Botão "Coração" (GPIO5): Dispara o efeito
   - Botão Reset Wi-Fi (GPIO22): Long press 5s para resetar

4. **Portal Wi-Fi em português:**
   - Interface "Print Pixel"
   - Configuração fácil de rede

---

## 🔌 Pinagem Atualizada

| Função | GPIO | Observação |
|--------|------|------------|
| Fita principal (220 LEDs) | GPIO19 | WS2812B |
| Coração (15 LEDs) | GPIO18 | WS2812B, vermelho pulsante |
| Botão disparo | GPIO5 | Pull-up interno |
| Botão reset Wi-Fi | GPIO22 | Long press 5s |
| I2S BCLK | GPIO27 | Áudio |
| I2S LRC | GPIO25 | Áudio |
| I2S DOUT | GPIO26 | Áudio |
| I2C SDA | GPIO33 | ES8388 codec |
| I2C SCL | GPIO32 | ES8388 codec |
| PA Enable | GPIO21 | Amplificador |

---

## 🚀 Como Testar

### 1. Backend:
```bash
cd totem-server
npm start
```

### 2. Acessar dashboard:
```
http://localhost:3000/cliente/login
```

### 3. Configurar:
- Definir modo idle (ex: BREATH azul)
- Definir modo trigger (ex: RAINBOW)
- Ajustar volume para 8
- Clicar em "Aplicar configurações"

### 4. Verificar MQTT:
Os seguintes tópicos devem receber mensagens (retained):
- `totem/{id}/config/idle`
- `totem/{id}/config/trigger`
- `totem/{id}/config/volume`

---

## 📝 Notas Importantes

1. **Compatibilidade:** O sistema mantém compatibilidade com o formato antigo (único config)

2. **MQTT Retained:** As configurações são publicadas com flag `retained` para que o ESP32 receba ao reconectar

3. **Sincronização:** Aguardar ~10 segundos após aplicar para o ESP32 receber via MQTT

4. **Volume:** Mapeamento 0-10 (servidor) → 0-21 (ESP32)

5. **Coração:** Sempre vermelho pulsante, independente das configurações do cliente

6. **Reset Wi-Fi:** Long press de 5 segundos no botão GPIO22

---

## 🐛 Troubleshooting

### Backend não publica MQTT:
- Verificar conexão com broker HiveMQ
- Checar logs do servidor

### Dashboard não carrega configurações:
- Verificar se Firebase está inicializado
- Checar documento no Firestore

### ESP32 não recebe configurações:
- Verificar conexão MQTT do ESP32
- Confirmar subscrição aos tópicos corretos
- Checar logs serial do ESP32

### LEDs não funcionam:
- Verificar pinagem (GPIO19 para main, GPIO18 para heart)
- Confirmar alimentação adequada (220 LEDs requerem fonte externa)
- Testar com poucos LEDs primeiro

---

**Versão:** 4.1.0  
**Data:** 04/03/2026  
**Status:** Backend e Frontend completos, Firmware parcialmente implementado
