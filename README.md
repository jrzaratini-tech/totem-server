# 🎛️ TOTEM INTERATIVO IoT v4.0 (PROFISSIONAL)

Servidor Node.js (Express) para totens IoT com:

- Rota pública via QR Code (`/totem/:id`)
- Validação de assinatura por data (Firestore)
- Acionamento físico via MQTT (`play`)
- Dashboard Admin (CRUD de totens)
- Dashboard do Cliente (upload/gravação de áudio)
- API para ESP32 consultar áudio (`/api/audio/:id`)

🔗 **Acesso (produção):** https://totem-server.onrender.com

## 📌 Visão geral

O **Totem Interativo IoT** é uma solução física para eventos e pontos de venda que permite gerar engajamento em redes sociais de forma automatizada.

Fluxo padrão:

1. Usuário escaneia QR Code (`/totem/:id`)
2. Servidor valida assinatura (Firebase Firestore) via `dataExpiracao`
3. Se ativo: publica MQTT (`totem/{id}` = `play`) e redireciona para o Instagram
4. Se expirado: redireciona para `/expirado`

## 🏗️ Arquitetura (ponta a ponta)

```text
Usuário → QR Code → Express (/totem/:id)
                 → Firestore (totens/{id})
                 → MQTT (totem/{id} = play)
                 → ESP32 (ação física)
                 → Redirect Instagram
```

## Componentes

### Backend

- **Node.js / Express** (`server/server.js`)
- **Firebase Admin**
  - Firestore (dados do totem/assinatura)
  - Storage (armazenamento do áudio, quando disponível)
- **MQTT** (HiveMQ público: `broker.hivemq.com:1883`)

### Dashboards

- **Admin**: `/admin/login`
  - Senha fixa (no código): `159268`
- **Cliente**: `/cliente/login`
  - Login: `totemId`
  - Senha: **últimos 4 caracteres do ID + `2026`** (ex.: `TOTEM47` → `47472026`)

## Estrutura do projeto (inventário real do repositório)

```text
totem-server/
├── .github/
│   └── workflows/
├── docs/
│   ├── ESP32.txt
│   ├── manual-firmware.md
│   ├── manual-instalacao.md
│   └── api-reference.md
├── firmware/
│   ├── src/
│   ├── include/
│   ├── test/
│   ├── platformio.ini
│   ├── partitions.csv
│   └── README.md
├── .gitignore
├── .env.example
├── README.md
├── deploy.bat
├── firebase-credentials.json.json
├── package-lock.json
├── package.json
└── server/
    ├── server.js
    ├── routes/
    │   └── audio.js
    ├── middlewares/
    │   └── auth.js
    ├── views/
    │   ├── admin.html
    │   ├── cliente-dashboard.html
    │   ├── cliente-login.html
    │   └── ...
    ├── clientes/            # Fallback
    └── uploads/             # Áudios (runtime)
```

### O que cada arquivo/pasta faz

- **`server/server.js`**
  - Entry point do servidor.
  - Implementa rotas públicas, admin, cliente, upload e API do ESP32.

- **`server/views/`**
  - HTMLs servidos pelo Express.
  - `admin.html` recebe as linhas da tabela via placeholder `{{LINHAS_TABELA}}`.
  - `cliente-dashboard.html` recebe placeholders `{{ID}}`, `{{LINK}}`, `{{DATA_EXPIRACAO}}`, `{{AUDIO_NOME}}`, `{{AUDIO_DATA}}`.

- **`server/clientes/`**
  - Compatibilidade/fallback: arquivos `<id>.txt` com link do Instagram.
  - Usado quando Firebase não está disponível.

- **`server/routes/audio.js`**
  - Rotas de API de áudio em formato de `express.Router()`.
  - **Observação:** hoje o `server/server.js` já implementa as rotas de áudio diretamente (ex.: `GET /api/audio/:id`).
    Se você quiser padronizar, dá para montar este router em `app.use('/api/audio', require('./routes/audio'))`.

- **`server/middlewares/auth.js`**
  - Middlewares de autenticação (admin/cliente) e logger.
  - **Observação:** no `server/server.js` a autenticação também está implementada localmente (função `adminAuth`) e o logger é embutido.

- **`firebase-credentials.json.json`**
  - Arquivo de credenciais do Firebase (Service Account).
  - **Não deve ser commitado**.
  - O `server/server.js` procura `./firebase-credentials.json` (sem o `.json` duplicado) ou a env var `FIREBASE_CREDENTIALS`.

- **`server/uploads/`**
  - **Gerada em runtime**.
  - O `server/server.js` cria a pasta automaticamente e serve estático em `/uploads`.

## Configuração

### Variáveis de ambiente

- **`PORT`**: porta do servidor (default `3000`).
- **`SESSION_SECRET`**: segredo de sessão (default: `totem-secret-key-v4`).
- **`SERVER_URL`**: URL pública do servidor (default: `https://totem-server.onrender.com`).
- **`FIREBASE_CREDENTIALS`**: JSON string com service account (recomendado em produção).

### Firebase / Firestore

Coleção esperada:

