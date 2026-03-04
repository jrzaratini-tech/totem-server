# Totem Server (v4.1.0)

Servidor Node.js/Express para o ecossistema **Totem Interativo IoT**.

Este repositório concentra principalmente:

- **Backend (este projeto)**: painel Admin e painel do Cliente, redirecionamento público `/totem/:id`, API para o firmware (ESP32), integração com **Firebase (Firestore + Storage)**, **MQTT** e upload/conversão de áudio.
- **Firmware (subpasta `firmware/`)**: projeto PlatformIO/Arduino para ESP32 (tópicos MQTT, download de áudio, OTA, efeitos de LED, etc.).

### ✨ Novidades v4.1.0

- **Sistema Dual de Iluminação**: Configure dois modos independentes (espera e disparo)
- **Controle de Volume**: Ajuste o volume do áudio de 0 (mudo) a 10 (máximo)
- **Prévias Visuais**: Simuladores em tempo real para ambos os modos de LED
- **Auto-Play**: Reprodução automática quando novo áudio é enviado ou configuração de trigger é atualizada
- **Limite de Upload**: Reduzido para 5MB para melhor performance
- **Otimização SPIFFS**: Firmware agora deleta áudio antigo antes de baixar novo, evitando erro de espaço

---

## Sumário

- [Visão geral](#visão-geral)
- [Arquitetura (alto nível)](#arquitetura-alto-nível)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Stack / dependências](#stack--dependências)
- [Configuração (variáveis de ambiente)](#configuração-variáveis-de-ambiente)
- [Credenciais do Firebase](#credenciais-do-firebase)
- [Executando localmente](#executando-localmente)
- [Deploy](#deploy)
- [Fluxos do sistema (end-to-end)](#fluxos-do-sistema-end-to-end)
- [Modelos de dados (Firestore)](#modelos-de-dados-firestore)
- [MQTT: tópicos e mensagens](#mqtt-tópicos-e-mensagens)
- [Endpoints e rotas](#endpoints-e-rotas)
  - [Rotas públicas](#rotas-públicas)
  - [Painel do cliente](#painel-do-cliente)
  - [Painel admin](#painel-admin)
  - [API para firmware/ESP32](#api-para-firmwareesp32)
- [Upload de áudio (detalhes)](#upload-de-áudio-detalhes)
- [Compatibilidade / modos de fallback](#compatibilidade--modos-de-fallback)
- [Troubleshooting (problemas comuns)](#troubleshooting-problemas-comuns)
- [Segurança e boas práticas](#segurança-e-boas-práticas)
- [Firmware (ESP32)](#firmware-esp32)

---

## Visão geral

O Totem Server é o ponto central de controle dos totens:

- **Admin** cadastra/edita/exclui totens, define link do Instagram e validade.
- **Cliente** (dono do totem) acessa um painel para:
  - ver link e validade
  - **enviar áudio personalizado** (até 5MB, qualquer formato suportado → o servidor converte para **MP3**)
  - configurar **volume do áudio** (0-10)
  - configurar **dois modos de iluminação independentes**:
    - **Modo Espera**: iluminação contínua quando o totem não está acionado
    - **Modo Disparo**: iluminação sincronizada com o áudio ao acionar o totem
  - visualizar **prévias em tempo real** de cada modo de LED
- **Público** acessa `/totem/:id`:
  - se ativo e não expirado, o servidor publica um comando via MQTT e redireciona para o link (Instagram)
- **Firmware/ESP32** consulta a API `/api/audio/:id` para saber se existe áudio novo e onde baixar.

O servidor opera em dois modos:

- **Com Firebase**: usa Firestore como fonte de verdade e (opcionalmente) Firebase Storage para hospedar o áudio.
- **Sem Firebase (fallback)**: mantém compatibilidade mínima usando arquivos locais em `server/clientes/*.txt`.

---

## Arquitetura (alto nível)

Componentes:

- **HTTP (Express)**
  - Serve páginas HTML (views) do Admin e do Cliente
  - Exponibiliza endpoints de API e páginas públicas
  - Hospeda arquivos estáticos de upload em fallback local (`/uploads`)

- **Firebase Admin SDK**
  - **Firestore**: coleção `totens` (cadastro/validades/link), `config/admin` (senha)
  - **Storage**: guarda áudio em `audios/{totemId}/{filename}` e torna público via `makePublic()`

- **MQTT (HiveMQ público)**
  - Publica comandos como `play` e mensagens JSON para atualização de áudio/config

Fluxo de dados (simplificado):

```text
Navegador Admin/Cliente  -->  Express  -->  Firestore/Storage
                                     -->  MQTT broker  -->  ESP32

Público /totem/:id  --> Express --> MQTT(play) --> ESP32
                    --> Express --> 302 redirect para Instagram

ESP32 --> GET /api/audio/:id --> Express --> Firestore --> resposta (url/versao)
ESP32 --> GET audioUrl (Storage público ou /uploads) --> baixa MP3 para SD
```

---

## Estrutura do repositório

Principais itens na raiz:

- `server/server.js`
  - **Aplicação principal** (Express, sessões, Firebase, MQTT, upload/ffmpeg, rotas).
- `server.js`
  - Apenas delega para `require('./server/server.js')`.
- `.env.example`
  - Exemplo de variáveis de ambiente.
- `firebase-credentials.json.json`
  - Arquivo de credenciais (atenção ao nome – ver [Credenciais do Firebase](#credenciais-do-firebase)).
- `docs/`
  - Documentos auxiliares (alguns estão “Em construção”).
- `server/views/`
  - HTMLs do painel Admin/Cliente e telas auxiliares.
- `server/uploads/`
  - Upload temporário/local de arquivos de áudio (fallback quando Storage não está disponível).
- `server/clientes/`
  - Fallback simples de cadastro de totens via arquivos `ID.txt` contendo somente o link.
- `firmware/`
  - Projeto do firmware ESP32 (PlatformIO).
- `deploy.bat`
  - Script simples para `git add/commit/push`.

---

## Stack / dependências

- **Node.js**: `>= 18` (ver `package.json` → `engines.node`)
- **Servidor HTTP**: Express (`express`)
- **Sessão**: `express-session`
- **Body parsing**: `body-parser`
- **CORS**: `cors`
- **Uploads**: `multer`
- **Conversão de áudio**: `fluent-ffmpeg` + `ffmpeg-static`
- **Firebase**: `firebase-admin`
- **MQTT**: `mqtt`
- **FS utilitário**: `fs-extra`

Scripts NPM:

- `npm start` → `node server/server.js`
- `npm run dev` → `nodemon server/server.js`

---

## Configuração (variáveis de ambiente)

Crie um arquivo `.env` na raiz (mesmo nível de `package.json`) baseado em `.env.example`.

Conteúdo esperado:

- `PORT`
  - Porta do servidor.
  - Padrão no código: `3000`.

- `SERVER_URL`
  - URL pública do servidor.
  - **Usada para montar links de fallback** quando o Firebase Storage não estiver disponível (ex.: `http://localhost:3000`).
  - Padrão no código: `https://totem-server.onrender.com`.

- `SESSION_SECRET`
  - Segredo de sessão do `express-session`.
  - Padrão no código: `totem-secret-key-v4`.

- `FIREBASE_CREDENTIALS`
  - JSON do service account (string única) para inicializar o Firebase Admin.
  - Alternativa ao arquivo de credenciais.

---

## Credenciais do Firebase

O servidor tenta inicializar o Firebase Admin de 3 formas (nessa ordem):

1. Arquivo na raiz: `firebase-credentials.json`
2. Arquivo alternativo na raiz: `firebase-credentials.json.json`
3. Variável de ambiente: `FIREBASE_CREDENTIALS` (conteúdo JSON)

Observações importantes:

- **Este repositório contém** um arquivo chamado `firebase-credentials.json.json`.
- Em ambientes de produção, o recomendado é **não commitar** credenciais e usar `FIREBASE_CREDENTIALS` como secret/variable.

Quando inicializado com sucesso:

- `db = admin.firestore()`
- `bucket = admin.storage().bucket()` (bucket: `${project_id}.appspot.com`)

Se falhar, o servidor segue em modo degradado:

- Alguns recursos continuam (admin/cliente com fallback em arquivo), mas a API do ESP32 e recursos que dependem do Firestore/Storage podem ficar limitados.

---

## Executando localmente

Pré-requisitos:

- Node.js 18+
- (Opcional) Firebase configurado (Firestore + Storage)
- (Opcional) acesso de saída para `broker.hivemq.com:1883`

Passo a passo:

1. Instale dependências:

```bash
npm install
```

2. Crie `.env` na raiz (copie de `.env.example`).

3. (Opcional) Configure credenciais do Firebase:

- Coloque `firebase-credentials.json` na raiz **ou**
- Ajuste `FIREBASE_CREDENTIALS` com o JSON do service account.

4. Suba o servidor:

```bash
npm start
```

ou em modo dev:

```bash
npm run dev
```

URLs úteis (considerando `SERVER_URL=http://localhost:3000`):

- Admin: `http://localhost:3000/admin/login`
- Cliente: `http://localhost:3000/cliente/login`
- Público: `http://localhost:3000/totem/<id>`

---

## Deploy

O projeto não contém `Dockerfile` nem `render.yaml` atualmente.

O `deploy.bat` apenas automatiza:

- `git add .`
- `git commit -m "..."`
- `git push`

Ou seja, o deploy em si depende da sua infraestrutura (ex.: Render, Railway, VPS, etc.).

Requisitos típicos em produção:

- Definir `PORT` via ambiente (muitos PaaS injetam automaticamente)
- Definir `SERVER_URL` com o domínio público
- Definir `SESSION_SECRET`
- Definir `FIREBASE_CREDENTIALS` como secret (recomendado)

---

## Fluxos do sistema (end-to-end)

### 1) Acesso público ao totem

Rota: `GET /totem/:id`

- O servidor busca o totem:
  - primeiro em `Firestore` (`totens/{id}`)
  - se não encontrar ou sem Firebase, tenta fallback em `server/clientes/{id}.txt`
- Se não existir: `404 Totem não encontrado`
- Se expirado: redireciona para `/expirado`
- Se ativo:
  - publica `play` no tópico MQTT `totem/{id}`
  - redireciona (HTTP 302) para o `link` do totem (Instagram)

### 2) Admin cria/edita/exclui totem

- Admin autentica em `/admin/login`.
- Criação (`/admin/novo`):
  - valida `link` contém `instagram.com`
  - valida `dataExpiracao` é futura
  - grava em Firestore (se disponível)
  - grava fallback em arquivo `server/clientes/{id}.txt`

### 3) Cliente envia áudio personalizado

Rota: `POST /cliente/audio/:id` (multipart com campo `audio`)

- Verifica sessão do cliente (`clienteTotemId`)
- Salva arquivo em `server/uploads/`
- Se não for `.mp3`, converte para `.mp3` com FFmpeg
- Se Firebase Storage estiver disponível:
  - faz upload em `audios/{id}/{filename}`
  - marca como público (`makePublic()`)
  - remove o arquivo temporário local
- Se Storage indisponível:
  - mantém arquivo em `server/uploads/`
  - usa URL de fallback: `${SERVER_URL}/uploads/<filename>`
- Atualiza Firestore em `totens/{id}`:
  - `audio` (metadados + `url`)
  - `ultimaAtualizacaoAudio`
- Notifica ESP32 via MQTT:
  - `totem/{id}/audio` com JSON `{ comando: "atualizar", versao: ... }`
  - e também publica `atualizar_audio` em `totem/{id}`

### 4) Cliente atualiza configuração de LED/efeito e volume

Rota: `POST /cliente/config/:id` (JSON)

- Aceita configurações duais:
  - **`idle`**: configuração do modo espera (sem duração, roda continuamente)
  - **`trigger`**: configuração do modo disparo (com duração em segundos)
  - **`volume`**: volume do áudio (0-10)
- Sanitiza e normaliza campos de cada modo:
  - `mode` (string upper: BREATH, SOLID, RAINBOW, BLINK, RUNNING, HEART)
  - `color` (hex)
  - `speed` (int >= 1)
  - `duration` (int >= 1, apenas para trigger)
  - `maxBrightness` (0..180)
- Salva (se Firestore disponível):
  - `totens/{id}.idleConfig` + `totens/{id}.triggerConfig` + `totens/{id}.volume`
  - `ultimaAtualizacaoConfig`
- Publica via MQTT (retained) em três tópicos:
  - `totem/{id}/config/idle` com JSON do idleConfig
  - `totem/{id}/config/trigger` com JSON do triggerConfig
  - `totem/{id}/config/volume` com valor inteiro (0-10)

### 5) Firmware consulta API de áudio

Rota: `GET /api/audio/:id`

- Se Firebase indisponível: retorna `{ url: null, versao: "1.0" }`
- Se totem existe e tem `audio.url`: retorna `{ url, nome, versao, tamanho }`
- Se totem existe mas sem áudio: retorna `{ url: null, versao: "1.0" }`

---

## Modelos de dados (Firestore)

### Coleção `totens`

Documento: `totens/{id}`

Campos observados no código:

- `link` (string)
  - URL do Instagram.
- `dataExpiracao` (string)
  - Formato esperado: `YYYY-MM-DD`.
- `status` (string)
  - Usado na criação como `ativo`.
- `criadoEm` (timestamp server)
- `ultimaRenovacao` (timestamp server)

Campos de áudio:

- `audio` (objeto)
  - `nome` (string) – nome original enviado pelo usuário
  - `tamanho` (number)
  - `dataUpload` (string ISO)
  - `filename` (string)
  - `mimetype` (string, definido como `audio/mpeg`)
  - `url` (string)
  - `storagePath` (string | null)
- `ultimaAtualizacaoAudio` (timestamp server)

Configuração de LED/efeito (Sistema Dual v4.1.0):

- `idleConfig` (objeto) - Modo Espera
  - `mode` (ex.: `BREATH`)
  - `color` (ex.: `#FF3366`)
  - `speed` (int)
  - `maxBrightness` (int, 0-180)
  - `updatedAt` (string ISO)
  
- `triggerConfig` (objeto) - Modo Disparo
  - `mode` (ex.: `RAINBOW`)
  - `color` (ex.: `#00FF00`)
  - `speed` (int)
  - `duration` (int, segundos)
  - `maxBrightness` (int, 0-180)
  - `updatedAt` (string ISO)

- `volume` (int, 0-10)
  - 0 = mudo
  - 10 = máximo
  
- `ultimaAtualizacaoConfig` (timestamp server)

**Compatibilidade**: O sistema mantém compatibilidade com o formato antigo (`config` único)

### Coleção `config`

Documento: `config/admin`

- `senha` (string)

Ver também: `docs/alterar-senha-admin.md`.

---

## MQTT: tópicos e mensagens

Broker no servidor:

- Host: `broker.hivemq.com`
- Porta: `1883`

### Tópicos usados pelo servidor (backend)

- `totem/{id}/trigger`
  - Publica: `play` (para disparar efeito no ESP32)

- `totem/{id}/audioUpdate`
  - Publica JSON:

```json
{
  "comando": "audioUpdate",
  "timestamp": 1710000000000,
  "versao": "2026-03-03T17:00:00.000Z"
}
```

- `totem/{id}/config/idle` **(retained)**
  - Publica JSON com configuração do modo espera:

```json
{
  "mode": "BREATH",
  "color": "#FF3366",
  "speed": 50,
  "maxBrightness": 120,
  "updatedAt": "2026-03-04T10:30:00.000Z"
}
```

- `totem/{id}/config/trigger` **(retained)**
  - Publica JSON com configuração do modo disparo:

```json
{
  "mode": "RAINBOW",
  "color": "#00FF00",
  "speed": 70,
  "duration": 30,
  "maxBrightness": 150,
  "updatedAt": "2026-03-04T10:30:00.000Z"
}
```

- `totem/{id}/config/volume` **(retained)**
  - Publica valor inteiro (0-10)

### Observação importante sobre compatibilidade com firmware

Na subpasta `firmware/` existe uma arquitetura mais modular que usa tópicos como:

- `totem/{id}/trigger`
- `totem/{id}/configUpdate`
- `totem/{id}/audioUpdate`
- `totem/{id}/firmwareUpdate`
- `totem/{id}/status`

Já o backend atual publica `play` em `totem/{id}` e usa `totem/{id}/audio`.

Se você estiver usando o firmware **modular** (o de `firmware/src/main.cpp`), ele espera `.../trigger` para disparar play.

Se você estiver usando o firmware “legado” (exemplo em `docs/ESP32.txt`), ele assina `totem/{id}` e `totem/{id}/audio` e é compatível com o backend.

---

## Endpoints e rotas

Baseado em `server/server.js`.

### Rotas públicas

- `GET /`
  - Redireciona para `/admin/login`.

- `GET /totem/:id`
  - Acesso público do totem.
  - Redireciona para o link do Instagram se ativo.

- `GET /expirado`
  - Página HTML informando expiração.

- `GET /uploads/<file>`
  - Servidor expõe `server/uploads/` como estático.
  - Usado como fallback quando Storage não está disponível.

### Painel do cliente

- `GET /cliente/login`
  - Página de login do cliente (por `totemId`).

- `POST /cliente/login`
  - Body: `{ totemId }`
  - Cria sessão (`clienteTotemId`, `isCliente`) e redireciona para dashboard.

- `GET /cliente/dashboard/:id`
  - Requer sessão compatível com `:id`.
  - Renderiza HTML substituindo placeholders (`{{ID}}`, `{{LINK}}`, etc.).

- `GET /cliente/logout`
  - Destroi sessão.

- `POST /cliente/audio/:id`
  - Multipart (campo `audio`).
  - Faz upload + conversão + notificação MQTT.

- `POST /cliente/config/:id`
  - JSON com campos de configuração.
  - Salva no Firestore (se disponível) e publica MQTT retained em `.../configUpdate`.

### Painel admin

- `GET /admin/login`
  - Página de login.

- `POST /admin/login`
  - Body: `{ senha }`
  - Valida contra Firestore `config/admin.senha` ou fallback `159268`.

- `GET /admin/logout`
  - Destroi sessão.

- `GET /admin/dashboard`
  - Requer sessão admin.
  - Lista totens (Firestore; se vazio, fallback em arquivos).

- `GET /admin/novo`
  - Página.

- `POST /admin/novo`
  - Body: `{ id, link, dataExpiracao }`

- `GET /admin/editar/:id`
  - Página.

- `POST /admin/editar/:id`
  - Body: `{ link, dataExpiracao }`

- `GET /admin/excluir/:id`
  - Exclui do Firestore e remove arquivo fallback.

- `POST /admin/disparar/:id`
  - Dispara manualmente:
    - publica `play` em `totem/{id}`

### API para firmware/ESP32

- `GET /api/audio/:id`
  - Retorna JSON com `url` (ou null), `versao`, etc.

- `DELETE /api/audio/:id`
  - Requer:
    - sessão admin **ou**
    - sessão cliente com `clienteTotemId === :id`
  - Remove do Storage (se `storagePath` existir) e apaga campo `audio` no Firestore.
  - Notifica via MQTT `notificarAtualizacaoAudio(id, 'removido')`.

- `GET /api/config/:id`
  - Retorna JSON com `config` atual e `updatedAt`.

---

## Upload de áudio (detalhes)

### Tipos aceitos

O upload aceita por mimetype ou extensão:

- Mimetypes: `audio/mpeg`, `audio/mp3`, `audio/webm`, `audio/ogg`, `audio/wav`, `audio/x-wav`, `audio/mp4`, `audio/x-m4a`, `audio/aac`...
- Extensões: `.mp3`, `.webm`, `.ogg`, `.wav`, `.m4a`, `.mp4`, `.aac`

### Limite de tamanho

- `5MB` por arquivo (atualizado em v4.1.0).

### Conversão para MP3

- Se o arquivo não termina em `.mp3`, o servidor converte para MP3 com:
  - codec `libmp3lame`
  - bitrate `128k`
  - 2 canais, 44.1kHz

Isso garante compatibilidade com firmwares que esperam `audio.mp3`.

### Onde o áudio fica hospedado

- Preferencial: **Firebase Storage**, com arquivo público em:
  - `https://storage.googleapis.com/<bucket>/audios/<id>/<filename>`

- Fallback: arquivo local em `server/uploads/` acessível via:
  - `${SERVER_URL}/uploads/<filename>`

---

## Compatibilidade / modos de fallback

O sistema foi escrito para continuar “subindo” mesmo sem Firebase:

- Cadastro de totem em arquivo:
  - `server/clientes/{id}.txt` contém o `link`
  - `dataExpiracao` no fallback fica como `2099-12-31`

Limitações no fallback:

- API `/api/audio/:id` retorna sempre sem URL (pois depende do Firestore)
- Painéis funcionam parcialmente, mas dados ricos (áudio/config) dependem do Firebase

---

## Troubleshooting (problemas comuns)

### 1) "FFmpeg não encontrado. Instale ffmpeg-static corretamente."

Causa:

- `ffmpeg-static` não conseguiu resolver o binário para a sua plataforma.

Ações:

- Reinstale dependências (`npm install`)
- Verifique antivírus/quarentena
- Confirme que `ffmpeg-static` suporta seu ambiente

### 2) Firebase não conecta (logs mostram "Credenciais Firebase não encontradas")

Causa:

- Não há `firebase-credentials.json`/`firebase-credentials.json.json` na raiz e `FIREBASE_CREDENTIALS` está vazio.

Ações:

- Defina `FIREBASE_CREDENTIALS` no `.env` (JSON) ou coloque arquivo na raiz.

### 3) Firebase Storage retorna 404 / bucket não existe

Causa:

- Bucket configurado (`${project_id}.appspot.com`) não existe ou Storage não está habilitado.

Ações:

- Ative Firebase Storage no console
- Confirme o `project_id` das credenciais
- Enquanto isso, o sistema usa fallback em `/uploads`

### 4) MQTT não publica ("MQTT não conectado")

Causa:

- Rede bloqueando `1883`.
- Instabilidade no broker público.

Ações:

- Teste outra rede
- Troque o broker para um privado/gerenciado

### 5) ESP32 não baixa áudio via HTTPS

Causa comum:

- TLS/CA não configurado (depende do firmware). Em `docs/ESP32.txt` é usado `setInsecure()`.

Ações:

- Se usar firmware modular (`firmware/`), configure `ROOT_CA_PEM` conforme `firmware/include/Config.h`.

---

## Segurança e boas práticas

- **Não commitar credenciais** do Firebase em repositórios públicos.
- **Trocar `SESSION_SECRET`** em produção.
- **Trocar senha do Admin** no Firestore (`config/admin.senha`).
- Se expor `/uploads` em produção:
  - considere colocar autenticação ou URLs temporárias; atualmente é um diretório público (apenas em fallback).

---

## Firmware (ESP32)

### Visão Geral

O firmware do ESP32 está localizado em `firmware/` e implementa uma arquitetura **modular e robusta** para controle de totens interativos IoT. Foi desenvolvido com foco em:

- **Confiabilidade**: Watchdog, máquina de estados, failsafe e recuperação automática de erros
- **Segurança**: HTTPS com validação de certificados, downloads seguros (temp → rename)
- **Flexibilidade**: Provisionamento dinâmico via WiFi AP, configuração via MQTT
- **Modularidade**: Componentes independentes e reutilizáveis

**Versão atual**: `4.0.0`

### Arquitetura Modular

O firmware é organizado em módulos independentes que se comunicam através do `main.cpp`:

#### Módulos Core (`src/core/`)

- **`StateMachine`** (`StateMachine.cpp/h`)
  - Gerencia estados do sistema: `BOOT`, `IDLE`, `PLAYING`, `DOWNLOADING_AUDIO`, `UPDATING_CONFIG`, `UPDATING_FIRMWARE`, `ERROR`
  - Valida transições de estado
  - Previne operações concorrentes inválidas

- **`WiFiManager`** (`WiFiManager.cpp/h`)
  - Conexão WiFi com reconexão automática
  - Portal AP para provisionamento quando não consegue conectar
  - SSID do AP: `Totem-Config-<id>`
  - Timeout configurável: 20s (padrão)
  - Intervalo de reconexão: 30s

- **`MQTTManager`** (`MQTTManager.cpp/h`)
  - Conexão com broker MQTT (HiveMQ público por padrão)
  - Reconexão automática
  - Subscrição dinâmica baseada no Totem ID
  - Suporte a mensagens retained
  - Callbacks para mensagens recebidas

- **`ConfigManager`** (`ConfigManager.cpp/h`)
  - Gerencia configurações de LED/efeito
  - Armazena configurações em memória
  - Detecta duplicatas de configuração (evita reprocessamento)
  - Ciclo de cores predefinidas
  - Validação e sanitização de parâmetros

- **`StorageManager`** (`StorageManager.cpp/h`)
  - Inicializa SPIFFS (sistema de arquivos)
  - Armazena Totem ID e Device Token em NVS (Non-Volatile Storage)
  - Gerencia versão de áudio
  - Persistência de dados entre reboots

- **`AudioManager`** (`AudioManager.cpp/h`)
  - Reprodução de áudio via I2S (ESP32-audioI2S)
  - Download seguro de áudio do servidor
  - Validação de tamanho e integridade
  - Suporte a HTTPS com validação de certificado
  - Fallback para modo insecure (se configurado)
  - Controle de volume e estado de reprodução

- **`LEDEngine`** (`LEDEngine.cpp/h`)
  - Controle de LEDs via FastLED (WS2812B)
  - Suporte a dois conjuntos de LEDs: principal (220 LEDs) e coração (15 LEDs)
  - Sistema dual: modo espera (idle) e modo disparo (trigger)
  - Execução não-bloqueante de efeitos
  - Controle de brilho (0-180)
  - Seleção de cor (RGB hex)

- **`ButtonManager`** (`ButtonManager.cpp/h`)
  - Gerencia 5 botões TTP223 capacitivos (v4.1.0)
  - Debounce configurável (50ms padrão)
  - Detecção de clique longo (5000ms para reset Wi-Fi)
  - Callbacks para cada botão
  - Prevenção de cliques duplos acidentais

- **`OTAManager`** (`OTAManager.cpp/h`)
  - Atualização de firmware Over-The-Air
  - Download de firmware via HTTPS
  - Validação e rollback automático em caso de falha
  - Timeout configurável (300s)

#### Efeitos de LED (`src/effects/`)

Cada efeito é implementado como uma classe independente:

- **`SolidEffect.h`**: Cor sólida estática
- **`RainbowEffect.h`**: Efeito arco-íris animado
- **`BlinkEffect.h`**: Piscada com velocidade configurável
- **`BreathEffect.h`**: Respiração suave (fade in/out)
- **`RunningEffect.h`**: Pontos correndo pela fita
- **`HeartEffect.h`**: Efeito especial para LEDs do coração

### Hardware Suportado

#### Placa Principal
- **ESP32-DevKit** ou **ESP32-Audio-Kit V2.2 A1S**
- Flash: mínimo 4MB (particionamento customizado)

#### LEDs (Atualizado v4.1.0)
- **Fita principal**: 220 LEDs WS2812B (GPIO 19)
- **Coração**: 15 LEDs WS2812B (GPIO 18)
- Tipo: `WS2812B`, ordem de cor: `GRB`
- Brilho máximo: 180 (de 255)
- **Sistema Dual**: Modo espera (contínuo) + Modo disparo (sincronizado com áudio)

#### Áudio (ESP32-Audio-Kit)
- **Codec**: ES8388
- **Interface**: I2S
- **Pinos I2S**:
  - BCLK: GPIO 27
  - LRC: GPIO 25
  - DOUT: GPIO 26
  - DIN: GPIO 35
- **Pinos I2C** (ES8388):
  - SDA: GPIO 33
  - SCL: GPIO 32
- **Power Amplifier Enable**: GPIO 21
- **Volume padrão**: 15 (escala 0-21)

#### Botões (TTP223 Capacitivos) - Atualizado v4.1.0
- **Botão Cor**: GPIO 15 (cicla entre cores)
- **Botão Mais**: GPIO 4 (aumenta brilho +10 / long press: +25)
- **Botão Menos**: GPIO 16 (diminui brilho -10 / long press: -25)
- **Botão Coração**: GPIO 5 (dispara reprodução - trigger)
- **Botão Reset Wi-Fi**: GPIO 22 (long press 5s para resetar credenciais)

#### SD Card (Opcional)
- CS: GPIO 5
- MOSI: GPIO 23
- MISO: GPIO 19
- SCK: GPIO 18

### Configuração (`include/Config.h`)

Arquivo central de configuração com todas as constantes do sistema:

#### Identificação
```cpp
#define TOTEM_ID                "printpixel"
#define FIRMWARE_VERSION        "4.0.0"
#define SERVER_URL              "https://totem-server.onrender.com"
```

#### Rede e MQTT
```cpp
#define MQTT_SERVER             "broker.hivemq.com"
#define MQTT_PORT               1883
#define WIFI_TIMEOUT            20000
#define WIFI_RECONNECT_INTERVAL 30000
```

#### Segurança
```cpp
#define DEVICE_TOKEN            ""           // Token opcional por dispositivo
#define ROOT_CA_PEM             ""           // Certificado CA para HTTPS
#define ALLOW_INSECURE_HTTPS    1            // Permite HTTPS sem validação (dev)
#define WDT_TIMEOUT_SECONDS     8            // Watchdog timeout
```

#### Hardware
- Todos os pinos GPIO configuráveis
- Parâmetros de LEDs (quantidade, tipo, brilho)
- Configurações de I2S e I2C
- Timings de botões (debounce, long press)

#### Comportamento
```cpp
#define EFEITO_TEMPO_PADRAO     30           // Duração padrão de efeito (segundos)
#define STATUS_INTERVAL         60000        // Intervalo de publicação de status
#define DOWNLOAD_TIMEOUT        300000       // Timeout de download (5 min)
#define MAX_AUDIO_SIZE          (8 * 1024 * 1024)  // Tamanho máximo de áudio (8MB)
```

### Tópicos MQTT

O firmware subscreve aos seguintes tópicos (montados dinamicamente com o Totem ID):

- **`totem/{id}/trigger`**
  - Comando: `play` → dispara reprodução de áudio e efeito de LED
  
- **`totem/{id}/configUpdate`** (retained)
  - JSON com configuração de LED/efeito
  - Campos: `mode`, `color`, `speed`, `duration`, `maxBrightness`
  
- **`totem/{id}/audioUpdate`**
  - Comando para forçar download de novo áudio
  
- **`totem/{id}/firmwareUpdate`**
  - URL do firmware para atualização OTA

O firmware publica em:

- **`totem/{id}/status`** (retained, a cada 60s)
  - JSON com: `online`, `fw`, `audioVersion`, `rssi`, `ip`, `state`

### Provisionamento (Totem ID e Token)

O firmware suporta provisionamento dinâmico sem necessidade de recompilação:

1. **Primeira inicialização**: Se não houver WiFi configurado, o ESP32 cria um Access Point:
   - SSID: `Totem-Config-<id>`
   - Senha: (sem senha ou configurável)

2. **Portal de configuração**: Ao conectar no AP, abre-se uma página web onde você define:
   - SSID e senha do WiFi
   - **Totem ID** (identificador único do totem)
   - **Device Token** (opcional, para autenticação)

3. **Armazenamento**: Os dados são salvos em **NVS** (memória não-volátil) e persistem entre reboots

4. **Reset WiFi**: Pressione o botão de reset (GPIO 22) por 5 segundos para resetar as configurações de WiFi e voltar ao modo AP

### Fluxos de Operação

#### 1. Boot e Inicialização
```
BOOT → Inicializa Storage → Carrega Config → Inicializa LEDs → 
Inicializa Botões → Inicializa Áudio → Conecta WiFi → 
Conecta MQTT → Inicia Watchdog → IDLE
```

#### 2. Reprodução (Trigger via MQTT ou Botão)
```
IDLE → Recebe "play" em totem/{id}/trigger → 
Verifica arquivo de áudio → (Se ausente: baixa do servidor) →
PLAYING → Inicia efeito LED (trigger mode) → Reproduz áudio → 
Aguarda duração configurada → Retorna ao modo IDLE (idle mode)
```

#### 3. Atualização de Configuração (Sistema Dual v4.1.0)

**Config Idle:**
```
IDLE → Recebe JSON em totem/{id}/config/idle → 
Valida e salva idleConfig → 
Aplica imediatamente se em estado IDLE → 
Atualiza LED (cor/brilho/efeito)
```

**Config Trigger:**
```
IDLE → Recebe JSON em totem/{id}/config/trigger → 
Valida e salva triggerConfig → 
🎵 AUTO-PLAY: Dispara efeito + áudio automaticamente → 
PLAYING → Aguarda duração → Retorna ao modo IDLE
```

**Volume:**
```
Qualquer estado → Recebe valor em totem/{id}/config/volume → 
Mapeia 0-10 para 0-21 (ES8388) → 
Aplica volume imediatamente
```

#### 4. Download de Áudio (v4.1.0 com Auto-Play)
```
IDLE → Recebe comando em totem/{id}/audioUpdate → 
DOWNLOADING_AUDIO → Consulta API /api/audio/{id} → 
Deleta áudio antigo (libera espaço SPIFFS) → 
Baixa para /audio.tmp → Valida tamanho → 
Renomeia para /audio.mp3 → Atualiza versão → 
🎵 AUTO-PLAY: Reproduz automaticamente → 
PLAYING → Aguarda duração → Retorna ao modo IDLE
```

#### 5. Atualização de Firmware (OTA)
```
IDLE → Recebe URL em totem/{id}/firmwareUpdate → 
UPDATING_FIRMWARE → Baixa firmware → Valida → 
Aplica update → Reinicia → (Se falhar: rollback automático)
```

#### 6. Recuperação de Erro
```
ERROR → (Modo failsafe: LED branco baixo brilho) → 
Aguarda WiFi e MQTT reconectarem → IDLE
```

### Build e Upload

#### PlatformIO (Recomendado)

Dentro da pasta `firmware/`:

```bash
# Compilar
pio run

# Upload via USB
pio run -t upload

# Monitor serial
pio device monitor

# Compilar + Upload + Monitor
pio run -t upload && pio device monitor
```

#### Arduino IDE

1. Crie uma pasta `TotemFirmware/`
2. Dentro crie `TotemFirmware.ino`:
   ```cpp
   #include "src/main.cpp"
   ```
3. Copie as pastas `src/` e `include/` para dentro da pasta do sketch

**Bibliotecas necessárias**:
- `ESP32-audioI2S`
- `ESPAsyncWebServer`
- `AsyncTCP`
- `WiFiManager` (tzapu)
- `FastLED`
- `ArduinoJson`
- `PubSubClient`

**Configurações da placa**:
- Board: ESP32 Dev Module
- Partition Scheme: Custom (usar `partitions.csv`)
- Flash Size: 4MB (mínimo)

### Particionamento de Memória

O arquivo `partitions.csv` define o layout da flash:

```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x180000
app1,     app,  ota_1,   0x190000,0x180000
spiffs,   data, spiffs,  0x310000,0xF0000
```

- **NVS**: Armazena Totem ID, token, configurações WiFi
- **OTA**: Suporte a duas partições de app (rollback automático)
- **SPIFFS**: Sistema de arquivos para áudio (960KB)

### Segurança e HTTPS

O firmware implementa downloads seguros via HTTPS:

1. **Validação de certificado**: Se `ROOT_CA_PEM` estiver definido em `Config.h`, valida o certificado do servidor
2. **Modo insecure**: Se `ALLOW_INSECURE_HTTPS = 1`, permite conexões sem validação (útil para desenvolvimento)
3. **Download seguro**: Arquivos são baixados para `.tmp` e só renomeados após validação completa

**Para produção**: Configure o `ROOT_CA_PEM` com o certificado raiz do seu servidor.

### Watchdog e Failsafe

- **Watchdog**: Timeout de 8 segundos, resetado a cada loop
- **Failsafe**: Em caso de erro crítico, entra em modo seguro com LED branco em baixo brilho
- **Auto-recuperação**: Tenta retornar ao estado IDLE quando WiFi/MQTT reconectam
- **OTA Rollback**: Se o novo firmware falhar, volta automaticamente para a versão anterior

### Debugging

O firmware gera logs detalhados via Serial (115200 baud):

```
[BOOT] app start
[BOOT] reset_reason=1
[BOOT] fw=4.0.0
[BOOT] totem_id=printpixel
[BOOT] Storage initialized
[BOOT] WiFi manager initialized
[MQTT] Connected to broker
[MAIN] TRIGGER COMMAND RECEIVED
[MAIN] ✓ Play command detected
[MAIN] ✓ Audio file exists - Size: 245632 bytes
[MAIN] ✓ Playback started
[LOOP] PLAYING - Audio: YES, Remaining: 25 sec
```

### Compatibilidade com Backend v4.1.0

**Sistema Dual**: O firmware v4.1.0 está totalmente compatível com o backend v4.1.0:

| Tópico MQTT | Payload | Função |
|-------------|---------|--------|
| `totem/{id}/trigger` | `"play"` | Dispara reprodução (trigger mode) |
| `totem/{id}/config/idle` | JSON | Configura modo espera (contínuo) |
| `totem/{id}/config/trigger` | JSON | Configura modo disparo + AUTO-PLAY |
| `totem/{id}/config/volume` | `0-10` | Ajusta volume do áudio |
| `totem/{id}/audioUpdate` | JSON | Notifica novo áudio + AUTO-PLAY |
| `totem/{id}/configUpdate` | JSON | ✅ Compatibilidade retroativa |

**Funcionalidades v4.1.0**:
- ✅ Sistema dual de iluminação (idle + trigger)
- ✅ Auto-play ao receber novo áudio
- ✅ Auto-play ao atualizar config trigger
- ✅ Controle de volume (0-10)
- ✅ Otimização SPIFFS (deleta áudio antigo)
- ✅ Retorno automático ao modo idle após playback

### Documentação Adicional

- **README do firmware**: `firmware/README.md`
- **Exemplo legado**: `docs/ESP32.txt` (firmware monolítico antigo)
- **Manual de firmware**: `docs/manual-firmware.md`
- **Referência da API**: `docs/api-reference.md`
