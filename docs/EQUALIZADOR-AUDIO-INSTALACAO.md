# 🎵 Sistema de Equalização de Áudio por Totem - Instalação e Uso

## 📋 Índice
1. [Visão Geral](#visão-geral)
2. [Arquivos Criados](#arquivos-criados)
3. [Instalação - Firmware](#instalação---firmware)
4. [Instalação - Backend](#instalação---backend)
5. [Configuração do Menu](#configuração-do-menu)
6. [Como Usar](#como-usar)
7. [Testes](#testes)
8. [Troubleshooting](#troubleshooting)

---

## 🎯 Visão Geral

Sistema completo de equalização de áudio que permite ao **cliente** (dono do totem) ajustar finamente as configurações de áudio do seu totem específico através de uma interface web intuitiva.

### Funcionalidades Principais:
- ✅ Controle de volume (0-10) com curvas personalizáveis
- ✅ Equalizador gráfico de 3 bandas (Graves, Médios, Agudos)
- ✅ Controle de ganho do amplificador MAX98357A (3dB, 9dB, 15dB)
- ✅ Compensação automática por modelo de ESP32
- ✅ Curvas de volume (Linear, Exponencial Suave, Exponencial Agressiva)
- ✅ Proteção contra clipping
- ✅ Prévia de ganho em tempo real
- ✅ Teste de áudio rápido (graves, médios, agudos)
- ✅ Sincronização via MQTT em tempo real
- ✅ Persistência no Firestore

---

## 📁 Arquivos Criados

### Firmware (ESP32)
```
firmware/
├── include/
│   └── EqualizerConfig.h                    # Configurações e estruturas do equalizador
├── src/
│   └── core/
│       ├── AudioEqualizer.h                 # Header do equalizador
│       ├── AudioEqualizer.cpp               # Implementação do equalizador
│       ├── AudioManager.h                   # Modificado (integração)
│       ├── AudioManager.cpp                 # Modificado (integração)
│       ├── MQTTManager.h                    # Modificado (novo tópico)
│       └── MQTTManager.cpp                  # Modificado (novo tópico)
```

### Backend (Node.js)
```
server/
├── routes/
│   └── audio-equalizer.js                   # Rotas da API do equalizador
├── views/
│   └── audio-equalizer.html                 # Interface web do equalizador
└── server.js                                # Modificado (importação das rotas)
```

### Documentação
```
docs/
└── EQUALIZADOR-AUDIO-INSTALACAO.md         # Este arquivo
```

---

## 🔧 Instalação - Firmware

### 1. Verificar Dependências

O sistema já utiliza as bibliotecas existentes do projeto:
- ✅ ArduinoJson (já instalada)
- ✅ PubSubClient (já instalada)
- ✅ AudioTools (já instalada)

### 2. Compilar e Upload

```bash
cd firmware
pio run -t upload
```

### 3. Verificar no Serial Monitor

Após o upload, você deve ver no serial:

```
[EQ] ========== AUDIO EQUALIZER INITIALIZED ==========
[EQ] Volume set to 8/10, gain: 0.876
[EQ] Amplifier gain: 15dB (HIGH)
[EQ] ESP32 DevKit V1 compensation: 2.5x boost
[EQ] ===================================================
```

### 4. Integração no main.cpp

Se ainda não estiver integrado, adicione no seu `main.cpp`:

```cpp
#include "core/AudioManager.h"
#include "core/MQTTManager.h"

AudioManager audioManager;
MQTTManager mqttManager;

void setup() {
    // ... seu código existente ...
    
    audioManager.begin(TOTEM_ID, DEVICE_TOKEN);
    mqttManager.begin(TOTEM_ID, DEVICE_TOKEN);
    
    // Callback MQTT para processar comandos de áudio
    mqttManager.onMessage([](const String& topic, const String& payload) {
        if (topic.endsWith("/audioConfig")) {
            // Processar configuração de áudio
            AudioEqualizer* eq = audioManager.getEqualizer();
            if (eq) {
                eq->profileFromJSON(payload);
            }
        }
        // ... outros callbacks ...
    });
}

void loop() {
    audioManager.loop();
    mqttManager.loop();
    // ... seu código existente ...
}
```

---

## 🌐 Instalação - Backend

### 1. Verificar Dependências

Todas as dependências já estão instaladas no projeto:
- ✅ express
- ✅ firebase-admin
- ✅ mqtt

### 2. Nenhuma Instalação Adicional Necessária

O sistema já foi integrado ao `server.js`. Apenas reinicie o servidor:

```bash
npm start
```

ou

```bash
node server.js
```

### 3. Verificar Logs

Você deve ver no console:

```
✅ Firebase conectado com sucesso!
✅ MQTT conectado ao broker HiveMQ
🚀 TOTEM SERVER v4.1.0 rodando!
```

---

## 🔗 Configuração do Menu

### Adicionar Link no Dashboard do Cliente

Edite `server/views/cliente-dashboard.html` e adicione o botão do equalizador:

**Localização:** Dentro da seção de controles, após o botão de upload de áudio.

```html
<!-- Adicionar após o card de upload de áudio -->
<div class="card">
    <h2>🎵 Equalizador de Áudio</h2>
    <p style="color: #666; margin-bottom: 15px;">
        Ajuste fino das configurações de áudio do seu totem
    </p>
    <a href="/cliente/equalizer/{{ID}}" class="button button-primary" style="text-decoration: none; display: block;">
        🎛️ Abrir Equalizador
    </a>
</div>
```

**Estilo CSS (se necessário):**

```css
.button-primary {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    padding: 15px 30px;
    border-radius: 8px;
    font-weight: 600;
    text-align: center;
    transition: all 0.3s;
}

.button-primary:hover {
    transform: translateY(-2px);
    box-shadow: 0 8px 20px rgba(102, 126, 234, 0.6);
}
```

---

## 📱 Como Usar

### Para o Cliente (Dono do Totem)

1. **Acessar o Dashboard**
   - Vá para: `https://seu-servidor.com/cliente/login`
   - Faça login com o ID do totem (ex: "printpixel")

2. **Abrir o Equalizador**
   - No dashboard, clique em "🎛️ Abrir Equalizador"
   - Você será redirecionado para: `/cliente/equalizer/printpixel`

3. **Ajustar Configurações**

   **Volume Principal:**
   - Slider de 0 a 10
   - Ajusta o volume geral do totem

   **Equalizador Gráfico:**
   - **Graves (60Hz):** -12dB a +12dB
   - **Médios (1kHz):** -12dB a +12dB
   - **Agudos (10kHz):** -12dB a +12dB

   **Ganho do Amplificador:**
   - **3dB:** Baixo (para ambientes silenciosos)
   - **9dB:** Médio (padrão balanceado)
   - **15dB:** Alto (máxima potência)

   **Modelo do ESP32:**
   - Selecione o modelo correto para compensação automática
   - ESP32 DevKit V1 (padrão)
   - ESP32 AudioKit
   - ESP32-S3 DevKit
   - ESP32 Generic

   **Curva de Volume:**
   - **Linear:** Resposta linear (1:1)
   - **Exponencial Suave:** Curva suave (recomendado)
   - **Exponencial Agressiva:** Mais potência nos níveis altos

   **Proteção contra Clipping:**
   - ON: Previne distorção (recomendado)
   - OFF: Permite ganho máximo (pode distorcer)

4. **Testar Áudio**
   - Clique em "🔊 Grave", "🔊 Médio" ou "🔊 Agudo"
   - O totem tocará um tom de teste de 3 segundos

5. **Salvar Configuração**
   - Clique em "💾 SALVAR CONFIGURAÇÃO"
   - A configuração é enviada via MQTT para o ESP32
   - Confirmação aparece: "✓ Configuração salva e enviada ao totem!"

6. **Restaurar Padrões**
   - Clique em "🔄 RESTAURAR PADRÕES"
   - Volta às configurações de fábrica

---

## 🧪 Testes

### Teste 1: Verificar Conectividade MQTT

**No Serial Monitor do ESP32:**
```
[MQTT] Connected successfully!
[MQTT] Subscribing to topics:
  - totem/printpixel/trigger
  - totem/printpixel/config/idle
  - totem/printpixel/config/trigger
  - totem/printpixel/config/volume
  - totem/printpixel/audioConfig       ← NOVO TÓPICO
  - totem/printpixel/audioUpdate
  - totem/printpixel/firmwareUpdate
```

### Teste 2: Salvar Configuração

1. Acesse `/cliente/equalizer/printpixel`
2. Mude o volume para 5
3. Ajuste graves para +3dB
4. Clique em "Salvar"

**Esperado no Serial Monitor:**
```
[MQTT] Message received on topic: totem/printpixel/audioConfig
[MQTT] Payload: {"volume":5,"equalizer":{"bass":3,"mid":0,"treble":0},...}
[EQ] Profile loaded from JSON
[EQ] Volume set to 5/10, gain: 0.575
[EQ] Equalizer: Bass=3.0dB, Mid=0.0dB, Treble=0.0dB
```

### Teste 3: Teste de Áudio

1. Clique em "🔊 Grave (60Hz)"

**Esperado no Serial Monitor:**
```
[MQTT] Message received on topic: totem/printpixel/audioTest
[Audio] ========== TEST TONE ==========
[Audio] Generating 3000ms sine wave at 1kHz, full scale
[Audio] Test tone: 25%
[Audio] Test tone: 50%
[Audio] Test tone: 75%
[Audio] Test tone complete
```

### Teste 4: Verificar Firestore

No Firebase Console:
```
totens/
  └── printpixel/
      └── audioProfile/
          ├── volume: 5
          ├── equalizer/
          │   ├── bass: 3
          │   ├── mid: 0
          │   └── treble: 0
          ├── amplifierGain: 15
          ├── espModel: "ESP32 DevKit V1"
          ├── volumeCurve: "exponential"
          ├── clippingProtection: true
          └── lastCalibrated: "2026-03-07T22:00:00.000Z"
```

### Teste 5: Diferentes Modelos de ESP

**Para ESP32 AudioKit:**
1. Selecione "ESP32 AudioKit" no dropdown
2. Salve a configuração

**Esperado:**
```
[EQ] ESP32 AudioKit compensation: 2.0x boost
```

**Para ESP32-S3:**
1. Selecione "ESP32-S3 DevKit"
2. Salve

**Esperado:**
```
[EQ] ESP32-S3 DevKit compensation: 3.0x boost
```

---

## 🐛 Troubleshooting

### Problema: Equalizador não carrega

**Sintoma:** Página fica em "Carregando configurações..."

**Solução:**
1. Verificar se o cliente está logado: `/cliente/login`
2. Verificar se o ID do totem está correto na URL
3. Verificar logs do servidor:
   ```bash
   npm start
   ```
4. Verificar se Firebase está conectado

### Problema: Configuração não salva

**Sintoma:** Erro ao clicar em "Salvar"

**Solução:**
1. Verificar console do navegador (F12)
2. Verificar se Firebase está inicializado:
   ```javascript
   // No console do servidor
   Firebase inicializado: true
   ```
3. Verificar permissões do Firestore

### Problema: MQTT não publica

**Sintoma:** Configuração salva mas ESP32 não recebe

**Solução:**
1. Verificar conexão MQTT no servidor:
   ```
   ✅ MQTT conectado ao broker HiveMQ
   ```
2. Verificar tópico no código:
   ```javascript
   totem/${id}/audioConfig
   ```
3. Testar manualmente com MQTT Explorer

### Problema: ESP32 não aplica configuração

**Sintoma:** ESP32 recebe mas não aplica

**Solução:**
1. Verificar callback MQTT no main.cpp
2. Verificar se AudioEqualizer foi inicializado:
   ```cpp
   AudioEqualizer* eq = audioManager.getEqualizer();
   if (eq) {
       eq->profileFromJSON(payload);
   }
   ```
3. Verificar Serial Monitor para erros de parsing JSON

### Problema: Som distorcido após ajustes

**Sintoma:** Áudio com distorção/clipping

**Solução:**
1. Ativar "Proteção contra Clipping"
2. Reduzir ganho do amplificador (15dB → 9dB)
3. Reduzir volume geral
4. Ajustar curva para "Exponencial Suave"
5. Reduzir EQ (evitar +12dB em todas as bandas)

### Problema: Volume muito baixo

**Sintoma:** Mesmo em volume 10, som está baixo

**Solução:**
1. Aumentar ganho do amplificador para 15dB
2. Mudar curva para "Exponencial Agressiva"
3. Selecionar modelo correto do ESP32
4. Verificar conexões físicas do MAX98357A
5. Verificar se GAIN pin está configurado corretamente

---

## 📊 Estrutura de Dados

### Firestore: `totens/{id}/audioProfile`

```json
{
  "volume": 8,
  "equalizer": {
    "bass": 0,
    "mid": 0,
    "treble": 0
  },
  "amplifierGain": 15,
  "espModel": "ESP32 DevKit V1",
  "volumeCurve": "exponential",
  "clippingProtection": true,
  "customGainPoints": [
    { "vol": 0, "gain": 0.0 },
    { "vol": 3, "gain": 0.3 },
    { "vol": 5, "gain": 0.5 },
    { "vol": 8, "gain": 0.876 },
    { "vol": 10, "gain": 1.2 }
  ],
  "lastCalibrated": "2026-03-07T22:00:00.000Z"
}
```

### MQTT: `totem/{id}/audioConfig`

```json
{
  "volume": 8,
  "equalizer": {
    "bass": 0,
    "mid": 0,
    "treble": 0
  },
  "amplifierGain": 15,
  "espModel": 0,
  "volumeCurve": 1,
  "clippingProtection": true,
  "timestamp": 1709848800000
}
```

**Mapeamento de Enums:**
- `espModel`: 0=DevKit V1, 1=AudioKit, 2=S3 DevKit, 3=Generic
- `volumeCurve`: 0=Linear, 1=Exponential Soft, 2=Exponential Aggressive

---

## 🎓 Exemplos de Uso

### Exemplo 1: Totem em Ambiente Silencioso

```
Volume: 3/10
Graves: -3dB
Médios: 0dB
Agudos: +2dB
Ganho: 3dB (Baixo)
Curva: Linear
Proteção: ON
```

### Exemplo 2: Totem em Ambiente Ruidoso

```
Volume: 10/10
Graves: +6dB
Médios: +3dB
Agudos: +6dB
Ganho: 15dB (Alto)
Curva: Exponencial Agressiva
Proteção: OFF
```

### Exemplo 3: Áudio Limpo e Balanceado

```
Volume: 8/10
Graves: 0dB
Médios: 0dB
Agudos: 0dB
Ganho: 15dB
Curva: Exponencial Suave
Proteção: ON
```

---

## 🔐 Segurança

- ✅ Apenas o cliente logado pode acessar o equalizador do próprio totem
- ✅ Validação de sessão: `clienteTotemId === :id`
- ✅ Sanitização de dados no backend
- ✅ Limites de valores (volume 0-10, EQ -12 a +12dB)
- ✅ MQTT com retained messages para persistência

---

## 📞 Suporte

Para problemas ou dúvidas:
1. Verificar logs do servidor e ESP32
2. Consultar esta documentação
3. Verificar Firebase Console
4. Testar com MQTT Explorer

---

**Versão:** 1.0.0  
**Data:** 07/03/2026  
**Autor:** Sistema Totem IoT  
**Compatibilidade:** Firmware v4.1.0+, Backend v4.1.0+
