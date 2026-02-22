# 🎛️ TOTEM INTERATIVO IoT v3.0 (PROFISSIONAL)

Sistema de Engajamento com QR Code + ESP32 + MQTT + Dashboard Administrativo + Controle de Assinaturas via Firebase

## 📌 Visão Geral

O Totem Interativo IoT é uma solução física para eventos que permite gerar engajamento em redes sociais de forma automatizada, agora com **sistema de assinatura mensal** e **bloqueio automático** por data de expiração.

### Fluxo completo:

1. Usuário escaneia QR Code
2. Servidor (Render) recebe requisição `/totem/:id`
3. Servidor **verifica no Firebase** se o totem está ativo
4. Se ativo: publica `play` no MQTT e redireciona para Instagram
5. Se expirado: redireciona para página de aviso ou não executa ação
6. ESP32 recebe e executa ação (LED 2 segundos)

---

## 🏗️ Arquitetura do Sistema v3.0
Usuário → QR Code → Servidor (Render) → Firebase (verifica validade)
→ Broker MQTT → ESP32 → Ação Física → Redirecionamento Instagram

text

---

## 📂 Estrutura do Projeto
totem-server/
│
├── server.js # Servidor principal + Firebase Admin
├── package.json # Dependências
├── deploy.bat # Script de deploy
├── firebase-credentials.json # 🔐 CHAVE DO FIREBASE (NÃO COMMITAR)
├── .gitignore # Arquivos ignorados (credentials, node_modules)
│
├── clientes/ # 🟡 SERÁ REMOVIDO NA MIGRAÇÃO
│ ├── 123.txt # (apenas compatibilidade temporária)
│ └── TOTEM47.txt
│
└── views/ # Páginas do dashboard
├── login.html
├── admin.html # 🔧 Será atualizado com campo "Data de Expiração"
├── novo.html
├── editar.html # 🔧 Será atualizado com campo "Data de Expiração"
├── mensagem.html
└── expirado.html # ⚠️ NOVA página para totens bloqueados

text

---

## 🔧 Backend v3.0 (Node.js + Firebase + MQTT + Dashboard)

### Funcionalidades NOVAS:

- **Controle de validade por data** (assinatura mensal)
- **Bloqueio automático** após data de expiração
- **Firebase Firestore** como banco de dados escalável
- **Dashboard atualizado** com campo "Data de Expiração"
- **Migração gradual** dos arquivos .txt para Firebase
- **Página de aviso** para totens expirados

### Funcionalidades mantidas:

- ✅ Rota pública `/totem/:id` (agora com verificação)
- ✅ Dashboard admin `/admin/login` (senha: `159268`)
- ✅ Gerenciamento de clientes (agora com data)
- ✅ Link do QR Code visível e copiável
- ✅ Login protegido por sessão

---

## 🔥 Firebase (NOVO)

### Estrutura do Firestore:
/totens/
{ID_DO_TOTEM}:
- link: "https://instagram.com/..."
- dataExpiracao: "2026-12-31" (formato YYYY-MM-DD)
- status: "ativo" / "bloqueado" (pode ser calculado)
- cliente: "Nome do Cliente" (opcional)
- criadoEm: timestamp
- ultimaRenovacao: timestamp

text

### Regras de segurança:

