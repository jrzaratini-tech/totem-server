# TOTEM INTERATIVO IoT v3.0 (PROFISSIONAL)

Sistema de engajamento com QR Code + ESP32 + MQTT + dashboard administrativo + controle de assinatura mensal por **data de expiração** usando **Firebase Firestore**.

## Visão geral

Este projeto é um servidor Node.js (Express) que atende dois fluxos:

- **Fluxo público (uso do totem)**: o QR Code do totem aponta para `/totem/:id`. O servidor valida a assinatura no Firestore; se estiver ativa, publica `play` no MQTT e redireciona para o Instagram cadastrado.
- **Fluxo administrativo (dashboard)**: CRUD de totens (criar/editar/excluir) e visualização do status (ativo/expirado) por data.

## Arquitetura (ponta a ponta)

1. Usuário escaneia o QR Code (URL do tipo `https://SEU_HOST/totem/TOTEM99`)
2. O servidor (`server.js`) recebe a requisição em `GET /totem/:id`
3. O servidor consulta o Firestore: `collection("totens").doc(id)`
4. O servidor compara `dataExpiracao` com a data atual
5. Se **ativo**:
   - publica `play` no tópico MQTT `totem/{id}`
   - redireciona (`302`) para o `link` (Instagram)
6. Se **expirado**:
   - redireciona para `GET /expirado` (página em `views/expirado.html`)
7. O ESP32 assina `totem/{DEVICE_ID}` e aciona a ação física ao receber `play`

## Estrutura do repositório (inventário completo)

Observação: a listagem abaixo considera apenas os arquivos/pastas do projeto (ignorando `node_modules/` e `.git/`).

```text
totem-server/
├── .gitignore
├── README.md
├── deploy.bat
├── firebase-credentials.json.json
├── package-lock.json
├── package.json
├── server.js
├── clientes/
└── views/
    ├── admin.html
    ├── editar.html
    ├── expirado.html
    ├── login.html
    ├── mensagem.html
    └── novo.html
```

### Descrição de cada item

#### Arquivos na raiz

- **`.gitignore`**
  - Define o que não deve ir para o Git.
  - Atualmente ignora `node_modules/`, `clientes/`, `.env*`, logs e também `*.json` (para evitar vazar credenciais).

- **`README.md`**
  - Documentação do projeto.

- **`deploy.bat`**
  - Script de deploy para Windows que faz:
    - `git add .`
    - `git commit -m "<mensagem>"`
    - `git push`
  - Útil quando o deploy está conectado ao repositório (ex.: Render/GitHub).

- **`firebase-credentials.json.json`**
  - Arquivo de credenciais do Firebase Admin SDK (Service Account).
  - **Importante**: este arquivo contém segredo e **não deve ser commitado**.
  - O `server.js` tenta carregar primeiro o arquivo local `./firebase-credentials.json`; se falhar, usa `FIREBASE_CREDENTIALS` como variável de ambiente.

- **`package.json`**
  - Metadados e dependências do projeto.
  - Dependências principais:
    - `express` (servidor HTTP)
    - `mqtt` (cliente MQTT)
    - `firebase-admin` (acesso Admin ao Firestore)
    - `express-session` (sessão do dashboard)
    - `body-parser` (parse de payload)
    - `fs-extra` (I/O com utilitários)

- **`package-lock.json`**
  - Travamento de versões (lockfile) para garantir builds reprodutíveis.

- **`server.js`**
  - **Arquivo principal** da aplicação.
  - Responsabilidades:
    - Inicializar Express + sessões + parsers
    - Conectar no MQTT (HiveMQ público)
    - Inicializar Firebase Admin SDK e Firestore
    - Definir rotas públicas (`/totem/:id`, `/expirado`) e rotas admin (`/admin/*`)
    - Renderizar HTML do dashboard lendo templates em `views/` e substituindo placeholders

#### Pastas

- **`clientes/`**
  - Pasta mantida para **compatibilidade temporária**.
  - O `server.js` garante que ela existe e escreve um arquivo `<id>.txt` com o link do Instagram ao criar/editar.
  - Fonte de verdade atual: Firestore (`totens/{id}`).

