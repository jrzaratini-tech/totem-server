# Como Alterar a Senha do Painel Admin

## 📋 Visão Geral

A senha do painel administrativo agora é armazenada no **Firebase Firestore** e só pode ser alterada diretamente no banco de dados. Isso garante maior segurança, pois não há interface web para alteração de senha.

## 🔐 Senha Atual (Padrão)

- **Senha padrão:** `159268`
- Esta senha será usada caso não exista configuração no Firebase

## 🔧 Como Alterar a Senha no Firebase

### Passo 1: Acessar o Firebase Console

1. Acesse [Firebase Console](https://console.firebase.google.com/)
2. Selecione seu projeto
3. No menu lateral, clique em **Firestore Database**

### Passo 2: Criar/Editar o Documento de Configuração

#### Se o documento ainda não existe:

1. Clique em **"Iniciar coleção"** (se for a primeira vez) ou **"Adicionar coleção"**
2. Digite o ID da coleção: `config`
3. Clique em **"Próximo"**
4. Configure o primeiro documento:
   - **ID do documento:** `admin`
   - **Campo:** `senha`
   - **Tipo:** `string`
   - **Valor:** Digite sua nova senha (ex: `minhaSenhaSegura123`)
5. Clique em **"Salvar"**

#### Se o documento já existe:

1. Navegue até a coleção `config`
2. Clique no documento `admin`
3. Edite o campo `senha`
4. Digite a nova senha
5. Clique em **"Atualizar"**

### Passo 3: Testar a Nova Senha

1. Acesse o painel admin: `https://seu-servidor.com/admin/login`
2. Digite a nova senha
3. Verifique se o login funciona corretamente

## 📊 Estrutura no Firestore

```
Firestore Database
└── config (coleção)
    └── admin (documento)
        └── senha: "suaSenhaAqui"
```

## ⚠️ Importante

- **Segurança:** Escolha uma senha forte com letras, números e caracteres especiais
- **Backup:** Anote a senha em local seguro
- **Sem recuperação automática:** Não há sistema de recuperação de senha. Se esquecer, precisará alterar novamente no Firebase
- **Efeito imediato:** A alteração é aplicada instantaneamente, sem necessidade de reiniciar o servidor

## 🔄 Senha de Fallback

Se o Firebase estiver indisponível ou o documento não existir, o sistema usará a senha padrão `159268` como fallback. Para desativar isso, você pode remover a constante `SENHA_ADMIN_FALLBACK` do código.

## 💡 Dicas de Segurança

1. **Nunca compartilhe** a senha do admin
2. **Altere regularmente** a senha (recomendado: a cada 3 meses)
3. **Use senhas fortes** com no mínimo 12 caracteres
4. **Não use** senhas óbvias como datas ou sequências numéricas
5. **Considere usar** um gerenciador de senhas

## 🆘 Problemas Comuns

### Não consigo fazer login com a nova senha

- Verifique se salvou corretamente no Firebase
- Confirme que está editando o documento correto (`config/admin`)
- Verifique se o campo se chama exatamente `senha` (minúsculo)
- Tente limpar o cache do navegador

### Firebase não está conectado

- Verifique os logs do servidor
- Confirme que as credenciais do Firebase estão corretas
- O sistema usará a senha padrão `159268` como fallback

### Esqueci a senha

1. Acesse o Firebase Console
2. Navegue até `config/admin`
3. Visualize ou altere o campo `senha`
