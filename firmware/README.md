# 🤖 Firmware ESP32 — Totem Interativo IoT

Firmware modular para ESP32 com:

- WiFi (STA/AP + reset via botão)
- MQTT (topics obrigatórios)
- Máquina de estados
- LEDs não-bloqueantes (FastLED)
- Botões TTP223 (debounce + clique longo)
- Download seguro de áudio (temp -> rename)
- OTA por URL (Update)

## 📂 Estrutura

- `include/Config.h`
  - Configurações do totem, pinagem, topics MQTT.
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

## ▶️ Build/Upload (PlatformIO)

Dentro de `firmware/`:

```bash
pio run
pio run -t upload
pio device monitor
```

## 🔊 Áudio

Este projeto usa a biblioteca `ESP32-audioI2S`.

O download segue o fluxo seguro:

- baixa para `AUDIO_TEMP_FILENAME` (`/audio.tmp`)
- valida tamanho e download completo
- renomeia para `AUDIO_FILENAME` (`/audio.mp3`)

Assim você não corrompe o arquivo ativo em caso de falha.
