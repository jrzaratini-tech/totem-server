📄 README.md (VERSÃO COMPLETA E PROFISSIONAL)
markdown
# 🎛️ TOTEM INTERATIVO IoT v4.0 (PROFISSIONAL)

Sistema completo de engajamento com QR Code + ESP32 + MQTT + Dashboard Administrativo + Áudio Personalizado pelo Cliente + Controle de Assinaturas via Firebase.

🔗 **Acesso ao sistema:** [https://totem-server.onrender.com](https://totem-server.onrender.com)

---

## 📌 VISÃO GERAL DO SISTEMA

O **Totem Interativo IoT** é uma solução física para eventos e pontos de venda que permite gerar engajamento em redes sociais de forma automatizada, com **sistema de assinatura mensal**, **bloqueio automático por data de expiração** e agora com **áudio personalizado gerenciado pelo próprio cliente**.

### 🎯 Objetivo Comercial

- ✅ Produto físico de engajamento com **receita recorrente**
- ✅ Cliente gerencia seu próprio áudio (música/mensagem promocional)
- ✅ Configuração rápida via WiFiManager
- ✅ Gerenciamento completo via dashboard com validade
- ✅ Bloqueio automático se não pagar
- ✅ Renovação simples (só alterar a data)
- ✅ Escalável para **milhares de clientes**
- ✅ Profissional com banco de dados seguro (Firebase)

---

## 🏗️ ARQUITETURA DO SISTEMA (PONTA A PONTA)
CLIENTE (usuário final)
↓ (escaneia QR Code)
Servidor Node.js (Render)
↓ (consulta Firebase)
Firebase Firestore (valida assinatura)
↓ (se ativo)
Broker MQTT (HiveMQ)
↓ (comando "play")
ESP32 + MP3 TF-16P
↓
✅ Ação Física: LED + Áudio Personalizado + Redirecionamento Instagram

text
        ╔══════════════════════════════════════╗
        ║     FLUXO DE GERENCIAMENTO           ║
        ╚══════════════════════════════════════╝
ADMIN (você)
↓ (cadastra totem)
Dashboard Admin → https://totem-server.onrender.com/admin/login
↓
CLIENTE (dono do totem)
↓ (faz upload do MP3)
Dashboard Cliente → https://totem-server.onrender.com/cliente/login
↓ (notificação via MQTT)
ESP32 baixa novo áudio e salva no SD Card
↓
✅ Totem atualizado automaticamente!

text

---

## 🔧 COMPONENTES DO SISTEMA

### 1️⃣ **Hardware (Totem Físico)**

| Componente | Especificação | Função |
|------------|---------------|--------|
| **ESP32** | AZDelivery / ELEGOO | Controle principal, WiFi, MQTT |
| **MP3 TF-16P** | Módulo leitor MP3 | Reprodução de áudio via cartão SD |
| **Cartão SD** | MicroSD (qualquer tamanho) | Armazena o áudio personalizado |
| **Alto-falante** | 3W - 5W | Saída de áudio |
| **Fita de LED** | Endereçável (WS2812B) | Efeitos visuais (100 LEDs) |
| **Fita de LED Cardíaco** | Endereçável (WS2812B) | Efeito de batimento cardíaco |
| **Botões Físicos** | 4 botões | Controle de cor e brilho, disparo manual |
| **Fonte** | 5V / 3A | Alimentação do sistema |

### 2️⃣ **Backend (Servidor na Nuvem)**

- **Plataforma:** Render (https://render.com)
- **URL:** https://totem-server.onrender.com
- **Linguagem:** Node.js / Express
- **Banco de Dados:** Firebase Firestore
- **Armazenamento de Áudio:** Firebase Storage
- **Comunicação:** MQTT (broker.hivemq.com)

### 3️⃣ **Dashboards**

#### 🔐 Dashboard Administrativo
- **URL:** `/admin/login`
- **Senha:** `159268`
- **Funcionalidades:**
  - Cadastrar novos totens (ID, link Instagram, data de expiração)
  - Editar totens existentes
  - Excluir totens
  - Visualizar status (ativo/expirado)
  - Ver quais totens têm áudio personalizado

#### 👤 Área do Cliente
- **URL:** `/cliente/login`
- **Login:** ID do totem + senha (últimos 4 dígitos do ID + 2026)
- **Funcionalidades:**
  - Upload de áudio personalizado (MP3 até 5MB)
  - Visualizar áudio atual
  - Remover áudio personalizado (volta ao padrão)
  - Pré-ouvir antes de enviar

---

## 📂 ESTRUTURA DO PROJETO
totem-server/
│
├── server.js # Servidor principal (Node.js)
├── package.json # Dependências
├── firebase-credentials.json # 🔐 Chave do Firebase (NÃO COMMITAR)
├── .gitignore # Arquivos ignorados
│
├── routes/
│ └── audio.js # Rotas de gerenciamento de áudio
│
├── middlewares/
│ └── auth.js # Middlewares de autenticação
│
├── views/ # Páginas HTML
│ ├── login.html # Login do admin
│ ├── admin.html # Dashboard admin
│ ├── novo.html # Cadastro de totens
│ ├── editar.html # Edição de totens
│ ├── mensagem.html # Mensagem de erro
│ ├── expirado.html # Página de totem expirado
│ ├── cliente-login.html # Login do cliente
│ └── cliente-dashboard.html # Dashboard do cliente
│
├── clientes/ # 🔸 Compatibilidade (arquivos .txt)
├── uploads/ # Arquivos temporários de upload
└── firmware/ # Código do ESP32
└── totem_v4.ino # Firmware completo

text

---

## 🔥 FIREBASE (BANCO DE DADOS)

### Coleção `totens`

```javascript
/totens/{ID_DO_TOTEM}/
  - link: "https://instagram.com/..."           // Instagram do cliente
  - dataExpiracao: "2026-12-31"                 // YYYY-MM-DD
  - status: "ativo"                              // ativo/bloqueado
  - criadoEm: timestamp                          // Data de criação
  - ultimaRenovacao: timestamp                    // Última renovação
  
  // Áudio personalizado
  - audio: {
      nome: "musica_cliente.mp3",                 // Nome do arquivo
      url: "https://storage.googleapis.com/...", // URL do áudio
      dataUpload: timestamp,                       // Data do upload
      tamanho: 1234567,                            // Tamanho em bytes
      storagePath: "audios/TOTEM47/arquivo.mp3"    // Caminho no Storage
    }
Regras de Segurança
javascript
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    match /{document=**} {
      allow read, write: if false;  // Apenas Admin SDK
    }
  }
}
📡 COMUNICAÇÃO MQTT
Broker: broker.hivemq.com

Porta: 1883

Tópicos:

totem/{DEVICE_ID} → Comando play (acionado pelo QR Code)

totem/{DEVICE_ID}/audio → Notificação de novo áudio

🔌 FIRMWARE ESP32 (TOTEM v4.0)
Configuração Inicial
No arquivo totem_v4.ino, alterar APENAS:

cpp
// LINHA 13 - Coloque o ID do seu totem (igual ao cadastrado)
#define DEVICE_ID "TOTEM47"

// LINHA 16 - JÁ ESTÁ CORRETO (NÃO ALTERAR)
#define SERVER_URL "https://totem-server.onrender.com"
Funcionalidades do Firmware
✅ Conexão WiFi via WiFiManager (modo AP: "TOTEM_SETUP" / senha "12345678")

✅ Comunicação MQTT (recebe comando play e notificações)

✅ Controle da fita de LED (cores, brilho, efeitos)

✅ Efeito de batimento cardíaco na fita secundária

✅ LED de feedback do QR Code

✅ Botões físicos para controle manual

✅ Download automático de áudio do servidor

✅ Armazenamento no cartão SD (arquivo 01.mp3)

✅ Reprodução via MP3 TF-16P (UART2)

✅ Verificação periódica de novo áudio (a cada 1 hora)

✅ Salvamento de configurações na EEPROM

Pinos do ESP32
Função	Pino GPIO
Botão Reset WiFi	0
Botão Cor	15
Botão Brilho +	4
Botão Brilho -	5
Botão Coração (manual)	18
LED QR Code	2
Fita LED Principal	12
Fita LED Cardíaco	13
MP3 RX (TX do módulo)	26
MP3 TX (RX do módulo)	27
🚀 FLUXO COMPLETO DE USO
1️⃣ Para o Administrador (você)






2️⃣ Para o Cliente (dono do totem)





3️⃣ Para o Usuário Final (quem escaneia)








📊 DASHBOARDS EM DETALHE
🔐 Admin (/admin/login)
Coluna	Descrição
ID	Identificador do totem
Link	Instagram do cliente
Expira em	Data de expiração da assinatura
Status	✅ Ativo / ❌ Expirado
Ações	Editar / Excluir
👤 Cliente (/cliente/login)
Informações exibidas:

ID do totem

Link do Instagram

Data de expiração

Áudio atual

Ações disponíveis:

📂 Escolher arquivo MP3 (até 5MB)

⬆️ Enviar novo áudio

🗑️ Remover áudio personalizado

👂 Pré-ouvir antes de enviar

🔐 SEGURANÇA
Camada	Proteção
Dashboard Admin	Senha fixa (159268) + sessão
Área do Cliente	Login por ID + senha (últimos 4 dígitos + 2026)
Firebase	Regras bloqueadas para acesso direto
Credenciais	Service Account NUNCA versionada
Upload de Áudio	Validação de tipo e tamanho (máx 5MB)
ESP32	Apenas baixa áudio do servidor autorizado
📈 ESCALABILIDADE
Componente	Capacidade
Firebase Firestore	Ilimitado (escalável automaticamente)
Firebase Storage	5GB gratuitos, depois pago por uso
Servidor Render	Plano gratuito: 750h/mês, sleep após inatividade
Broker MQTT	Público (HiveMQ) - sem limite prático
ESP32	Cada totem é independente
🧪 TESTES REALIZADOS
✅ Cadastro de totem com data futura

✅ Bloqueio de totem com data passada

✅ Upload de áudio pelo cliente

✅ Download automático pelo ESP32

✅ Reprodução no módulo MP3 TF-16P

✅ Notificação via MQTT

✅ Botões físicos (cor, brilho, manual)

✅ Efeitos de LED sincronizados com áudio

✅ Fallback para áudio padrão

🚀 DEPLOY E ATUALIZAÇÃO
Servidor (Render)
O servidor está hospedado no Render e atualiza automaticamente quando você faz push no GitHub.

Firmware (ESP32)
Para atualizar um totem:

Conecte o ESP32 ao computador via USB

Abra a Arduino IDE com o código totem_v4.ino

Altere o DEVICE_ID se necessário

Clique em Upload

📌 OBSERVAÇÕES IMPORTANTES
Áudio Personalizado: O arquivo baixado é sempre salvo como 01.mp3 na raiz do SD Card

Volume: Configurado para nível 20 (0-30) no setup

Verificação de Áudio: O ESP32 verifica novidades a cada 1 hora

WiFi: Usa WiFiManager - na primeira vez, conecte-se à rede "TOTEM_SETUP" e configure

Reset de Fábrica: Segure o botão RESET por 5 segundos para apagar WiFi

🆘 SUPORTE E MANUTENÇÃO
Logs no Serial Monitor
Conecte o ESP32 e abra o Serial Monitor (115200 baud) para ver:

Status da conexão WiFi

Mensagens MQTT recebidas

Downloads de áudio

Ações dos botões

Códigos de Erro Comuns
Código	Significado	Solução
❌ Erro ao montar SD	Cartão não inserido ou com falha	Verificar SD Card
❌ Erro HTTP 404	Totem não encontrado	Verificar DEVICE_ID
❌ Falha MQTT	Broker inacessível	Verificar internet
🎯 CONCLUSÃO
O TOTEM INTERATIVO IoT v4.0 é uma solução profissional, escalável e com modelo de negócio baseado em receita recorrente. Com a nova funcionalidade de áudio personalizado gerenciado pelo cliente, você oferece:

✅ Autonomia para o cliente - gerencia próprio áudio
✅ Zero necessidade de acesso físico - tudo remoto
✅ Escalabilidade - prepare-se para centenas/milhares de totens
✅ Profissionalismo - dashboard white-label para seus clientes
✅ Diferencial competitivo - áudio personalizado em tempo real

📞 CONTATO E SUPORTE
Sistema Online: https://totem-server.onrender.com

Admin: /admin/login (senha: 159268)

Cliente: /cliente/login

Desenvolvido com ❤️ para Totens Interativos IoT
Versão 4.0 - Fevereiro 2026