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

Fluxo padrão (ponta a ponta):

1. Usuário escaneia QR Code (`/totem/:id`)
2. Backend busca o documento do totem (Firestore; ou fallback local em `server/clientes/<id>.txt`)
3. Backend valida a assinatura por data (`dataExpiracao`)
4. Se ativo: publica um comando via MQTT e redireciona para o link configurado (Instagram)
5. Se expirado: redireciona para `/expirado`

## 🏗️ Arquitetura (ponta a ponta)

```text
Usuário → QR Code → Express (/totem/:id)
                 → Firestore (totens/{id})
                 → MQTT (totem/{id} = play)
                 → ESP32 (ação física)
                 → Redirect Instagram
```

## ✅ Status do projeto (o que está pronto de verdade)

- **Backend**
  - Express + sessões
  - Integração com Firebase Admin (Firestore + Storage)
  - Fallback sem Firebase (arquivos `.txt`)
  - Upload/gravação de áudio com conversão para MP3 (FFmpeg)
  - MQTT (HiveMQ público)
- **Front (views HTML)**
  - Admin (login + CRUD)
  - Cliente (login + dashboard + áudio + configuração de LEDs/efeitos)
- **Firmware (ESP32)**
  - Firmware modular em `firmware/` (PlatformIO)
  - Exemplo legado em `docs/ESP32.txt` (Arduino IDE / NeoPixel)

> Observação: os arquivos em `docs/` estão parcialmente “Em construção”. Este `README.md` documenta o que está implementado no código atual.

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

## ⚠️ Segurança (importante)

- A autenticação do Admin e Cliente é **simples** e baseada em senha fixa/regra (não é multiusuário).
- Se você for expor isso publicamente para terceiros, o recomendado é:
  - trocar senha fixa por usuário/senha no Firestore
  - habilitar rate limit
  - proteger rotas sensíveis com autenticação real (JWT/OAuth)
  - trocar o broker MQTT público por um broker privado e autenticado

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

- **`firmware/include/span`**
  - Polyfill mínimo de `std::span` para toolchains que não possuem o header `<span>`.
  - Necessário para compilar a lib `ESP32-audioI2S` com o toolchain padrão do PlatformIO para ESP32 (GCC 8.x).

## Configuração

### Variáveis de ambiente

- **`PORT`**: porta do servidor (default `3000`).
- **`SESSION_SECRET`**: segredo de sessão (default: `totem-secret-key-v4`).
- **`SERVER_URL`**: URL pública do servidor (default: `https://totem-server.onrender.com`).
- **`FIREBASE_CREDENTIALS`**: JSON string com service account (recomendado em produção).

Exemplo (.env):

```bash
PORT=3000
SERVER_URL=http://localhost:3000
SESSION_SECRET=change-me
FIREBASE_CREDENTIALS=
```

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

Requisitos no Firebase:

- **Service Account**
  - Em produção, prefira setar **`FIREBASE_CREDENTIALS`** como env var.
  - Localmente, você também pode colocar:
    - `firebase-credentials.json` (preferível)
    - `firebase-credentials.json.json` (suportado por compatibilidade)
- **Storage**
  - O código usa o bucket padrão: `${project_id}.appspot.com`.
  - O upload tenta `makePublic()` no arquivo, então você precisa permitir isso no seu projeto/bucket.

## MQTT

- **Broker**: `broker.hivemq.com`
- **Porta**: `1883`
- **Tópicos**:
  - `totem/{ID}`: recebe `play` (acionamento)
  - `totem/{ID}/audio`: notificação JSON para “atualizar”
  - `totem/{ID}/configUpdate`: atualização de configuração (retained)

### Nota importante sobre compatibilidade de tópicos (firmware)

Existem **duas referências de firmware** neste repositório:

- **Firmware modular (em `firmware/`)**
  - Usa tópicos do tipo:
    - `totem/{id}/trigger` (play)
    - `totem/{id}/audioUpdate`
    - `totem/{id}/configUpdate`
    - `totem/{id}/firmwareUpdate`
    - `totem/{id}/status`
