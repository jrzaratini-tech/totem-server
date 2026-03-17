# 🚀 Guia Completo de OTA - Totem Server v4.3.0

## 📋 Índice

1. [Visão Geral](#visão-geral)
2. [Arquitetura](#arquitetura)
3. [Configuração Inicial](#configuração-inicial)
4. [Uso do Dashboard Admin](#uso-do-dashboard-admin)
5. [Atualização Individual](#atualização-individual)
6. [Atualização em Massa](#atualização-em-massa)
7. [Rollback](#rollback)
8. [Monitoramento](#monitoramento)
9. [Troubleshooting](#troubleshooting)
10. [API Reference](#api-reference)

---

## 🎯 Visão Geral

O sistema OTA (Over-The-Air) permite atualizar o firmware dos totens ESP32-S3 remotamente através do dashboard admin, sem necessidade de acesso físico aos dispositivos.

### Características Principais

- ✅ Upload e validação automática de firmware
- ✅ Atualização individual ou em massa
- ✅ Agendamento de atualizações
- ✅ Monitoramento em tempo real
- ✅ Histórico de versões por totem
- ✅ Rollback para versões anteriores
- ✅ Validação de integridade (checksum MD5)
- ✅ URLs assinadas com expiração (1 hora)
- ✅ Rate limiting (5 uploads/hora)
- ✅ Logs de auditoria completos

---

## 🏗️ Arquitetura

```
┌─────────────────────────────────────────────────────┐
│ DASHBOARD ADMIN (admin-ota.html)                    │
│ • Upload firmware (.bin)                             │
│ • Seleção individual/massa                          │
│ • Progresso em tempo real                            │
│ • Histórico de versões                               │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────┐
│ BACKEND (Node.js/Express)                            │
│ • routes/ota.js - Rotas CRUD                        │
│ • utils/firmwareValidator.js - Validação            │
│ • services/firebaseOTA.js - Storage/Firestore       │
│ • services/mqttOTA.js - Publicação MQTT             │
│ • services/jobManager.js - Jobs em massa            │
└────────────────────┬────────────────────────────────┘
         ↓                       ↓
┌────────────────┐      ┌──────────────────────────┐
│ FIREBASE       │      │ MQTT Broker (HiveMQ)     │
│ • Storage      │      │ Tópico: totem/{id}/ota   │
│ • Firestore    │      │ QoS: 1 (garantido)       │
└────────┬───────┘      └───────────┬──────────────┘
         │                          ↓
         └─────────────►┌──────────────────────────┐
                        │ ESP32-S3 (OTAManager.h)  │
                        │ • Recebe notificação     │
                        │ • Download via HTTPS     │
                        │ • Validação checksum     │
                        │ • Atualização + reboot   │
                        │ • Rollback automático    │
                        └──────────────────────────┘
```

---

## ⚙️ Configuração Inicial

### 1. Firestore - Estrutura de Dados

Certifique-se de que seus documentos de totens tenham a estrutura:

```javascript
// Coleção: totens/{totemId}
{
  "id": "totem123",
  "link": "https://instagram.com/...",
  "dataExpiracao": "2024-12-31",
  
  // Novo campo para OTA
  "firmware": {
    "atual": "v4.2.1",
    "status": "online",  // online, updating, offline, failed
    "versaoDisponivel": "v4.3.0",
    "urlDownload": "https://storage.googleapis.com/...",
    "checksum": "abc123...",
    "tamanho": 1024000,
    "storagePath": "firmware/totem123/v4.3.0.bin",
    "ultimaAtualizacao": "2024-03-16T10:00:00Z",
    "ultimoBoot": "2024-03-16T09:00:00Z",
    "historico": [
      {
        "versao": "v4.2.1",
        "data": "2024-03-15T10:00:00Z",
        "status": "success",
        "checksum": "def456...",
        "url": "https://storage.googleapis.com/..."
      }
    ]
  }
}
```

### 2. Firebase Storage

O firmware é armazenado em:
```
/firmware/{totemId}/{versao}.bin
```

Exemplo:
```
/firmware/totem123/v4.3.0.bin
/firmware/totem456/v4.3.0.bin
```

### 3. Coleção de Jobs (ota_jobs)

```javascript
{
  "id": "job_1234567890_abc",
  "status": "in_progress",  // pending, in_progress, completed, failed, scheduled
  "totens": ["totem123", "totem456"],
  "versao": "v4.3.0",
  "progresso": {
    "total": 10,
    "concluidos": 5,
    "falhas": 1,
    "emAndamento": 4
  },
  "agendamento": "2024-03-20T03:00:00Z",  // opcional
  "criadoEm": "2024-03-16T10:00:00Z",
  "criadoPor": "admin@email.com",
  "resultados": [
    {
      "totemId": "totem123",
      "status": "success",
      "mensagem": "OTA enviado com sucesso",
      "timestamp": "2024-03-16T10:05:00Z"
    }
  ]
}
```

---

## 🖥️ Uso do Dashboard Admin

### Acessar Interface OTA

1. Faça login no admin: `https://seu-servidor.com/admin/login`
2. Acesse: `https://seu-servidor.com/admin/ota`

### Upload de Firmware

1. **Preparar arquivo firmware**:
   - Formato: `.bin`
   - Nome sugerido: `firmware_vX.X.X.bin` (ex: `firmware_v4.3.0.bin`)
   - Tamanho máximo: 4MB
   - Magic number: `0xE9` (ESP32)

2. **Fazer upload**:
   - Arraste o arquivo para a área de upload OU
   - Clique na área para selecionar arquivo
   - Sistema valida automaticamente:
     - Magic number ESP32
     - Tamanho (100KB - 4MB)
     - Checksum MD5
     - Extração de versão

3. **Validação bem-sucedida**:
   ```
   ✅ Firmware validado com sucesso!
   Versão: v4.3.0
   Tamanho: 1.2 MB
   Checksum: abc123def456...
   ```

---

## 📱 Atualização Individual

### Passo a Passo

1. **Selecionar totem**:
   - Na tabela, clique em "Atualizar" no totem desejado

2. **Confirmar dados**:
   ```
   Totem ID: totem123
   Versão do Firmware: v4.3.0
   Tamanho: 1.2 MB
   ```

3. **Iniciar atualização**:
   - Clique em "Confirmar Atualização"
   - Sistema executa:
     - Upload para Firebase Storage
     - Geração de URL assinada (válida por 1h)
     - Salvamento de metadados no Firestore
     - Publicação MQTT no tópico `totem/{id}/ota`

4. **Monitorar progresso**:
   - Status muda para "UPDATING"
   - ESP32 recebe notificação
   - Download e instalação automáticos
   - Reboot após sucesso

### Exemplo de Payload MQTT

```json
{
  "comando": "ota",
  "versao": "v4.3.0",
  "url": "https://storage.googleapis.com/...",
  "checksum": "abc123def456",
  "tamanho": 1024000,
  "timestamp": 1710586800000
}
```

---

## 🚀 Atualização em Massa

### Uso Básico

1. **Fazer upload do firmware** (se ainda não fez)

2. **Clicar em "Atualização em Massa"**

3. **Selecionar totens**:
   - Use checkboxes para selecionar múltiplos totens
   - Visualize versão atual de cada um

4. **Opções**:
   - **Imediato**: Inicia imediatamente
   - **Agendado**: Marque "Agendar atualização" e escolha data/hora

5. **Iniciar**:
   - Clique em "Iniciar Atualização"
   - Sistema cria job e processa sequencialmente

### Exemplo de Agendamento

```javascript
// Agendar para 20/03/2024 às 03:00 AM
{
  "totens": ["totem1", "totem2", "totem3"],
  "versao": "v4.3.0",
  "agendamento": "2024-03-20T03:00:00Z"
}
```

### Monitoramento de Job

O dashboard mostra em tempo real:

```
Job: job_1710586800_abc
Status: EM ANDAMENTO

[████████████░░░░░░░░] 60%

Total: 10
Concluídos: 6
Falhas: 1
```

Atualização automática a cada 3 segundos.

---

## ⏮️ Rollback

### Quando Usar

- Firmware novo apresenta bugs
- Incompatibilidade detectada
- Necessidade de reverter para versão estável

### Como Fazer

1. **Acessar histórico**:
   - Clique em "Histórico" no totem desejado

2. **Visualizar versões**:
   ```
   Versão Atual: v4.3.0
   
   Histórico:
   ✓ v4.3.0 - 16/03/2024 10:00 - success
   ✓ v4.2.1 - 15/03/2024 14:00 - success
   ✓ v4.2.0 - 10/03/2024 09:00 - success
   ```

3. **Selecionar versão anterior**:
   - Clique em "Rollback" na versão desejada
   - Confirme a ação

4. **Sistema executa**:
   - Gera nova URL assinada para versão antiga
   - Publica comando de rollback via MQTT
   - ESP32 baixa e instala versão anterior

### Rollback Automático (ESP32)

O firmware ESP32 possui rollback automático em caso de falha:

```cpp
// Se o boot falhar após OTA
void verificarBootValido() {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        // Valida firmware após 5 segundos
        delay(5000);
        
        if (tudo_ok) {
            esp_ota_mark_app_valid_cancel_rollback();
        } else {
            // Rollback automático para partição anterior
            esp_ota_set_boot_partition(partAnterior);
            ESP.restart();
        }
    }
}
```

---

## 📊 Monitoramento

### Status dos Totens

| Status | Descrição | Ação |
|--------|-----------|------|
| `online` | Funcionando normalmente | - |
| `updating` | Atualização em andamento | Aguardar |
| `offline` | Sem conexão | Verificar conectividade |
| `failed` | Falha na atualização | Verificar logs, tentar novamente |

### Logs de Auditoria

Todas as ações OTA são registradas em `ota_auditoria`:

```javascript
{
  "acao": "ota_individual",
  "usuario": "admin@email.com",
  "detalhes": {
    "totemId": "totem123",
    "versao": "v4.3.0",
    "ip": "192.168.1.100"
  },
  "timestamp": "2024-03-16T10:00:00Z"
}
```

### Verificar Status de Job

```bash
GET /admin/ota/job/{jobId}
```

Resposta:
```json
{
  "success": true,
  "job": {
    "id": "job_123",
    "status": "in_progress",
    "progresso": {
      "total": 10,
      "concluidos": 6,
      "falhas": 1,
      "emAndamento": 3
    }
  }
}
```

---

## 🔧 Troubleshooting

### Problema: Upload falha com "Magic number inválido"

**Causa**: Arquivo não é um firmware ESP32 válido

**Solução**:
- Verifique se compilou para ESP32-S3
- Use PlatformIO ou Arduino IDE
- Certifique-se de exportar o `.bin` correto

### Problema: Totem não recebe atualização

**Causa**: MQTT desconectado ou totem offline

**Solução**:
1. Verificar conexão MQTT do servidor:
   ```bash
   # Logs do servidor
   ✅ MQTT conectado ao broker HiveMQ
   ```

2. Verificar conexão do totem:
   ```cpp
   // Serial do ESP32
   ✅ MQTT conectado
   ✅ Inscrito em totem/totem123/ota
   ```

3. Tentar novamente após reconexão

### Problema: Download falha no ESP32

**Causa**: URL expirada (>1 hora) ou sem HTTPS

**Solução**:
- URLs assinadas expiram em 1 hora
- Refazer atualização para gerar nova URL
- Verificar se Firebase Storage está público

### Problema: OTA falha e ESP32 não faz rollback

**Causa**: Partição OTA corrompida

**Solução**:
1. Conectar via USB
2. Flash manual do firmware:
   ```bash
   esptool.py --port COM3 write_flash 0x10000 firmware.bin
   ```

### Problema: Rate limit atingido

**Causa**: Mais de 5 uploads em 1 hora

**Solução**:
- Aguardar 1 hora
- Ou usar firmware já validado anteriormente

---

## 📚 API Reference

### POST /admin/ota/upload

Upload e validação de firmware.

**Request**:
```
Content-Type: multipart/form-data
firmware: [arquivo .bin]
```

**Response**:
```json
{
  "success": true,
  "firmware": {
    "versao": "v4.3.0",
    "tamanho": 1024000,
    "tamanhoMB": "1.00",
    "checksum": "abc123",
    "nomeArquivo": "firmware_v4.3.0.bin",
    "path": "/uploads/firmware/firmware_123.bin"
  }
}
```

### POST /admin/ota/individual/:totemId

Atualizar um totem específico.

**Request**:
```json
{
  "versao": "v4.3.0",
  "firmwarePath": "/uploads/firmware/firmware_123.bin",
  "checksum": "abc123",
  "tamanho": 1024000
}
```

**Response**:
```json
{
  "success": true,
  "message": "OTA iniciado para totem123",
  "totemId": "totem123",
  "versao": "v4.3.0"
}
```

### POST /admin/ota/massa

Atualizar múltiplos totens.

**Request**:
```json
{
  "totens": ["totem1", "totem2", "totem3"],
  "versao": "v4.3.0",
  "firmwarePath": "/uploads/firmware/firmware_123.bin",
  "checksum": "abc123",
  "tamanho": 1024000,
  "agendamento": "2024-03-20T03:00:00Z"  // opcional
}
```

**Response**:
```json
{
  "success": true,
  "jobId": "job_1710586800_abc",
  "message": "Job criado para 3 totens",
  "total": 3
}
```

### GET /admin/ota/job/:jobId

Obter status de job em massa.

**Response**:
```json
{
  "success": true,
  "job": {
    "id": "job_123",
    "status": "in_progress",
    "totens": ["totem1", "totem2"],
    "versao": "v4.3.0",
    "progresso": {
      "total": 10,
      "concluidos": 6,
      "falhas": 1,
      "emAndamento": 3
    },
    "resultados": [...]
  }
}
```

### GET /admin/ota/status/:totemId

Obter status OTA de um totem.

**Response**:
```json
{
  "success": true,
  "totemId": "totem123",
  "firmware": {
    "versaoAtual": "v4.3.0",
    "versaoDisponivel": null,
    "status": "online",
    "ultimaAtualizacao": "2024-03-16T10:00:00Z",
    "ultimoBoot": "2024-03-16T09:00:00Z"
  }
}
```

### POST /admin/ota/rollback/:totemId

Fazer rollback para versão anterior.

**Request**:
```json
{
  "versao": "v4.2.1"
}
```

**Response**:
```json
{
  "success": true,
  "totemId": "totem123",
  "versao": "v4.2.1"
}
```

### GET /admin/ota/versoes/:totemId

Listar histórico de versões.

**Response**:
```json
{
  "success": true,
  "versaoAtual": "v4.3.0",
  "historico": [
    {
      "versao": "v4.3.0",
      "data": "2024-03-16T10:00:00Z",
      "status": "success",
      "checksum": "abc123",
      "url": "https://..."
    }
  ]
}
```

---

## 🔒 Segurança

### Medidas Implementadas

1. **Autenticação**: Apenas admin autenticado pode acessar rotas OTA
2. **Validação de firmware**: Magic number, tamanho, checksum
3. **URLs assinadas**: Expiram em 1 hora
4. **Rate limiting**: Máximo 5 uploads/hora por usuário
5. **Logs de auditoria**: Todas as ações registradas
6. **Verificação de totem**: Valida existência antes de enviar
7. **HTTPS obrigatório**: ESP32 requer HTTPS para download

### Boas Práticas

- ✅ Testar firmware em ambiente de desenvolvimento primeiro
- ✅ Fazer backup da versão atual antes de atualizar
- ✅ Agendar atualizações em massa para horários de baixo uso
- ✅ Monitorar logs durante atualizações
- ✅ Manter histórico de versões estáveis
- ✅ Documentar mudanças entre versões

---

## 📞 Suporte

Para problemas ou dúvidas:
- Verifique os logs do servidor
- Consulte a seção Troubleshooting
- Verifique logs seriais do ESP32
- Entre em contato com o administrador do sistema

---

**Totem Server v4.3.0** - Sistema OTA Completo 🚀
