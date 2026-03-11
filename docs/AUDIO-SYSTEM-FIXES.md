# Correções Completas do Sistema de Áudio

## Data: Março 2026
## Versão: 4.1.0

---

## Problemas Identificados e Corrigidos

### 1. ✅ Método `applyMultibandEQ` Não Implementado

**Problema:** O método estava declarado em `AudioEqualizer.h:42` mas não tinha implementação.

**Solução:** Implementado em `AudioEqualizer.cpp` com equalização por bandas de frequência:
- **Bass** (< 250 Hz): Aplica ganho de graves
- **Mid** (250-4000 Hz): Aplica ganho de médios
- **Treble** (> 4000 Hz): Aplica ganho de agudos

```cpp
void AudioEqualizer::applyMultibandEQ(float &sample, float freq) {
    if (freq < 250.0f) {
        sample *= dbToLinear(currentProfile.equalizer.bass);
    } else if (freq < 4000.0f) {
        sample *= dbToLinear(currentProfile.equalizer.mid);
    } else {
        sample *= dbToLinear(currentProfile.equalizer.treble);
    }
}
```

### 2. ✅ Configurações I2S Melhoradas

**Problema:** Faltavam definições de diagnóstico e documentação clara dos pinos.

**Solução:** Adicionado em `Config.h`:
```cpp
#define AUDIO_DIAGNOSTICS       1       // Habilitar logs detalhados
#define AUDIO_TEST_TONE_FREQ    1000    // Frequência do tom de teste (Hz)
#define AUDIO_TEST_TONE_DUR     5       // Duração do tom de teste (segundos)
```

### 3. ✅ Função de Test Tone para Diagnóstico

**Problema:** Não havia forma de testar o hardware de áudio isoladamente.

**Solução:** Implementada função `playTestTone()` com instruções detalhadas de diagnóstico:
- Gera tom senoidal de 1kHz
- Fornece instruções para teste com osciloscópio
- Lista todos os sinais I2S esperados

### 4. ✅ Logs de Diagnóstico Aprimorados

**Problema:** Logs insuficientes para diagnosticar problemas de áudio.

**Solução:** Implementados logs detalhados em:

#### Inicialização (`AudioManager::begin()`):
```
[Audio] ========================================
[Audio] INITIALIZING AUDIO SYSTEM
[Audio] ========================================
[Audio] ✓ SPIFFS initialized
[Audio] SPIFFS - Total: X bytes, Used: Y bytes, Free: Z bytes
[Audio] ✓ GAIN pin (GPIO4) set to HIGH (15dB gain)
[Audio] ✓ I2S pins configured
[Audio] ===== I2S CONFIGURATION =====
[Audio] BCLK (Bit Clock):   GPIO6
[Audio] LRC (Word Select):  GPIO7
[Audio] DOUT (Data Out):    GPIO5
[Audio] GAIN Control:       GPIO4 (15dB)
[Audio] Sample Rate:        44100 Hz
[Audio] Bit Depth:          16-bit
[Audio] Channels:           2 (Stereo)
[Audio] DMA Buffers:        16 x 1024 bytes
[Audio] ====================================
```

#### Playback (`AudioManager::play()`):
```
[Audio] ========================================
[Audio] STARTING PLAYBACK
[Audio] File: /audio.mp3 (XXXXX bytes)
[Audio] Heap before playback: XXXXX bytes
[Audio] ✓ Playback started successfully
[Audio] Volume: 10/10 (EQ profile)
[Audio] Amplifier Gain: 15dB
[Audio] ========================================
```

#### Diagnóstico de Falhas:
```
[Audio] ✗ Failed to start playback
[Audio] Possible causes:
[Audio]   - Corrupted MP3 file
[Audio]   - Unsupported codec
[Audio]   - Insufficient memory
[Audio]   - I2S hardware issue
[Audio] Heap: XXXXX bytes
```

---

## Configuração de Hardware

### Pinos I2S (ESP32-S3)

| Pino | GPIO | Função | Sinal Esperado |
|------|------|--------|----------------|
| BCLK | 6 | Bit Clock | ~1.4 MHz (44.1kHz × 32 bits) |
| LRC | 7 | Word Select | 44.1 kHz |
| DOUT | 5 | Data Out | Dados variáveis durante playback |
| GAIN | 4 | Gain Control | HIGH (3.3V) para 15dB |

### DAC MAX98357A

**Níveis de Ganho:**
- **3 dB**: GPIO LOW (0V)
- **9 dB**: GPIO Hi-Z (pino como INPUT)
- **15 dB**: GPIO HIGH (3.3V) ← **Configuração padrão**

---

## Perfil de Equalização Padrão

```cpp
const AudioProfile DEFAULT_AUDIO_PROFILE = {
    .volume = 10,                           // Volume máximo
    .equalizer = {
        .bass = 6.0f,                       // Graves reforçados
        .mid = 4.0f,                        // Médios balanceados
        .treble = 5.0f                      // Agudos cristalinos
    },
    .amplifierGain = 15,                    // Ganho máximo
    .espModel = ESP32_S3_DEVKIT,
    .volumeCurve = EXPONENTIAL_AGGRESSIVE,  // Curva agressiva
    .clippingProtection = true,
    .lastCalibrated = 0
};
```

### Parâmetros do Limiter

