# Correção de Áudio ES8388 - Firmware 4.1.0+

## Problema Identificado

O áudio estava sendo baixado com sucesso mas **não reproduzia no alto-falante**. Apenas ruído de fundo era audível, indicando que o amplificador estava ligado mas sem sinal de áudio. 

Após análise detalhada do caminho completo do áudio (servidor → SPIFFS → I2S → ES8388 → amplificador → alto-falante), foi identificado o problema crítico: **O codec ES8388 não estava roteando o sinal DAC para as saídas físicas LOUT**.

## Problemas Corrigidos

### 1. Registradores de Volume Incorretos

**ANTES (ERRADO):**
```cpp
static void setVolume(uint8_t volume) {
    writeReg(0x27, volume);  // ❌ 0x27 é mixer control, não volume
    writeReg(0x2A, volume);  // ❌ 0x2A é mixer control, não volume
}
```

**DEPOIS (CORRETO):**
```cpp
static void setVolume(uint8_t volume) {
    writeReg(0x2E, volume);  // ✓ LOUT1VOL - Volume saída esquerda 1
    writeReg(0x2F, volume);  // ✓ ROUT1VOL - Volume saída direita 1
    writeReg(0x30, volume);  // ✓ LOUT2VOL - Volume saída esquerda 2
    writeReg(0x31, volume);  // ✓ ROUT2VOL - Volume saída direita 2
}
```

### 2. Mixer Control Incorreto

**ANTES (ERRADO):**
```cpp
writeReg(0x27, 0x90);  // ❌ Valor incorreto bloqueava roteamento do áudio
```

**DEPOIS (CORRETO):**
```cpp
writeReg(0x27, 0x09);  // ✓ RMIXSEL = RDAC (conecta DAC direito ao mixer)
```

### 3. **CRÍTICO: Falta de Habilitação das Saídas LOUT (PROBLEMA PRINCIPAL)**

Este era o problema principal que impedia o áudio de chegar ao alto-falante.

**SINTOMA:**
- Amplificador ligado (PA_EN = HIGH)
- Ruído de fundo audível no alto-falante
- Nenhum som de áudio reproduzido
- Logs indicavam reprodução bem-sucedida

**CAUSA:**
O codec ES8388 estava configurado para processar o áudio internamente (DAC funcionando), mas as saídas físicas LOUT1/LOUT2 não estavam habilitadas corretamente. O sinal de áudio ficava "preso" dentro do chip.

**ANTES (INCOMPLETO):**
```cpp
// Apenas configurava o mixer, mas não habilitava as saídas físicas
writeReg(0x26, 0x09);  // LMIXSEL = LDAC
writeReg(0x27, 0x09);  // RMIXSEL = RDAC
// ❌ Faltava habilitar Power Management para saídas
```

**DEPOIS (CORRETO):**
```cpp
// Configurar mixer
writeReg(0x26, 0x09);  // LMIXSEL = LDAC (Left DAC to Left Mixer)
writeReg(0x27, 0x09);  // RMIXSEL = RDAC (Right DAC to Right Mixer)
writeReg(0x28, 0x38);  // LOUT2 mixer control
writeReg(0x29, 0x38);  // ROUT2 mixer control

// ✓ CRÍTICO: Habilitar saídas físicas LOUT1/LOUT2
writeReg(0x02, 0x00);  // Power Management 1: All powered up
writeReg(0x04, 0x3C);  // Power Management 2: DEM, STM enabled

// Configurar volume das saídas
writeReg(0x2E, 0x1C);  // LOUT1 volume = 28 (42dB)
writeReg(0x2F, 0x1C);  // ROUT1 volume = 28 (42dB)
writeReg(0x30, 0x1C);  // LOUT2 volume = 28
writeReg(0x31, 0x1C);  // ROUT2 volume = 28
```

### 4. Transição de Estado para Auto-Play

**ANTES (ERRADO):**
```cpp
// StateMachine.cpp - linha 26
if (currentState != IDLE && currentState != BOOT) return false;
// ❌ Bloqueava transição DOWNLOADING_AUDIO -> PLAYING
```