- **`views/`** (front-end estático do dashboard)
  - **`login.html`**: formulário de login do admin (`POST /admin/login`).
  - **`mensagem.html`**: página de erro quando a senha do admin está incorreta.
  - **`admin.html`**: dashboard com tabela e ações; recebe conteúdo via placeholder `{{LINHAS_TABELA}}` montado no `server.js`.
  - **`novo.html`**: formulário para cadastrar novo totem (ID, link, `dataExpiracao`).
  - **`editar.html`**: formulário de edição com placeholders `{{ID}}`, `{{LINK}}`, `{{DATA_EXPIRACAO}}`.
  - **`expirado.html`**: página pública exibida quando um totem está com assinatura expirada.

## Como rodar localmente

### Pré-requisitos

- Node.js (recomendado 18+)
- Conta e projeto no Firebase (Firestore habilitado)

### Instalação

```bash
npm install
```

### Executar

```bash
node server.js
```

Por padrão o servidor sobe em `http://localhost:3000` (ou na porta definida por `PORT`).

## Configuração

### Variáveis de ambiente

- **`PORT`** (opcional)
  - Porta do servidor. Default: `3000`.

- **`FIREBASE_CREDENTIALS`** (recomendado em produção)
  - JSON string com as credenciais do Service Account.
  - O `server.js` tenta carregar primeiro o arquivo local `./firebase-credentials.json`; se falhar, usa esta env var.

### Firebase / Firestore

Coleção esperada:

```text
totens/{ID_DO_TOTEM}
  link: string
  dataExpiracao: string (YYYY-MM-DD)
  status: string (ex.: "ativo")
  criadoEm: timestamp
  ultimaRenovacao: timestamp
```

Regras de segurança (sugestão): bloquear acesso direto do cliente e permitir apenas via servidor (Admin SDK).

## Comunicação MQTT

- **Broker**: `broker.hivemq.com`
- **Porta**: `1883`
- **Tópico por totem**: `totem/{ID}`
- **Mensagem**: `play`

## Rotas HTTP (documentação completa)

### Públicas

- **`GET /`**
  - Redireciona para `GET /admin/login`.

- **`GET /totem/:id`**
  - Busca o documento `totens/{id}` no Firestore.
  - Se não existir: `404`.
  - Se expirado: redireciona para `/expirado`.
  - Se ativo: publica MQTT em `totem/{id}` com payload `play` e redireciona para o Instagram.

- **`GET /expirado`**
  - Retorna `views/expirado.html`.

### Admin (protegidas por sessão)

- **`GET /admin/login`**
  - Retorna `views/login.html`.

- **`POST /admin/login`**
  - Valida a senha fixa do admin (constante em `server.js`: `SENHA_ADMIN`).
  - Se ok: cria sessão e redireciona para `/admin/dashboard`.
  - Se falhar: retorna `views/mensagem.html`.

- **`GET /admin/logout`**
  - Destroi a sessão e redireciona para `/admin/login`.

- **`GET /admin/dashboard`**
  - Lista todos os totens do Firestore.
  - Monta as linhas HTML e injeta em `views/admin.html` no placeholder `{{LINHAS_TABELA}}`.
  - Calcula status (ativo/expirado) comparando `dataExpiracao` com o dia atual.

- **`GET /admin/novo`**
  - Retorna `views/novo.html`.

- **`POST /admin/novo`**
  - Valida `id`, `link` e `dataExpiracao`.
  - Exige que `link` contenha `instagram.com`.
  - Exige data futura.
  - Cria documento `totens/{id}`.
  - (Compatibilidade) cria `clientes/{id}.txt` com o link.

- **`GET /admin/editar/:id`**
  - Busca `totens/{id}`.
  - Injeta valores em `views/editar.html` usando placeholders.

- **`POST /admin/editar/:id`**
  - Atualiza `link` e `dataExpiracao` e grava `ultimaRenovacao`.
  - (Compatibilidade) regrava `clientes/{id}.txt`.

- **`GET /admin/excluir/:id`**
  - Remove o documento no Firestore.
  - Remove `clientes/{id}.txt` se existir.

## Deploy

- **Via Git**: você pode usar o `deploy.bat` para commitar e fazer push rapidamente.
- **Produção**: configure `FIREBASE_CREDENTIALS` (env var) no provedor (ex.: Render) e não dependa de arquivo local de credenciais.

## Observações importantes

- **Credenciais**: nunca comite service account.
- **Timezone**: a lógica atual trata `dataExpiracao` como válida até `23:59:59` do dia informado.
- **Broker MQTT público**: ótimo para protótipo; para produção em escala, considere broker privado/autenticado.