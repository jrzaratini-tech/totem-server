# ESP32-S3 WROOM Freenove - Pin Mapping

## Adaptação do Projeto para ESP32-S3

Este documento descreve o mapeamento de pinos adaptado do ESP32 original para o ESP32-S3 WROOM Freenove.

## Mudanças Principais

### 1. Board Configuration (platformio.ini)
- **Board**: `esp32-s3-devkitc-1`
- **MCU**: ESP32-S3
- **CPU Frequency**: 240MHz
- **Flash Mode**: QIO
- **PSRAM**: OPI (Octal)
- **USB CDC**: Habilitado

### 2. Mapeamento de Pinos

#### LEDs (WS2812B)
| Função | ESP32 Original | ESP32-S3 | Notas |
|--------|---------------|----------|-------|
| LED Principal | GPIO19 | **GPIO1** | Fita LED principal (199 LEDs) |
| LED Coração | GPIO18 | **GPIO9** | Fita LED coração (9 LEDs) |
| Heartbeat LED 1 | GPIO15 | **GPIO1** | LED de status |
| Heartbeat LED 2 | GPIO18 | **GPIO2** | LED de status |

#### Botões
| Função | ESP32 Original | ESP32-S3 | Tipo |
|--------|---------------|----------|------|
| Trigger/Coração | GPIO15 | **GPIO10** | TTP223 Capacitivo |
| Reset WiFi | GPIO22 | **GPIO11** | Botão Mecânico |
| Cor | GPIO15 | **GPIO10** | (Legacy) |
| Mais | GPIO4 | **GPIO12** | (Legacy) |
| Menos | GPIO16 | **GPIO13** | (Legacy) |

#### Áudio I2S
| Função | ESP32 Original | ESP32-S3 | Notas |
|--------|---------------|----------|-------|
| BCLK (Bit Clock) | GPIO27 | **GPIO6** | Clock de bit I2S |
| LRC (Word Select) | GPIO25 | **GPIO7** | Word select / LRCLK |
| DOUT (Data Out) | GPIO26 | **GPIO5** | Saída de dados |
| GAIN (Controle) | GPIO33 | **GPIO4** | Controle de ganho do amplificador |

#### SD Card (Opcional)
| Função | ESP32 Original | ESP32-S3 | Notas |
|--------|---------------|----------|-------|
| CS (Chip Select) | GPIO5 | **GPIO14** | SPI Chip Select |
| MOSI (Master Out) | GPIO23 | **GPIO15** | SPI MOSI |
| MISO (Master In) | GPIO19 | **GPIO16** | SPI MISO |
| SCK (Clock) | GPIO18 | **GPIO17** | SPI Clock |

## Restrições de GPIO do ESP32-S3

### GPIOs Seguros (0-18)
- **GPIO0-18**: Podem ser usados para propósitos gerais
- Evitados: GPIO19-20 (USB D-/D+)

### GPIOs Restritos
- **GPIO19-20**: Reservados para USB (não usar)
- **GPIO26-32**: Não existem no ESP32-S3
- **GPIO33-37**: Usados para PSRAM/Flash (não usar)

### GPIOs Strapping (cuidado ao usar)
- **GPIO0**: Boot mode (usado, mas OK)
- **GPIO3**: JTAG (pode ser usado)
- **GPIO45**: VDD_SPI (não usar)
- **GPIO46**: Boot mode (não usar)

## Memória e Partições

### Flash: 8MB
- **NVS**: 24KB (0x9000 - 0xF000)
- **OTA Data**: 8KB (0xF000 - 0x11000)
- **App0**: 3MB (0x10000 - 0x310000)
- **App1**: 3MB (0x310000 - 0x610000)
- **SPIFFS**: ~2MB (0x610000 - 0x800000)

### Melhorias de Memória
- **RAM**: 512KB (vs 520KB no ESP32 original)
- **PSRAM**: 8MB disponível (OPI mode)
- **Flash**: 8MB (vs 4MB típico no ESP32)

## Configurações de Build

### Flags Adicionais
```ini
-DARDUINO_USB_CDC_ON_BOOT=1  # Habilita USB CDC para Serial
-DBOARD_HAS_PSRAM            # Habilita suporte PSRAM
```

## Verificação Pós-Migração

### Checklist
- [ ] Compilação sem erros
- [ ] Upload via USB funcional
- [ ] LEDs WS2812B funcionando (GPIO1 e GPIO9)
- [ ] Botão trigger respondendo (GPIO10)
- [ ] Áudio I2S funcionando (GPIO4-7)
- [ ] WiFi conectando
- [ ] MQTT funcionando
- [ ] OTA updates funcionando
- [ ] SPIFFS montando corretamente

### Comandos de Teste
```bash
# Compilar
pio run -e esp32-s3-devkitc-1

# Upload
pio run -e esp32-s3-devkitc-1 -t upload

# Monitor serial
pio device monitor -b 115200
```

## Notas Importantes

1. **USB CDC**: O ESP32-S3 usa USB nativo. Certifique-se de que `ARDUINO_USB_CDC_ON_BOOT=1` está definido.

2. **PSRAM**: Habilitado por padrão. Útil para buffers de áudio maiores.

3. **Pinos I2S**: Os pinos I2S no ESP32-S3 são mais flexíveis. Qualquer GPIO pode ser usado, mas escolhemos GPIO4-7 por serem seguros e próximos.

4. **FastLED**: Compatível com ESP32-S3. Nenhuma mudança necessária no código.

5. **WiFi**: Totalmente compatível. Mesma API do ESP32.

## Troubleshooting

### Problema: Device não aparece na porta serial
- **Solução**: Pressione e segure BOOT, pressione RESET, solte RESET, solte BOOT

### Problema: Upload falha
- **Solução**: Use `pio run -t erase` e tente novamente

### Problema: LEDs não acendem
- **Solução**: Verifique conexões nos GPIO1 e GPIO9

### Problema: Áudio não funciona
- **Solução**: Verifique conexões I2S nos GPIO4-7 e o pino GAIN

## Referências

- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [Freenove ESP32-S3 WROOM](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board)
