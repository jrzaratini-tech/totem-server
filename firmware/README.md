# 🤖 Firmware ESP32 — Totem Interativo IoT

Firmware modular para ESP32-S3 com:

- WiFi (STA/AP + reset via botão)
- MQTT (topics obrigatórios)
- Máquina de estados
- LEDs não-bloqueantes (FastLED)
- Botões TTP223 (debounce + clique longo)
- Download seguro de áudio (temp → rename)
- OTA por URL (Update)
- Provisionamento (Totem ID + token) via Portal AP/NVS
- Watchdog + modo seguro (failsafe)
- **Áudio I2S com MAX98357A** (GPIO 6, 7, 5)
- **Equalização de áudio em tempo real**

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

### Plataforma
Este projeto usa **Pioarduino** (Arduino Core 3.x) para suporte completo ao ESP32-S3:

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.07/platform-espressif32.zip
```

### Comandos
Dentro de `firmware/`:

```bash
# Compilar
pio run

# Upload
pio run -t upload

# Monitor serial
pio device monitor
```

### ⚠️ Importante - Biblioteca ESP32-audioI2S
A biblioteca ESP32-audioI2S requer um patch para funcionar com Arduino Core 3.x:

```bash
# Após o primeiro build, aplicar patch
python patch_library.py
```

Este patch adiciona suporte a `std::span` necessário pela biblioteca.

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

## 🔊 Sistema de Áudio

### Hardware
- **Amplificador**: MAX98357A (I2S)
- **Pinos I2S**:
  - `GPIO 6` → BCLK (Bit Clock)
  - `GPIO 7` → LRC (Word Select)
  - `GPIO 5` → DIN (Data In)
  - `GPIO 4` → GAIN (Controle de ganho)

### Biblioteca
Este projeto usa a biblioteca **ESP32-audioI2S** (schreibfaul1).

**Status**: ⚠️ A versão mais recente tem incompatibilidades com Arduino Core 3.x. Consulte `STATUS_ESP32_AUDIOI2S.md` para detalhes.

### Download Seguro de Áudio
O download segue o fluxo seguro:

1. Baixa para `AUDIO_TEMP_FILENAME` (`/audio.tmp`)
2. Valida tamanho e download completo
3. Renomeia para `AUDIO_FILENAME` (`/audio.mp3`)

Assim você não corrompe o arquivo ativo em caso de falha.

### Equalização
O firmware inclui equalização de áudio em tempo real com:
- Controle de graves, médios e agudos
- Limiter/compressor para proteção contra clipping
- Curvas de volume (linear, exponencial suave, exponencial agressiva)
- Perfis de áudio salvos em Preferences

## 🔐 HTTPS

Para cumprir o requisito de HTTPS:

- Defina `ROOT_CA_PEM` em `include/Config.h`
- Sem CA, as rotinas de download (áudio/OTA) falham por segurança

## 📚 Documentação Adicional

- **`docs/STATUS_ESP32_AUDIOI2S.md`** - Status da migração para ESP32-audioI2S
- **`docs/MIGRATION_ESP32_AUDIOI2S.md`** - Guia de migração detalhado
- **`docs/AUDITORIA_MIGRACAO_ESP32_AUDIOI2S.md`** - Auditoria completa da migração
- **`docs/ESP32-S3_PIN_MAPPING.md`** - Mapeamento de pinos do ESP32-S3
- **`docs/WIRING_MAX98357A.md`** - Esquema de ligação do amplificador

## 🔧 Troubleshooting

### Erro de compilação com ESP32-audioI2S
Se encontrar erros relacionados a `std::span` ou `i2s_chan_config_t`:

1. Execute o patch: `python patch_library.py`
2. Consulte `STATUS_ESP32_AUDIOI2S.md` para status atual
3. Considere usar AudioTools como alternativa temporária

### Problemas com áudio
- Verifique as conexões do MAX98357A
- Confirme que GPIO 4 (GAIN) está em HIGH
- Verifique logs serial: `[Audio]` tags

### WiFi não conecta
- Aguarde o AP `Totem-Config-<id>` aparecer
- Configure SSID/senha e Totem ID
- Dados salvos em NVS (persistem após reset)
