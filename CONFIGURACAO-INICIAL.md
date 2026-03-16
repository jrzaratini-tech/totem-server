# 🚀 Configuração Inicial do Sistema Totem

## ⚠️ IMPORTANTE: Configuração de Senha Admin

**O sistema NÃO possui senha padrão por questões de segurança.**

Você DEVE configurar a senha do admin antes de usar o sistema.

## 🔐 Opções de Configuração de Senha

### Opção 1: Firebase Firestore (Recomendado)

1. Acesse o [Firebase Console](https://console.firebase.google.com/)
2. Selecione seu projeto
3. Vá em **Firestore Database**
4. Crie a coleção `config` (se não existir)
5. Dentro de `config`, crie o documento `admin`
6. Adicione o campo:
   - **Nome:** `senha`
   - **Tipo:** `string`
   - **Valor:** Sua senha segura (ex: `MinhaSenh@Segura2024`)

**Estrutura no Firestore:**
```
Firestore Database
└── config (coleção)
    └── admin (documento)
        └── senha: "sua_senha_aqui"
```

### Opção 2: Variável de Ambiente

1. Crie/edite o arquivo `.env` na raiz do projeto
2. Adicione a linha:
   ```
   ADMIN_PASSWORD=sua_senha_segura_aqui
   ```
3. Reinicie o servidor

**Nota:** A senha do Firebase tem prioridade sobre a variável de ambiente.

## 📝 Outras Configurações Necessárias

### 1. Firebase Credentials

Configure as credenciais do Firebase de uma das formas:

**Opção A - Arquivo:**
- Coloque o arquivo `firebase-credentials.json` na raiz do projeto

**Opção B - Variável de ambiente:**
```
FIREBASE_CREDENTIALS={"type":"service_account",...}
```

### 2. Variáveis de Ambiente (.env)

Copie o arquivo `.env.example` para `.env` e configure:

```env
PORT=3000
SERVER_URL=https://seu-dominio.com
SESSION_SECRET=uma-chave-secreta-aleatoria
ADMIN_PASSWORD=sua_senha_admin
FIREBASE_CREDENTIALS=
```

## 🎯 Primeiro Acesso

1. Inicie o servidor: `npm start`
2. Acesse: `http://localhost:3000/admin/login`
3. Use a senha configurada no Firebase ou no `.env`

## 🔒 Dicas de Segurança

- ✅ Use senhas fortes (mínimo 12 caracteres)
- ✅ Combine letras maiúsculas, minúsculas, números e símbolos
- ✅ Não compartilhe a senha
- ✅ Altere a senha regularmente
- ❌ Nunca use senhas óbvias ou sequências numéricas

## 📚 Documentação Adicional

- [Manual de Instalação](docs/manual-instalacao.md)
- [Como Alterar Senha Admin](docs/alterar-senha-admin.md)
- [Referência da API](docs/api-reference.md)

## 🆘 Problemas Comuns

### "Senha incorreta" ao fazer login

- Verifique se configurou a senha no Firebase ou no `.env`
- Confirme que não há espaços extras na senha
- Verifique os logs do servidor para ver qual fonte de senha está sendo usada

### "Nenhuma senha configurada"

- Configure a senha no Firebase (config/admin/senha) ou
- Configure a variável `ADMIN_PASSWORD` no arquivo `.env`

### Firebase não conecta

- Verifique se o arquivo `firebase-credentials.json` está correto
- Confirme que as credenciais têm permissões adequadas
- Use a variável de ambiente `ADMIN_PASSWORD` como alternativa