```text
totens/{ID_DO_TOTEM}
  link: string
  dataExpiracao: string (YYYY-MM-DD)
  status: string
  criadoEm: timestamp
  ultimaRenovacao: timestamp
  audio: {
    nome: string
    url: string
    dataUpload: string (ISO)
    tamanho: number
    storagePath: string
  }
```

## MQTT

- **Broker**: `broker.hivemq.com`
- **Porta**: `1883`
- **Tópicos**:
  - `totem/{ID}`: recebe `play` (acionamento)
  - `totem/{ID}/audio`: notificação JSON para “atualizar”

## Áudio (upload/gravação) — compatibilidade

- O **Dashboard do Cliente** permite:
  - upload de arquivo (ex.: `.mp3`)
  - **gravação no navegador** (ex.: `.webm/.ogg`) e envio ao backend
- O backend converte automaticamente formatos gravados para **MP3** via `ffmpeg-static` + `fluent-ffmpeg`.

## Rotas HTTP (implementadas no `server/server.js`)

### Públicas

- **`GET /`**: redireciona para `/admin/login`.
- **`GET /totem/:id`**:
  - Busca dados do totem (Firestore; fallback `server/clientes/<id>.txt`).
  - Se `dataExpiracao` expirou: redireciona para `/expirado`.
  - Se ativo: publica MQTT e redireciona para o Instagram.
- **`GET /expirado`**: página `views/expirado.html`.

### Admin

- **`GET /admin/login`**: login.
- **`POST /admin/login`**: valida senha fixa e cria sessão.
- **`GET /admin/dashboard`**: lista totens e mostra status.
- **`GET /admin/novo`** / **`POST /admin/novo`**: cria totem.
- **`GET /admin/editar/:id`** / **`POST /admin/editar/:id`**: edita totem.
- **`GET /admin/excluir/:id`**: exclui totem.
- **`GET /admin/logout`**: encerra sessão.

### Cliente

- **`GET /cliente/login`**: login do cliente.
- **`POST /cliente/login`**: valida senha (últimos 4 + 2026) e cria sessão.
- **`GET /cliente/dashboard/:id`**: dashboard do cliente.
- **`POST /cliente/audio/:id`**:
  - Upload/gravação de áudio.
  - Limite atual: **10MB**.
  - Tipos aceitos: por mimetype e/ou extensão (MP3 e alguns formatos comuns).

### API (ESP32)

- **`GET /api/audio/:id`**: retorna JSON com `url`, `nome`, `versao`, `tamanho`.
- **`DELETE /api/audio/:id`**: remove áudio personalizado (autorização por sessão).

## ▶️ Como rodar localmente

```bash
npm install
npm run start
```

Para desenvolvimento com reload automático:

```bash
npm run dev
```

Admin local:

- `http://localhost:3000/admin/login`

## Deploy

- `deploy.bat`: automatiza `git add/commit/push`.
- Em produção (Render), prefira `FIREBASE_CREDENTIALS` via env var.

## ESP32 (hardware) — pinagem e ligações

Esta seção documenta as ligações do **firmware ESP32** que conversa com este servidor (via HTTP + MQTT) e controla periféricos (SD, áudio I2S, LEDs e botões capacitivos).

### SD Card (SPI)

```text
GPIO05  CS
GPIO23  MOSI
GPIO19  MISO
GPIO18  SCK
```

### Botões capacitivos TTP223

```text
GPIO15  COR
GPIO04  MAIS
GPIO33  MENOS
GPIO32  CORAÇÃO (DISPARO MANUAL)
```

### LEDs

```text
GPIO12  Fita LED Principal
GPIO13  Fita LED Cardíaco
```

### I2S — MAX98357

```text
GPIO26  BCLK
GPIO25  LRC
GPIO22  DIN
```

### Potenciômetro (volume)

```text
GPIO34  Wiper (volume)
```

### Alimentação

```text
5V    MAX98357 (VIN)
5V    SD Card (VCC)
3.3V  TTP223 (VCC) + potenciômetro
GND   GND geral (todos)
```

### Outros sinais

```text
GPIO35  Reset WiFi (botão físico; ao pressionar conecta ao GND — ativo em LOW)
```

## 📌 Observações importantes

- **Firmware/ESP32**: o firmware **está neste repositório** em `firmware/`.
  - O `Totem ID` e o `token` podem ser **provisionados via portal AP/NVS** (não precisa recompilar por cliente).
  - Downloads (áudio/OTA) exigem **HTTPS** e `ROOT_CA_PEM` configurado no `firmware/include/Config.h`.
  - O ESP32 envia `X-Device-Token` (quando configurado) nas requisições HTTP para permitir validação no backend.
- **Credenciais Firebase**: nunca versionar service account.
- **Recomendação**: manter credenciais fora do Git e fornecer via `FIREBASE_CREDENTIALS` (env var). O `server/server.js` aceita `firebase-credentials.json` e também `firebase-credentials.json.json`.
- **Script `migrate`**: o `package.json` referencia `migrate_to_v4.js`, mas este arquivo não está no repositório no momento.