**DEPOIS (CORRETO):**
```cpp
if (currentState != IDLE && currentState != BOOT && currentState != DOWNLOADING_AUDIO) return false;
// ✓ Permite auto-play após download de áudio
```

### 5. Logs de Debug Adicionados

Adicionados logs detalhados em `AudioManager.cpp` para facilitar diagnóstico:

- Status do amplificador (PA_EN GPIO 21)
- Verificação do arquivo de áudio
- Tamanho do arquivo em bytes
- Status de cada etapa da cadeia de áudio
- Mensagens de erro detalhadas

## Cadeia de Áudio Completa

```
ARQUIVO MP3 (SPIFFS)
    ↓
ESP32-audioI2S Library (decodifica MP3)
    ↓
I2S Bus (BCLK=27, LRC=25, DOUT=26)
    ↓
ES8388 Codec (I2C: SDA=33, SCL=32)
    ↓
Power Amplifier (PA_EN GPIO 21 = HIGH)
    ↓
Alto-falante (LOUT/ROUT)
```

## Registradores ES8388 Críticos

| Registrador | Função | Valor Correto |
|-------------|--------|---------------|
| 0x26 | LMIXSEL (Left Mixer) | 0x09 (LDAC) |
| 0x27 | RMIXSEL (Right Mixer) | 0x09 (RDAC) |
| 0x2E | LOUT1VOL | 0-33 (volume) |
| 0x2F | ROUT1VOL | 0-33 (volume) |
| 0x30 | LOUT2VOL | 0-33 (volume) |
| 0x31 | ROUT2VOL | 0-33 (volume) |

## Verificação de Hardware

### Amplificador (PA_EN)
- GPIO 21 deve estar em **HIGH** durante reprodução
- Verificado automaticamente nos logs de inicialização e play

### Codec ES8388
- Endereço I2C: **0x10**
- Pinos I2C: SDA=33, SCL=32
- Pinos I2S: BCLK=27, LRC=25, DOUT=26

### Volume (3 níveis)
1. **ES8388 Codec**: 0-33 (registradores 0x2E-0x31)
2. **I2S Library**: 0-21 (mapeado internamente)
3. **MQTT Config**: 0-10 (mapeado para 0-21)

## Como Testar

1. Compile e faça upload do firmware 4.1.0
2. Monitore a porta serial (115200 baud)
3. Envie um novo áudio via painel admin
4. Observe os logs detalhados:

```
[Audio] ========== PLAY REQUEST ==========
[Audio] ✓ Audio file exists - Size: 83216 bytes (81.27 KB)
[Audio] PA_EN status: HIGH ✓
[Audio] Calling audio.connecttoFS() for: /audio.mp3
[Audio] ✓ Playback started successfully!
[Audio] Audio chain: FILE -> I2S -> ES8388 -> PA -> SPEAKER
```

## Referências

- [ES8388 Datasheet](https://www.everest-semi.com/pdf/ES8388%20DS.pdf)
- [ESP32-audioI2S Library](https://github.com/schreibfaul1/ESP32-audioI2S)
- [Espressif ESP-ADF ES8388 Driver](https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/abstraction/es8388.html)

## Changelog

### v4.1.0+ (2026-03-04) - Correção Crítica de Áudio
- ✅ **CRÍTICO:** Habilitadas saídas físicas LOUT1/LOUT2 do ES8388 (registradores 0x02, 0x04)
- ✅ **CRÍTICO:** Corrigido roteamento DAC → Mixer → LOUT (problema principal)
- ✅ Corrigidos registradores de volume do ES8388 (0x2E-0x31)
- ✅ Corrigido mixer control (0x27 de 0x90 para 0x09)
- ✅ Adicionada transição DOWNLOADING_AUDIO -> PLAYING
- ✅ Logs detalhados de debug de áudio com caminho completo do sinal
- ✅ Verificação automática do PA_EN antes de cada reprodução
- ✅ Reaplicação de configuração do codec antes de reproduzir
- ✅ Documentação completa do caminho do áudio

### v4.1.0 (2026-03-04) - Versão Anterior
- ⚠️ Áudio baixava mas não reproduzia (saídas LOUT não habilitadas)
- ✅ Sistema dual de configuração (idle/trigger)
- ✅ Auto-play funcional
- ✅ Integração MQTT completa
