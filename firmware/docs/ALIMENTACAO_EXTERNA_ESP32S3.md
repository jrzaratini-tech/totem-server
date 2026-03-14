# Alimentação Externa - ESP32-S3 + LEDs + Amplificador

## Problema Identificado

**Sintoma**: ESP32-S3 funciona via USB mas falha com fonte externa 5V 10A

**Causa**: Fiação incorreta - 5V conectado diretamente no pino 5V do ESP32-S3

## ⚠️ Fiação INCORRETA (Atual)

```
Fonte 5V 10A:
├─ GND → GND ESP32-S3 ✓
├─ GND → GND MAX98357A ✓
├─ 5V → VIN MAX98357A ✓
└─ 5V → Pino 5V ESP32-S3 ❌ ERRO!
```

### Por Que Está Errado?

1. **Pino 5V é SAÍDA**: O pino 5V do ESP32-S3 é uma saída quando alimentado via USB, não uma entrada
2. **Bypassa regulador**: Conectar 5V direto pode danificar o chip (precisa de 3.3V)
3. **Instabilidade**: Queda de tensão dos LEDs afeta o ESP32-S3
4. **Risco de dano**: Pode queimar o regulador interno ou o próprio ESP32-S3

## ✅ Soluções Corretas

### Solução 1: Usar Pino VIN (RECOMENDADO)

**Verifique se sua placa ESP32-S3 tem pino VIN (ou VUSB)**

```
Fonte 5V 10A:
│
├─ GND ────────┬─→ GND ESP32-S3
│              ├─→ GND MAX98357A
│              └─→ GND Fita LED (comum)
│
└─ 5V ─────────┬─→ VIN ESP32-S3 (ou VUSB)
               ├─→ VIN MAX98357A (3.3V-5V)
               └─→ 5V Fita LED
```

**Como funciona:**
- VIN → Regulador interno → 3.3V para o ESP32-S3 ✓
- Aceita 5V-12V (depende do regulador da placa)
- Protege o chip contra sobretensão

**Vantagens:**
- ✅ Simples e direto
- ✅ Usa regulador interno da placa
- ✅ Protegido contra picos

**Desvantagens:**
- ⚠️ Queda de tensão dos LEDs pode afetar o ESP32-S3
- ⚠️ Nem todas as placas têm pino VIN

### Solução 2: Regulador Externo 3.3V (MAIS SEGURO)

**Use um módulo regulador de tensão (LM2596, AMS1117-3.3, etc.)**

```
Fonte 5V 10A:
│
├─ GND ────────┬─→ GND Regulador 3.3V
│              ├─→ GND ESP32-S3
│              ├─→ GND MAX98357A
│              └─→ GND Fita LED
│
└─ 5V ─────────┬─→ IN Regulador 3.3V
               ├─→ VIN MAX98357A
               └─→ 5V Fita LED

Regulador 3.3V (LM2596 ou similar):
└─ OUT 3.3V ───→ 3.3V ESP32-S3
```

**Reguladores recomendados:**
- **LM2596 DC-DC Step Down** (até 3A, eficiente)
- **AMS1117-3.3** (até 1A, simples)
- **XL4015 DC-DC** (até 5A, alta corrente)

**Vantagens:**
- ✅ Tensão estável e isolada
- ✅ Protege contra picos dos LEDs
- ✅ Mais confiável e profissional
- ✅ Regulador dedicado para o ESP32-S3

**Desvantagens:**
- ⚠️ Requer componente adicional (~R$5-15)

### Solução 3: Fontes Separadas (MÁXIMA SEGURANÇA)

**Use duas fontes independentes**

```
┌─────────────────────────────────────────┐
│ Fonte 5V 10A (LEDs + Amplificador)     │
├─────────────────────────────────────────┤
│                                         │
├─ GND ────────┬─→ GND MAX98357A         │
│              ├─→ GND Fita LED          │
│              └─→ GND Comum ────────────┼─┐
│                                         │ │
└─ 5V ─────────┬─→ VIN MAX98357A         │ │
               └─→ 5V Fita LED           │ │
                                          │ │
┌─────────────────────────────────────────┤ │
│ Fonte 5V 1A ou USB (ESP32-S3)          │ │
├─────────────────────────────────────────┤ │
│                                         │ │
├─ GND ────────→ GND ESP32-S3 ───────────┼─┘
│                                         │
└─ 5V USB ─────→ USB ESP32-S3            │
   ou VIN ─────→ VIN ESP32-S3            │
                                          │
└─────────────────────────────────────────┘

IMPORTANTE: GNDs devem estar conectados (terra comum)
```

**Vantagens:**
- ✅ Isolamento total
- ✅ ESP32-S3 nunca afetado por LEDs
- ✅ Máxima estabilidade
- ✅ Pode usar USB durante desenvolvimento

**Desvantagens:**
- ⚠️ Requer duas fontes
- ⚠️ Mais cabos e conexões

## Esquema Completo Recomendado

### Opção A: Com Regulador Externo (Melhor Custo-Benefício)