- **Exemplo legado (em `docs/ESP32.txt`)**
  - Escuta `totem/{id}` e `totem/{id}/audio`

O **backend atual** publica:

- `totem/{id}` com payload `play`
- `totem/{id}` com payload `atualizar_audio`
- `totem/{id}/audio` com JSON `{"comando":"atualizar", ...}`
- `totem/{id}/configUpdate` com JSON de config

Se você estiver usando o firmware modular, ajuste os tópicos (no firmware ou no backend) para ficarem iguais.

### Recomendação (para evitar incompatibilidades)

Se você está usando o firmware em `firmware/`, o caminho mais previsível é **padronizar o backend** para publicar também nos tópicos do firmware:

- `totem/{id}/trigger` com payload `play`
- `totem/{id}/audioUpdate` com payload/JSON de “atualizar”

Assim você não precisa manter “compatibilidade dupla” (firmware legado vs modular).

## Áudio (upload/gravação) — compatibilidade

- O **Dashboard do Cliente** permite:
  - upload de arquivo (ex.: `.mp3`)
  - **gravação no navegador** (ex.: `.webm/.ogg`) e envio ao backend
- O backend converte automaticamente formatos gravados para **MP3** via `ffmpeg-static` + `fluent-ffmpeg`.

Limites atuais:

- **10MB** por upload
- Tipos aceitos: MP3, WebM, OGG, WAV, M4A/MP4, AAC (por mimetype e/ou extensão)

## Rotas HTTP (implementadas no `server/server.js`)

### Públicas

- **`GET /`**: redireciona para `/admin/login`.
- **`GET /totem/:id`**:
  - Busca dados do totem (Firestore; ou fallback local em `server/clientes/<id>.txt`)
  - Se `dataExpiracao` expirou: redireciona para `/expirado`
  - Se ativo: publica um comando via MQTT e redireciona para o link configurado (Instagram)
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
- **`POST /cliente/config/:id`**: salva configuração de LEDs/efeitos no Firestore e publica via MQTT (`totem/{id}/configUpdate`).

### API (ESP32)

- **`GET /api/audio/:id`**: retorna JSON com `url`, `nome`, `versao`, `tamanho`.
- **`DELETE /api/audio/:id`**: remove áudio personalizado (autorização por sessão).
- **`GET /api/config/:id`**: consulta configuração atual no Firestore (debug).

## ▶️ Como rodar localmente

Pré-requisitos:

- Node.js **>= 18**
- Git

Passos:

```bash
npm install
npm run start
```

Para desenvolvimento com reload automático:

```bash
npm run dev
```

URLs úteis (local):

- Admin: `http://localhost:3000/admin/login`
- Cliente: `http://localhost:3000/cliente/login`

> O arquivo `server.js` na raiz apenas faz `require('./server/server.js')`.

## Deploy

- `deploy.bat`: automatiza `git add/commit/push`.
- Em produção (Render), prefira `FIREBASE_CREDENTIALS` via env var.

Checklist rápido (Render/outro PaaS):

- Definir `PORT` (se necessário)
- Definir `SERVER_URL` com o domínio final
- Definir `SESSION_SECRET`
- Definir `FIREBASE_CREDENTIALS` (JSON em uma linha)

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

## 🧩 Firmware ESP32 (PlatformIO) — como compilar e gravar

Pré-requisitos:

- VS Code + extensão PlatformIO
  - ou `pio` instalado

Comandos (dentro de `firmware/`):

```bash
pio run
pio run -t upload
pio device monitor
```

Arquivos relevantes:

- `firmware/platformio.ini`: define libs (`FastLED`, `ESP32-audioI2S`, `ESP Async WebServer`, etc.)
- `firmware/include/Config.h`: pinos, URL do servidor, MQTT, watchdog, HTTPS CA
- `firmware/src/main.cpp`: integra state machine, WiFi/AP, MQTT, áudio, LEDs, botões, OTA

### Dependências do firmware (PlatformIO)

