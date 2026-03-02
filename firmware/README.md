# 🤖 Firmware ESP32 — Totem Interativo IoT

Firmware modular para ESP32 com:

- WiFi (STA/AP + reset via botão)
- MQTT (topics obrigatórios)
- Máquina de estados
- LEDs não-bloqueantes (FastLED)
- Botões TTP223 (debounce + clique longo)
- Download seguro de áudio (temp -> rename)
- OTA por URL (Update)
- Provisionamento (Totem ID + token) via Portal AP/NVS
- Watchdog + modo seguro (failsafe)

## 📂 Estrutura

- `include/Config.h`
  - Configurações do hardware, rede e segurança.
  - **HTTPS exige `ROOT_CA_PEM` configurado**.
- `src/main.cpp`
  - Integração e loop principal.
- `src/core/*`
  - Módulos principais.
- `src/effects/*`
  - Efeitos de LED.

## 📡 MQTT Topics

- `totem/{id}/trigger`
- `totem/{id}/configUpdate`
- `totem/{id}/audioUpdate`
- `totem/{id}/firmwareUpdate`
- `totem/{id}/status`

> Observação: os tópicos são montados em runtime a partir do `Totem ID` provisionado (NVS).

## 🆔 Provisionamento (Totem ID / Token)

- Se o ESP32 não conseguir conectar no WiFi salvo, ele sobe um AP:
  - `Totem-Config-<id>`
- Na página de configuração você define:
  - SSID e senha
  - `Totem ID`
  - `Token` (opcional)

O `Totem ID` e o `Token` ficam armazenados em NVS e não exigem recompilar.

## ▶️ Build/Upload (PlatformIO)

Dentro de `firmware/`:

```bash
pio run
pio run -t upload
pio device monitor
```

## ▶️ Build/Upload (Arduino IDE)

Se você só usa **Arduino IDE**, você pode compilar criando um sketch que inclua o `src/main.cpp`.

- Crie uma pasta `TotemFirmware/`
- Dentro crie `TotemFirmware.ino` com:

```cpp
#include "src/main.cpp"
```

- Copie as pastas `src/` e `include/` para dentro dessa pasta do sketch.

Bibliotecas típicas necessárias no Arduino IDE:

- `ArduinoJson`
- `PubSubClient`
- `FastLED`
- `ESP Async WebServer`
- `AsyncTCP`
- `ESP32-audioI2S`

## 🔊 Áudio

Este projeto usa a biblioteca `ESP32-audioI2S`.

O download segue o fluxo seguro:

- baixa para `AUDIO_TEMP_FILENAME` (`/audio.tmp`)
- valida tamanho e download completo
- renomeia para `AUDIO_FILENAME` (`/audio.mp3`)

Assim você não corrompe o arquivo ativo em caso de falha.

## 🔐 HTTPS

Para cumprir o requisito de HTTPS:

- Defina `ROOT_CA_PEM` em `include/Config.h`
- Sem CA, as rotinas de download (áudio/OTA) falham por segurança
