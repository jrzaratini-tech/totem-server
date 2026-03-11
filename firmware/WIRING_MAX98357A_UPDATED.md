# MAX98357A I2S DAC - Configuração de Hardware

## Conexões Físicas (ESP32-S3 → MAX98357A)

```
ESP32-S3        MAX98357A
━━━━━━━━        ━━━━━━━━━
GPIO6    →      BCLK        (Bit Clock)
GPIO7    →      LRC         (Left/Right Clock / Word Select)
GPIO5    →      DIN         (Data In)
GND      →      GAIN        (9dB ganho fixo)
5V       →      VIN         (Alimentação)
GND      →      GND         (Terra)
```

## Configuração de Ganho do MAX98357A

O MAX98357A possui 3 níveis de ganho configuráveis via pino GAIN:

| Conexão GAIN | Ganho | Uso Recomendado |
|--------------|-------|-----------------|
| **GND** | **9 dB** | **Uso geral (configuração atual)** |
| VDD | 15 dB | Ambientes ruidosos / alto-falantes de baixa sensibilidade |
| Floating (desconectado) | 12 dB | Intermediário |

### ⚠️ Importante
- **GAIN está conectado ao GND = 9dB fixo**
- Não é controlável por software
- Para mudar o ganho, é necessário alterar a conexão física do pino GAIN

## Especificações I2S

| Parâmetro | Valor |
|-----------|-------|
| Sample Rate | 44.1 kHz |
| Bit Depth | 16-bit |
| Canais | 2 (Stereo) |
| Formato | I2S Philips |
| BCLK Frequency | ~1.4 MHz (44.1kHz × 32 bits) |

## Conexão do Alto-Falante

```
MAX98357A        Alto-Falante
━━━━━━━━━        ━━━━━━━━━━━━
OUT+      →      Terminal +
OUT-      →      Terminal -
```

**Impedância recomendada:** 4Ω ou 8Ω  
**Potência máxima:** 3.2W @ 5V, 4Ω

## Sinais I2S Esperados (Osciloscópio)

Durante reprodução de áudio:

### BCLK (GPIO6)
- **Forma de onda:** Clock quadrado
- **Frequência:** ~1.4 MHz
- **Amplitude:** 0V a 3.3V
- **Duty cycle:** 50%

### LRC (GPIO7)
- **Forma de onda:** Clock quadrado
- **Frequência:** 44.1 kHz
- **Amplitude:** 0V a 3.3V
- **Duty cycle:** 50%

### DIN (GPIO5)
- **Forma de onda:** Dados digitais variáveis
- **Amplitude:** 0V a 3.3V
- **Comportamento:** Varia durante reprodução, não deve ficar constante

## Troubleshooting

### Problema: Sem áudio

**Verificar:**
1. ✓ Alimentação 5V presente no VIN
2. ✓ GND conectado
3. ✓ Alto-falante conectado corretamente (4-8Ω)
4. ✓ BCLK, LRC, DIN conectados aos pinos corretos
5. ✓ GAIN conectado ao GND

### Problema: Volume muito baixo

**Causa:** GAIN no GND = 9dB (ganho médio)

**Soluções:**
1. Conectar GAIN ao VDD para 15dB (ganho máximo)
2. Aumentar volume no software (0-10)
3. Ajustar equalizer (bass, mid, treble)

### Problema: Distorção

**Causas possíveis:**
1. Alto-falante com impedância muito baixa (< 4Ω)
2. Volume muito alto
3. Equalizer com ganhos excessivos

**Soluções:**
1. Usar alto-falante de 4Ω ou 8Ω
2. Reduzir volume
3. Reduzir ganhos do equalizer
4. Habilitar clipping protection

### Problema: Ruído/chiado

**Verificar:**
1. Conexões firmes (sem mau contato)
2. Cabos curtos (< 20cm para sinais I2S)
3. Alimentação estável (5V sem ripple)
4. GND comum entre ESP32 e MAX98357A
5. Arquivo MP3 não corrompido

## Código de Configuração

### Config.h
```cpp
#define I2S_BCLK                6       // ESP32-S3 → MAX98357A BCLK
#define I2S_LRC                 7       // ESP32-S3 → MAX98357A LRC
#define I2S_DOUT                5       // ESP32-S3 → MAX98357A DIN
// GAIN conectado ao GND = 9dB fixo

#define AUDIO_SAMPLE_RATE       44100
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_CHANNELS          2
```

### Inicialização
```cpp
audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
audio.setVolume(21);  // Volume máximo (0-21)
```

## Logs Esperados

```
[Audio] ========================================
[Audio] INITIALIZING AUDIO SYSTEM
[Audio] ========================================
[Audio] ✓ SPIFFS initialized
[Audio] ✓ I2S pins configured
[Audio] ===== I2S CONFIGURATION =====
[Audio] DAC: MAX98357A
[Audio] BCLK (Bit Clock):   GPIO6 → MAX98357A BCLK
[Audio] LRC (Word Select):  GPIO7 → MAX98357A LRC
[Audio] DOUT (Data Out):    GPIO5 → MAX98357A DIN
[Audio] GAIN:               GND (9dB fixed)
[Audio] Sample Rate:        44100 Hz
[Audio] Bit Depth:          16-bit
[Audio] Channels:           2 (Stereo)
[Audio] DMA Buffers:        16 x 1024 bytes
[Audio] ====================================
[Audio] ✓ AUDIO SYSTEM READY
[Audio] ========================================
```

## Datasheet e Referências

- [MAX98357A Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
- [ESP32 I2S Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html)

---

**Versão:** 4.1.0  
**Data:** Março 2026  
**Hardware:** ESP32-S3 + MAX98357A (GAIN @ GND = 9dB)