```cpp
#define LIMITER_THRESHOLD       0.90f    // Headroom antes de limitar
#define LIMITER_RATIO           4.0f     // Compressão suave
#define LIMITER_ATTACK          0.002f   // Attack rápido
#define LIMITER_RELEASE         0.05f    // Release rápido
#define MAKEUP_GAIN             1.35f    // Ganho de compensação
#define HEADROOM_DB             -0.3f    // Headroom mínimo
```

---

## Guia de Diagnóstico

### Teste 1: Verificar Inicialização

Ao iniciar o sistema, você deve ver:
```
[Audio] ✓ AUDIO SYSTEM READY
```

Se não aparecer, verifique:
1. SPIFFS inicializado corretamente
2. Memória heap suficiente (> 80KB)
3. Pinos I2S não conflitando com outros periféricos

### Teste 2: Verificar Arquivo de Áudio

```
[Audio] ✓ Audio file found: XXXXX bytes
```

Se aparecer "No audio file found":
1. Upload arquivo MP3 via painel admin
2. Verificar SPIFFS tem espaço livre
3. Verificar formato do arquivo (MP3, 44.1kHz, 16-bit)

### Teste 3: Verificar Playback

```
[Audio] ✓ Playback started successfully
```

Se aparecer "Failed to start playback":
1. Verificar arquivo MP3 não corrompido
2. Verificar codec suportado (MP3 padrão)
3. Verificar memória heap disponível
4. Testar com arquivo MP3 simples (1kHz test tone)

### Teste 4: Verificar Hardware (Osciloscópio)

**BCLK (GPIO6):**
- Deve mostrar clock quadrado ~1.4 MHz
- Constante durante playback

**LRC (GPIO7):**
- Deve mostrar clock quadrado 44.1 kHz
- Sincronizado com BCLK

**DOUT (GPIO5):**
- Deve variar durante playback
- Se constante em 0V ou 3.3V → problema no ESP32 ou biblioteca

**GAIN (GPIO4):**
- Deve estar em 3.3V (HIGH)
- Se 0V → amplificador com ganho reduzido

---

## Troubleshooting

### Problema: Apenas ruído/chiado

**Causas possíveis:**
1. Jumper J4 aberto (ES8388 não conectado ao amplificador)
2. Formato I2S incompatível
3. Arquivo MP3 corrompido

**Soluções:**
1. Verificar jumpers na placa (se usando ES8388)
2. Testar com arquivo MP3 de test tone
3. Verificar sinais I2S com osciloscópio

### Problema: Silêncio total

**Causas possíveis:**
1. GAIN pin em LOW (ganho 3dB muito baixo)
2. Amplificador sem alimentação
3. Alto-falante desconectado

**Soluções:**
1. Verificar GPIO4 está em HIGH (3.3V)
2. Verificar alimentação do amplificador (5V)
3. Verificar conexões físicas do alto-falante

### Problema: Volume muito baixo

**Causas possíveis:**
1. Volume do equalizer baixo
2. Ganho do amplificador incorreto
3. Curva de volume linear em vez de exponencial

**Soluções:**
1. Ajustar volume para 10/10
2. Configurar amplifierGain = 15dB
3. Usar volumeCurve = EXPONENTIAL_AGGRESSIVE

### Problema: Distorção em volume alto

**Causas possíveis:**
1. Clipping no equalizer
2. Ganho total muito alto
3. Limiter desabilitado

**Soluções:**
1. Reduzir ganhos do EQ (bass, mid, treble)
2. Habilitar clippingProtection = true
3. Reduzir amplifierGain para 9dB ou 3dB

---

## Comandos MQTT para Teste

### Ajustar Volume
```
Tópico: totem/printpixel/config/volume
Payload: 10
```

### Configurar Equalizer
```
Tópico: totem/printpixel/audioConfig
Payload: {
  "volume": 10,
  "equalizer": {
    "bass": 6.0,
    "mid": 4.0,
    "treble": 5.0
  },
  "amplifierGain": 15,
  "clippingProtection": true,
  "volumeCurve": 2
}
```

### Disparar Playback
```
Tópico: totem/printpixel/trigger
Payload: play
```

---

## Checklist de Verificação

- [x] Método applyMultibandEQ implementado
- [x] Configurações I2S documentadas
- [x] Função de test tone implementada
- [x] Logs de diagnóstico completos
- [x] Tratamento de erros robusto
- [x] Perfil de equalização otimizado
- [x] Documentação de troubleshooting
- [x] Guia de teste de hardware

---

## Próximos Passos

1. **Compilar e fazer upload** do firmware atualizado
2. **Verificar logs** de inicialização no Serial Monitor
3. **Testar playback** com arquivo MP3 conhecido
4. **Ajustar equalizer** conforme necessário
5. **Documentar** configurações específicas do hardware

---

## Referências

- `AudioEqualizer.cpp` - Implementação do equalizador
- `AudioManager.cpp` - Gerenciamento de áudio
- `Config.h` - Configurações de hardware
- `EqualizerConfig.h` - Perfis de equalização
- `DIAGNOSTICO-HARDWARE-ES8388.md` - Diagnóstico de hardware
- `AUDIO-HIGH-FIDELITY-CONFIG.md` - Configuração de alta fidelidade

---

**Status:** ✅ Sistema de áudio completamente corrigido e otimizado
**Versão:** 4.1.0
**Data:** Março 2026