```
┌──────────────┐
│ Fonte 5V 10A │
└──┬───────┬───┘
   │       │
   │       └─────────────────────────────┐
   │                                     │
   │  ┌────────────────┐                 │
   └──┤ Regulador 3.3V │                 │
      │   (LM2596)     │                 │
      └────┬───────────┘                 │
           │                             │
           │ 3.3V                        │ 5V
           │                             │
      ┌────▼─────────────┐          ┌────▼──────────┐
      │   ESP32-S3       │          │  MAX98357A    │
      │                  │          │  Amplificador │
      │ GPIO 8 ──────────┼──────────┤               │
      │ GPIO 5,6,7 ──────┼──────────┤ BCLK,LRC,DIN  │
      │ GND ─────────────┼──────────┤ GND           │
      └──────────────────┘          └───────────────┘
                                             │
                                             │ Audio Out
                                             ▼
                                      ┌──────────────┐
                                      │ Alto-falante │
                                      │    4-8Ω      │
                                      └──────────────┘

      ┌────────────────────────────────────┐
      │ Fita LED WS2812B (208 LEDs)        │
      │ DIN ◄─── GPIO 1                    │
      │ 5V  ◄─── Fonte 5V 10A              │
      │ GND ◄─── GND Comum                 │
      └────────────────────────────────────┘
```

### Lista de Materiais

**Essencial:**
- ✅ Fonte 5V 10A (para LEDs e amplificador)
- ✅ Regulador 3.3V (LM2596 ou similar)
- ✅ Fios adequados (AWG 18-20 para 5V, AWG 22-24 para sinais)
- ✅ Conectores (XT60, JST, ou terminais)

**Opcional mas recomendado:**
- ✅ Capacitor 1000µF 16V (próximo ao ESP32-S3)
- ✅ Capacitor 1000µF 16V (próximo aos LEDs)
- ✅ Fusível 10A (proteção da fonte)
- ✅ Diodo Schottky 5A (proteção contra inversão)

## Checklist de Verificação

Antes de ligar:

- [ ] **GND comum**: Todos os GNDs conectados juntos
- [ ] **Tensão correta**: 3.3V no pino 3.3V do ESP32-S3 (medir com multímetro)
- [ ] **Polaridade**: Positivo e negativo corretos
- [ ] **Capacitores**: Instalados próximos aos componentes
- [ ] **Conexões firmes**: Nenhum fio solto
- [ ] **Isolamento**: Nenhum curto-circuito

Após ligar:

- [ ] **LED de status**: ESP32-S3 acende LED interno
- [ ] **Serial**: Monitor serial mostra boot normal
- [ ] **WiFi**: Conecta na rede
- [ ] **LEDs**: Acendem sem piscar ou apagar
- [ ] **Áudio**: Reproduz sem falhas

## Troubleshooting

### ESP32-S3 reinicia aleatoriamente
**Causa**: Queda de tensão quando LEDs acendem
**Solução**: 
- Adicionar capacitor 1000µF próximo ao ESP32-S3
- Usar regulador dedicado (Solução 2)
- Reduzir brilho máximo dos LEDs

### LEDs piscam ou apagam
**Causa**: Fonte insuficiente ou cabos finos
**Solução**:
- Verificar fonte (mínimo 10A para 208 LEDs)
- Usar cabos mais grossos (AWG 18 ou menor)
- Alimentar LEDs em múltiplos pontos

### Áudio com ruído ou falhas
**Causa**: Interferência dos LEDs ou GND mal conectado
**Solução**:
- Verificar GND comum bem conectado
- Adicionar capacitor no amplificador
- Usar cabos blindados para I2S

### ESP32-S3 esquenta muito
**Causa**: Tensão errada (5V direto no chip)
**Solução**:
- ⚠️ DESLIGUE IMEDIATAMENTE
- Verificar fiação (não conectar 5V no pino 5V)
- Usar VIN ou regulador 3.3V

### Fonte desliga sozinha
**Causa**: Sobrecarga ou curto-circuito
**Solução**:
- Verificar curtos com multímetro (modo continuidade)
- Reduzir brilho dos LEDs
- Verificar capacidade da fonte

## Cálculo de Corrente

### Consumo Estimado

**ESP32-S3:**
- Idle: ~80mA
- WiFi ativo: ~160mA
- Máximo: ~250mA

**MAX98357A:**
- Idle: ~10mA
- Reproduzindo: ~500mA (3W @ 4Ω)

**LEDs WS2812B (208 LEDs):**
- Por LED: ~60mA máximo (branco total)
- 208 LEDs: ~12.5A máximo
- Uso típico (50% brilho, cores): ~4-7A

**Total:**
- Mínimo: ~100mA (tudo idle)
- Típico: ~5A (operação normal)
- Máximo: ~15A (LEDs brancos 100%)

**Recomendação**: Fonte de **10A** é adequada se limitar brilho a 70-80%

## Configuração de Brilho Seguro

Para evitar sobrecarga, ajuste o brilho máximo no código:

```cpp
// Config.h
#define MAX_BRIGHTNESS 140  // Em vez de 180 (77% do máximo)
```

Isso limita o consumo a ~10A, deixando margem de segurança.

## Resumo

**Problema**: 5V conectado direto no pino 5V do ESP32-S3 ❌

**Solução Rápida**: Conectar 5V no pino **VIN** (se disponível) ✅

**Solução Ideal**: Usar regulador 3.3V dedicado (LM2596) ✅

**Solução Máxima**: Fontes separadas para ESP32-S3 e LEDs ✅
