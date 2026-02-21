🎛️ TOTEM INTERATIVO IoT

Sistema de Engajamento com QR Code + ESP32 + MQTT

📌 Visão Geral

O Totem Interativo IoT é uma solução física para eventos que permite gerar engajamento em redes sociais de forma automatizada.

Quando um usuário escaneia um QR Code:

A requisição passa pelo servidor (Render)

O servidor publica uma mensagem MQTT

O ESP32 correspondente recebe o comando

O totem executa ação física (LED / Som / Efeito)

O usuário é redirecionado para o Instagram do cliente

O sistema foi projetado para:

Funcionar em qualquer evento com Wi-Fi local

Atender múltiplos clientes (até 300+ totens)

Não depender da API do Instagram

Ser escalável e comercialmente viável

🏗️ Arquitetura do Sistema
Usuário
   ↓
QR Code
   ↓
Servidor (Render - Node.js)
   ↓
Broker MQTT
   ↓
ESP32 (Totem)
   ↓
Ação Física
   ↓
Redirecionamento para Instagram
📂 Estrutura do Projeto
totem-server/
│
├── server.js
├── package.json
├── deploy.bat
└── README.md
🔧 Backend (Node.js + MQTT)
📄 server.js

Recebe requisição via /totem/:id

Publica play no tópico MQTT correspondente

Redireciona para Instagram fixo do cliente

Exemplo atual configurado:

const clientes = {
  "123": "https://www.instagram.com/printpixel_grafica/"
};

Tópico MQTT utilizado:

totem/123
📡 Comunicação MQTT

Broker utilizado (teste):

broker.hivemq.com
porta: 1883

Cada totem se inscreve em:

totem/{DEVICE_ID}

Exemplo:

totem/123

Quando o servidor publica:

play

O ESP executa a ação física.

🔌 Firmware ESP32
Configuração essencial
#define DEVICE_ID "123"

O valor precisa ser idêntico ao ID usado no servidor e na URL.

🌐 URL para QR Code

Formato padrão:

https://SEUAPP.onrender.com/totem/ID

Exemplo real:

https://totem-server.onrender.com/totem/123
🔁 Fluxo Completo de Execução

Usuário escaneia QR

Acessa /totem/123

Servidor publica:

totem/123 → play

ESP32 recebe mensagem

LED pisca

Usuário é redirecionado para:

https://www.instagram.com/printpixel_grafica/
🚀 Deploy
Atualizar servidor

Use o arquivo:

deploy.bat

Ele:

Adiciona alterações

Pede mensagem personalizada

Faz commit

Executa push

Render faz deploy automático

📊 Escalabilidade (até 300 totens)

Arquitetura atual suporta múltiplos dispositivos.

Para adicionar novos clientes:

Definir novo ID

Atualizar clientes no server.js

Gerar QR correspondente

Gravar firmware com mesmo ID

Exemplo:

const clientes = {
  "123": "https://instagram.com/clienteA",
  "124": "https://instagram.com/clienteB",
  "125": "https://instagram.com/clienteC"
};
🔐 Segurança (Próxima Evolução)

Para produção real com 300 unidades recomenda-se:

Broker MQTT privado (EMQX Cloud / HiveMQ Cloud)

Autenticação MQTT

Token na URL para evitar spam

Rate limit

Monitoramento de uptime

⚠️ Observações Importantes

Plano Free do Render pode entrar em sleep

Primeira requisição pode demorar alguns segundos

Broker público não é recomendado para produção

🎯 Objetivo Comercial

Transformar o Totem em:

Produto físico de engajamento para eventos

Solução white-label

Plataforma escalável para múltiplas marcas

📌 Status Atual

✔ ESP32 configurado
✔ MQTT funcional
✔ Backend funcional
✔ QR Code operacional
✔ Redirecionamento validado

Sistema completo e funcional.