As dependências são resolvidas via `firmware/platformio.ini`. Os identificadores abaixo são os **compatíveis com o registry do PlatformIO** (evita `UnknownPackageError`):

```ini
lib_deps =
    ESP32-audioI2S
    esp32async/ESPAsyncWebServer
    esp32async/AsyncTCP
    FastLED
    ArduinoJson
    PubSubClient
```

> Observação: existem múltiplos forks de `AsyncTCP` no registry. Usar `esp32async/AsyncTCP` evita que o PlatformIO puxe bibliotecas incorretas (ex.: `RPAsyncTCP`, que é para RP2040 e não compila no ESP32).

### C++ standard / flags

Este firmware usa recursos modernos e algumas libs exigem um padrão de C++ mais novo. O `platformio.ini` está configurado para:

```ini
build_unflags = -std=gnu++20
build_flags = -std=gnu++2a -fpermissive
```

- `-std=gnu++2a`: mantém compatibilidade com o toolchain GCC 8.x do ESP32 no PlatformIO.
- `-fpermissive`: aplicado como *workaround* para diferenças de const-correctness entre `ESPAsyncWebServer` e `AsyncTCP` em algumas combinações de versões.

### `ESP32-audioI2S` e erro `fatal error: span: No such file or directory`

Se o build falhar com:

```text
fatal error: span: No such file or directory
```

Isso ocorre porque o toolchain padrão (GCC 8.x) não fornece `<span>`. Este repositório inclui um polyfill mínimo em `firmware/include/span`, o que permite compilar sem trocar o toolchain.

## 🔐 HTTPS no ESP32 (CA raiz)

O backend de produção e URLs do Storage normalmente são **HTTPS**.

- Se você não configurar o CA em `ROOT_CA_PEM` (em `firmware/include/Config.h`), downloads podem falhar.
- O arquivo `docs/ESP32.txt` usa `setInsecure()` (menos seguro) apenas como exemplo/atalho.

## 🧯 Troubleshooting

### Firebase não conecta

- Verifique se você forneceu `FIREBASE_CREDENTIALS`.
- Verifique se o JSON é válido (sem quebras de linha).
- Se você usa arquivo local, confira se ele está com o nome correto:
  - `firebase-credentials.json` (preferível)
  - `firebase-credentials.json.json` (compat)

### Upload de áudio falha / conversão não roda

- O backend depende de `ffmpeg-static`.
- Se você ver erro “FFmpeg não encontrado”, rode `npm install` novamente.

### ESP32 não toca ou não atualiza áudio

- Confirme que o tópico MQTT do firmware bate com o tópico publicado pelo backend (ver seção MQTT).
- Confirme que `SERVER_URL` no firmware aponta para o servidor correto.
- Se o download for HTTPS:
  - configure `ROOT_CA_PEM`, ou (apenas para teste) use abordagem insegura.

### PlatformIO: `UnknownPackageError` ao instalar libs

Se aparecer `UnknownPackageError` para libs como `ESP32-audioI2S` ou `ESP Async WebServer`, confira se o `lib_deps` está usando o **nome do registry** (ver seção “Dependências do firmware”).

Exemplos que costumam falhar:

- `schreibfaul1/ESP32-audioI2S`
- `me-no-dev/ESP Async WebServer @ ^1.2.4`

Exemplos corretos:

- `ESP32-audioI2S`
- `esp32async/ESPAsyncWebServer`

### PlatformIO: erro envolvendo `RPAsyncTCP` / `ip_addr_t` / lwIP

Se o build baixar `RPAsyncTCP` e falhar com erros do tipo `ip_addr_t has no member named addr`, significa que uma variante errada de AsyncTCP foi resolvida. Fix:

- use `esp32async/AsyncTCP` no `lib_deps`.

### PlatformIO: erro `passing 'const ...' discards qualifiers` (AsyncWebServer/AsyncTCP)

Algumas versões da stack async apresentam inconsistências de `const`. Caso isso bloqueie o build:

- mantenha `-fpermissive` em `build_flags`.

### Cliente não consegue logar

- A senha é **últimos 4 caracteres do ID + `2026`**.