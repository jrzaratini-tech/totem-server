# Diagnóstico de Hardware - ES8388 Audio Kit

## Problema
Software reporta "Playback started successfully", todos os registros I2C corretos (0x17=0x18, 0x26=0x09, 0x27=0x90), mas apenas ruído/chiado é ouvido em vez do áudio gravado.

## 1. Teste de Tom Interno do ES8388

O ES8388 **não possui gerador de test tone interno**. Alternativas:

### Opção A: Gerar Tom Senoidal via I2S (Software)
```cpp
// Adicionar em AudioManager.cpp - função de teste
void AudioManager::testTone() {
    Serial.println("[Audio] Generating 1kHz test tone...");
    
    // Configurar I2S para gerar tom de 1kHz
    // Sample rate: 44100 Hz
    // Frequência: 1000 Hz
    // Amplitude: 50% (para evitar clipping)
    
    const int sampleRate = 44100;
    const int frequency = 1000;
    const int amplitude = 16000; // 50% de 32767
    
    int16_t samples[2]; // Stereo
    for (int i = 0; i < 100; i++) { // 100 ciclos = ~2.27 segundos
        for (int j = 0; j < sampleRate / frequency; j++) {
            float angle = 2.0 * PI * j / (sampleRate / frequency);
            int16_t sample = (int16_t)(amplitude * sin(angle));
            samples[0] = sample; // Left
            samples[1] = sample; // Right
            
            // Enviar via I2S
            // audio.writeI2S((uint8_t*)samples, 4);
        }
    }
}
```

### Opção B: Arquivo MP3 de Test Tone
1. Gere um arquivo MP3 de 1kHz usando Audacity
2. Faça upload via painel admin
3. Reproduza normalmente

**Se o test tone também sair como ruído, o problema é no caminho I2S → ES8388 → Amplificador**

## 2. Medições com Osciloscópio/Multímetro

### 2.1 Pinos I2S (ESP32 → ES8388)

| Pino | GPIO | Função | O que medir |
|------|------|--------|-------------|
| BCLK | 27 | Bit Clock | **Osciloscópio:** Clock quadrado ~1.4 MHz (44.1kHz × 32 bits) |
| LRC (WS) | 25 | Word Select | **Osciloscópio:** Clock quadrado 44.1 kHz (frequência de amostragem) |
| DOUT | 26 | Data Out | **Osciloscópio:** Dados digitais variando durante reprodução |

**Teste:**
```
1. Inicie reprodução de áudio
2. Meça BCLK: deve ter clock constante ~1.4 MHz
3. Meça LRC: deve ter clock 44.1 kHz
4. Meça DOUT: deve variar (não ficar em 0V ou 3.3V constante)
```

**Se DOUT está constante em 0V ou 3.3V → Problema no ESP32 ou biblioteca I2S**

### 2.2 Saída Analógica ES8388 (LOUT/ROUT)

| Pino ES8388 | Função | O que medir |
|-------------|--------|-------------|
| LOUT1 | Left Output 1 | **Multímetro AC:** ~1-2V RMS durante áudio |
| ROUT1 | Right Output 1 | **Multímetro AC:** ~1-2V RMS durante áudio |
| LOUT2 | Left Output 2 | **Multímetro AC:** ~1-2V RMS durante áudio |
| ROUT2 | Right Output 2 | **Multímetro AC:** ~1-2V RMS durante áudio |

**Teste:**
```
1. Configure multímetro para AC Voltage
2. Durante reprodução, meça entre LOUT1 e GND
3. Deve ler entre 0.5V - 2V RMS (varia com volume)
4. Se ler apenas ruído (~0.01V) → ES8388 não está gerando áudio
```

**Osciloscópio (melhor):**
- Conecte probe em LOUT1
- Durante reprodução de test tone 1kHz, deve ver onda senoidal limpa
- Se ver apenas ruído aleatório → Problema no ES8388 ou configuração I2S

### 2.3 Alimentação do Amplificador

| Pino | Função | Tensão Esperada |
|------|--------|-----------------|
| PA_EN (GPIO 21) | Power Amp Enable | **3.3V** (HIGH) durante reprodução |
| VCC Amplificador | Alimentação | **5V** (verifique esquemático) |
| GND | Terra | **0V** |

**Teste:**
```
1. Multímetro DC: Meça PA_EN → deve estar em 3.3V
2. Multímetro DC: Meça VCC do amplificador → deve estar em 5V
3. Se PA_EN = 0V → Amplificador desligado (problema software)
4. Se VCC < 4.5V → Problema de alimentação
```

## 3. Teste do Amplificador PAM8403

### Método 1: Bypass do ES8388
```
1. Desconecte entrada do amplificador (LOUT/ROUT do ES8388)
2. Conecte fonte de áudio externa (celular com cabo P2)
3. Use divisor de tensão para reduzir nível (celular = ~1V RMS)
4. Se amplificador reproduzir áudio limpo → ES8388 é o problema
5. Se continuar com ruído → Amplificador defeituoso
```

### Método 2: Verificar Componentes
```
1. Inspeção visual: capacitores inchados/queimados?
2. Resistência DC entre saída e GND: deve ser > 1kΩ
3. Curto-circuito entre VCC e GND: deve ser > 100kΩ
```

### Sintomas de Amplificador Defeituoso
- ✓ Ruído constante mesmo sem sinal de entrada
- ✓ Distorção em todos os volumes
- ✓ Aquecimento excessivo do chip
- ✓ Consumo de corrente anormal (> 500mA em idle)

## 4. Jumpers/Configurações Físicas - ESP32-Audio-Kit V2.2

