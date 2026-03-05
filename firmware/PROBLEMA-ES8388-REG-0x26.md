# Problema: Registrador 0x26 do ES8388 Não Mantém Valor

## Sintoma
- Apenas chiado/interferência no alto-falante
- Nenhum áudio reproduzido

## Causa Raiz Identificada
O registrador **0x26 (LMIXSEL - Left Mixer Select)** do ES8388 não aceita o valor **0x90** e retorna **0x10**.

```
[ES8388] Mixer check - 0x26: 0x10, 0x27: 0x90, 0x04: 0x3C
[ES8388] ⚠ Reg 0x26 incorrect, forcing multiple writes...
[ES8388] Attempt 1 - Reg 0x26: 0x10
[ES8388] Attempt 2 - Reg 0x26: 0x10
[ES8388] Attempt 3 - Reg 0x26: 0x10
[ES8388] Attempt 4 - Reg 0x26: 0x10
[ES8388] Attempt 5 - Reg 0x26: 0x10
```

## Análise do Valor 0x10

**0x10 = 0b00010000**
- Bit 4 = 1: Não corresponde a nenhuma configuração válida do mixer
- Bits 0-3 = 0: DAC não roteado para mixer
- **Resultado:** Canal esquerdo sem áudio

## Soluções Tentadas

### 1. ✗ Reescrita Múltipla com 0x90
Tentou escrever 0x90 cinco vezes - falhou em todas

### 2. ⏳ Valor Alternativo 0x09 (Em Teste)
**0x09 = 0b00001001**
- Bit 0 (LI2LO) = 1: Left Input 2 to Left Output
- Bit 3 (LD2LO) = 1: Left DAC to Left Output
- Ganho: 0dB

Este valor pode funcionar melhor neste hardware específico.

## Próximas Ações

### Se 0x09 Funcionar
✓ Áudio deve reproduzir normalmente

### Se 0x09 Também Falhar
Possíveis causas:

1. **Problema de Hardware I2C**
   - Verificar conexões SDA (GPIO 33) e SCL (GPIO 32)
   - Testar com pull-ups externos (4.7kΩ)

2. **ES8388 Defeituoso**
   - Chip pode ter problema de fabricação
   - Registrador 0x26 pode estar danificado

3. **Incompatibilidade de Versão do Chip**
   - Algumas revisões do ES8388 têm bugs conhecidos
   - Verificar marcação do chip na placa

## Teste de Diagnóstico I2C

Adicionar código para verificar comunicação I2C:

```cpp
// Testar escrita/leitura de outros registradores
writeReg(0x2E, 0x15);  // Volume LOUT1
delay(10);
uint8_t test = readReg(0x2E);
Serial.printf("[TEST] Write 0x15, Read: 0x%02X %s\n", 
             test, test == 0x15 ? "OK" : "FAIL");
```

## Alternativa: Usar Apenas Canal Direito

Se o canal esquerdo não funcionar, configurar para mono:

```cpp
// Rotear canal direito para ambas as saídas
writeReg(0x26, 0x90);  // Mesmo que falhe
writeReg(0x27, 0x90);  // Canal direito OK
// Conectar alto-falante apenas em ROUT1
```

## Referências
- ESP-ADF ES8388 Driver: https://github.com/espressif/esp-adf
- ES8388 Datasheet (registrador 0x26, página 52)
- Bug reports similares em fóruns ESP32
