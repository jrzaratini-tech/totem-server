🎛️ TOTEM INTERATIVO IoT

Sistema de Engajamento com QR Code + ESP32 + MQTT

📌 Visão Geral

O Totem Interativo IoT é um dispositivo baseado em ESP32 que permite interação em eventos através de QR Code.

Quando um usuário escaneia o QR Code do Instagram:

O acesso passa por um servidor intermediário

O servidor publica um evento via MQTT

O ESP32 recebe o comando em tempo real

O totem executa ação (som, LED, efeitos)

O sistema foi projetado para:

Funcionar em eventos com redes diferentes

Permitir reconfiguração rápida de Wi-Fi

Ser escalável para múltiplos totens

Operar em tempo real

🏗️ Arquitetura do Sistema
Usuário
   ↓
QR Code (link rastreável)
   ↓
Servidor Backend
   ↓
Broker MQTT
   ↓
ESP32 (Totem)
   ↓
Ação Física (LED / Som / Efeito)
🔧 Hardware

ESP32

Botão físico de RESET

Módulo de som

LEDs / efeitos visuais

Fonte de alimentação adequada

🌐 Configuração Inicial (Cliente Final)
1️⃣ Primeiro uso

Ao ligar o dispositivo:

Se não houver Wi-Fi salvo, o ESP inicia em modo Access Point

Rede criada:

TOTEM_SETUP
Senha: 12345678

O manual acompanha um QR Code padrão para conexão automática.

2️⃣ Configuração da Rede

Cliente conecta na rede TOTEM_SETUP

Abre automaticamente 192.168.4.1

Seleciona a rede Wi-Fi do local

Insere senha

Dispositivo reinicia e conecta à internet

🔁 Troca de Rede (Novo Evento)

O dispositivo possui botão físico de reset.

Para redefinir:

Pressionar botão por 5 segundos

Credenciais Wi-Fi são apagadas

Dispositivo retorna ao modo configuração

Se a conexão falhar por 30 segundos, o dispositivo entra automaticamente em modo configuração.

📡 Comunicação MQTT
Broker

Endereço: broker.seudominio.com

Porta: 1883 (ou 8883 com TLS)

Tópico
totem/{ID_DO_TOTEM}

Exemplo:

totem/123
Payload esperado
play

Ao receber play, o dispositivo executa a ação configurada.

🖥️ Backend

Quando o QR Code do Instagram é acessado:

O servidor registra o acesso

Publica mensagem MQTT no tópico correspondente

Redireciona o usuário para o Instagram

Exemplo de fluxo:

seudominio.com/totem/123

Servidor:

Publica MQTT → totem/123

Redireciona → Instagram

🔐 Segurança

Recomendado para ambiente de produção:

MQTT com autenticação (usuário/senha)

TLS (porta 8883)

ID único por dispositivo

Watchdog ativo

Reconexão automática Wi-Fi e MQTT

OTA para atualização remota

🔄 Reconexão Automática

O firmware deve:

Reconectar Wi-Fi caso perca sinal

Reconectar MQTT automaticamente

Reiniciar em caso de falha crítica

📊 Escalabilidade

O sistema suporta múltiplos totens simultaneamente:

Cada dispositivo possui ID único

Cada evento pode ter QR exclusivo

Backend pode registrar métricas por local

🚀 Recursos Futuramente Integráveis

Dashboard de métricas

Atualização OTA

Ranking por evento

Sistema gamificado

Captura de leads

Integração com CRM

📦 Estrutura do Projeto
/firmware
   main.ino
   wifi_manager.cpp
   mqtt_handler.cpp

/backend
   server.js
   mqtt_publish.js
🧠 Objetivo do Produto

Criar uma solução escalável para eventos que:

Gera engajamento real

Produz métricas mensuráveis

Funciona em qualquer local

Não depende de API do Instagram

Permite modelo SaaS

📄 Licença

Uso comercial permitido mediante autorização do desenvolvedor.

Se quiser, posso gerar agora:

🔐 Versão README voltada para investidores

📦 Versão técnica detalhada para desenvolvedores

📘 Manual simplificado para cliente final

🚀 Modelo de apresentação comercial do produto

Qual você quer preparar agora?