### Jumpers Críticos

| Jumper | Posição Correta | Função |
|--------|-----------------|--------|
| **J4** | **Fechado** | Conecta LOUT/ROUT ao amplificador |
| **J5** | **Aberto** | Desconecta line-out (se quiser apenas alto-falante) |
| **J6** | **Fechado** | Habilita alimentação do amplificador |

**CRÍTICO:** Verifique se o jumper J4 está fechado (soldado ou com jumper cap). Sem ele, o áudio do ES8388 não chega ao amplificador!

### Seleção de Saída de Áudio

A placa ESP32-Audio-Kit V2.2 tem 3 opções de saída:

1. **Alto-falantes** (padrão): J4 fechado, J5 aberto
2. **Line-out (P2)**: J4 aberto, J5 fechado
3. **Ambos**: J4 e J5 fechados (pode reduzir volume)

### Verificação Visual
```
1. Localize os jumpers perto do conector de alto-falante
2. J4 deve ter solda/jumper conectando os 2 pads
3. Se J4 estiver aberto → ESTE É O PROBLEMA!
4. Solde os 2 pads do J4 ou coloque jumper cap
```

## 5. Teste Mínimo: Onda Senoidal via I2S

### Código de Teste (adicionar em main.cpp)

```cpp
#include "driver/i2s.h"

void generateTestTone() {
    Serial.println("[Test] Generating 1kHz sine wave via I2S...");
    
    const int sampleRate = 44100;
    const int frequency = 1000;
    const int amplitude = 10000; // Amplitude moderada
    const int duration = 5; // 5 segundos
    
    int16_t *samples = (int16_t*)malloc(sampleRate * 4); // 2 canais, 2 bytes cada
    
    // Gerar 1 segundo de áudio
    for (int i = 0; i < sampleRate; i++) {
        float angle = 2.0 * PI * frequency * i / sampleRate;
        int16_t sample = (int16_t)(amplitude * sin(angle));
        samples[i * 2] = sample;     // Left
        samples[i * 2 + 1] = sample; // Right
    }
    
    // Reproduzir por 5 segundos
    for (int sec = 0; sec < duration; sec++) {
        size_t bytes_written;
        i2s_write(I2S_NUM_0, samples, sampleRate * 4, &bytes_written, portMAX_DELAY);
        Serial.printf("[Test] Second %d/%d\n", sec + 1, duration);
    }
    
    free(samples);
    Serial.println("[Test] Test tone finished");
}
```

### Como Usar
```cpp
void setup() {
    // ... inicialização normal ...
    
    // Após inicializar ES8388
    delay(2000);
    generateTestTone(); // Gerar tom de teste
}
```

### Resultado Esperado
- **Tom limpo de 1kHz**: Hardware OK, problema no arquivo MP3 ou decodificação
- **Ruído/chiado**: Problema no caminho I2S → ES8388 → Amplificador
- **Silêncio**: Amplificador desligado ou ES8388 não configurado

## 6. Checklist de Diagnóstico

### Software (já verificado ✓)
- [x] Registros I2C corretos (0x17=0x18, 0x26=0x09, 0x27=0x90)
- [x] PA_EN = HIGH
- [x] Biblioteca reporta "Playback started"

### Hardware (verificar)
- [ ] **Jumper J4 fechado** (conecta ES8388 ao amplificador)
- [ ] **BCLK tem clock ~1.4 MHz** (osciloscópio)
- [ ] **LRC tem clock 44.1 kHz** (osciloscópio)
- [ ] **DOUT varia durante reprodução** (osciloscópio)
- [ ] **LOUT1 tem sinal AC ~1V RMS** (multímetro AC)
- [ ] **PA_EN = 3.3V** (multímetro DC)
- [ ] **VCC amplificador = 5V** (multímetro DC)
- [ ] **Alto-falante conectado corretamente** (polaridade)

## 7. Problemas Comuns e Soluções

| Sintoma | Causa Provável | Solução |
|---------|----------------|---------|
| Ruído constante, não varia com áudio | Jumper J4 aberto | Fechar jumper J4 |
| Ruído que varia com áudio | Formato I2S incorreto | Testar 0x17 = 0x00 (Left Justified) |
| Silêncio total | PA_EN = LOW ou amplificador sem alimentação | Verificar GPIO 21 e VCC |
| Áudio muito baixo com ruído | Ganho do amplificador incorreto | Ajustar resistor de ganho |
| Distorção em volume alto | Clipping no ES8388 | Reduzir volume (reg 0x2E-0x31) |

## 8. Teste Definitivo: Bypass Completo

Se nada funcionar, teste cada componente isoladamente:

### Teste 1: ESP32 I2S OK?
```cpp
// Medir BCLK, LRC, DOUT com osciloscópio durante i2s_write()
// Deve ver sinais digitais limpos
```

### Teste 2: ES8388 I2C OK?
```cpp
// Ler registros via I2C
// Valores devem corresponder aos escritos
```

### Teste 3: ES8388 DAC OK?
```cpp
// Medir LOUT1 com osciloscópio durante test tone
// Deve ver onda senoidal limpa de 1kHz
```

### Teste 4: Amplificador OK?
```cpp
// Injetar sinal externo (celular) na entrada do amplificador
// Deve reproduzir áudio limpo
```

## Conclusão

**Problema mais provável:** Jumper J4 aberto (ES8388 não conectado ao amplificador)

**Segundo mais provável:** Formato I2S incompatível (testar 0x17 = 0x00 em vez de 0x18)

**Terceiro:** Arquivo MP3 corrompido ou codec incompatível (testar test tone)