```javascript
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    // Apenas o servidor (Admin SDK) tem acesso total
    // Clientes não acessam diretamente
    match /{document=**} {
      allow read, write: if false;  // Bloqueado para acesso direto
    }
  }
}
📡 Comunicação MQTT
Broker: broker.hivemq.com

Porta: 1883

Tópico: totem/{DEVICE_ID}

Mensagem: play (apenas se ativo)

🔌 Firmware ESP32 (INALTERADO)
O firmware do ESP32 não precisa ser alterado para o sistema v3.0. Ele continua recebendo o comando play via MQTT e acionando o LED.

cpp
// Código permanece o mesmo
#define DEVICE_ID "123"  // ⚠️ MUDAR POR CLIENTE
🚀 Fluxo Comercial v3.0 (NOVO)
Passo	Ação
1	Cliente contrata plano mensal
2	Você define um ID (ex: TOTEM99)
3	Altera #define DEVICE_ID "TOTEM99" no firmware
4	Grava o firmware no ESP32
5	Acessa o dashboard: /admin/novo
6	Cadastra: ID + Link do Instagram + Data de Expiração (ex: 2026-04-22)
7	Sistema salva no Firebase
8	Copia o link do QR Code no dashboard
9	Gera o QR Code e cola no totem
10	Entrega o totem instalado
Renovação:
Passo	Ação
1	Cliente paga nova mensalidade
2	Acessa dashboard /admin/editar/TOTEM99
3	Atualiza a Data de Expiração para +30 dias
4	Sistema volta a aceitar requisições
🔐 Como funciona o bloqueio por data
Na rota /totem/:id:

Busca o totem no Firebase pelo ID

Compara dataExpiracao com a data atual

Se dataExpiracao >= hoje → permite acesso

Se dataExpiracao < hoje → bloqueia

Opções de bloqueio:

Não publicar no MQTT

Redirecionar para página de aviso (/expirado)

Retornar erro 403

📊 Dashboard v3.0 (O QUE SERÁ ALTERADO)
Página NOVO Cliente (novo.html):
Adicionar campo "Data de Expiração" (input type="date")

Validação: data deve ser futura

Página EDITAR Cliente (editar.html):
Adicionar campo "Data de Expiração" preenchido

Exibir status atual (Ativo / Expirado)

Botão "Renovar por +30 dias" (atalho)

Página ADMIN (admin.html):
Nova coluna "Expira em" com a data

Nova coluna "Status" com ícone: ✅ Ativo / ❌ Bloqueado

Cálculo automático: se dataExpiracao < hoje = bloqueado

Ordenação por data de expiração

🔄 Migração dos dados existentes
Para não perder os clientes atuais:

Script de migração lerá todos os arquivos da pasta clientes/

Para cada arquivo .txt, criará um documento no Firebase

Data de expiração inicial: será definida manualmente ou padrão (ex: +30 dias)

Pasta clientes/ será mantida apenas como backup temporário

🧪 Testes necessários
Criar totem com data futura → deve funcionar

Criar totem com data passada → deve bloquear

Editar data de expiração → deve atualizar

Dashboard exibir corretamente ativos/expirados

Migração dos arquivos .txt para Firebase

🚀 Deploy e Atualização
Usar o mesmo deploy.bat:

batch
@echo off
echo ===============================
echo   Deploy Totem Server v3.0
echo ===============================
set /p msg=Mensagem do commit:
git add .
git commit -m "%msg%"
git push
pause
Cuidados:

O arquivo firebase-credentials.json NÃO deve ser commitado

Adicionar no .gitignore antes do primeiro commit

No Render, as credenciais podem ser adicionadas como variável de ambiente (ou fazer upload manual)

🔐 Segurança v3.0
Item	Status
Senha do dashboard	✅ (159268)
Sessão expira ao fechar	✅
Firebase com regras restritas	🔧 (configurar)
Credenciais fora do Git	✅ (no .gitignore)
Validação de data no backend	🔧 (implementar)
📊 Escalabilidade (1000+ totens)
Com Firebase, o sistema suporta:

✅ Milhares de totens sem perda de performance

✅ Consultas rápidas por ID

✅ Backup automático (Firebase gerencia)

✅ Sem risco de corrupção de arquivos

✅ Atualizações simultâneas seguras

✅ Controle de acesso refinado

⚠️ Observações Importantes
Firebase plano gratuito: 1 GiB de dados, 50k leituras/dia (mais que suficiente)

Render plano free: pode entrar em sleep (primeira requisição demora)

Broker público: considere um privado para produção em larga escala

Data de expiração: usar formato UTC para evitar problemas de fuso

🎯 Objetivo Comercial v3.0
Produto físico de engajamento com receita recorrente:

✅ Configuração rápida (WiFiManager)
✅ Gerenciamento via dashboard com validade
✅ Links de QR Code sempre acessíveis
✅ Bloqueio automático se não pagar
✅ Renovação simples (só alterar a data)
✅ Escalável para milhares de clientes
✅ Profissional com banco de dados seguro

📌 Status do Desenvolvimento v3.0
Firebase criado

Coleção totens criada

Estrutura de dados definida

Instalar firebase-admin no projeto

Configurar credenciais no servidor

Implementar verificação de data na rota /totem/:id

Atualizar dashboard com campo "Data de Expiração"

Criar script de migração dos .txt para Firebase

Testar bloqueio/renovação

Fazer deploy da versão 3.0

Sistema profissional, escalável e com receita recorrente! 🚀