# 🎛️ TOTEM INTERATIVO IoT v2.0

Sistema de Engajamento com QR Code + ESP32 + MQTT + Dashboard Administrativo

## 📌 Visão Geral

O Totem Interativo IoT é uma solução física para eventos que permite gerar engajamento em redes sociais de forma automatizada.

### Fluxo completo:

1. Usuário escaneia QR Code
2. Servidor (Render) recebe requisição `/totem/:id`
3. Servidor publica `play` no MQTT
4. ESP32 recebe e executa ação (LED 2 segundos)
5. Usuário é redirecionado para Instagram do cliente

---

## 🏗️ Arquitetura do Sistema
Usuário → QR Code → Servidor (Render) → Broker MQTT → ESP32 → Ação Física → Redirecionamento Instagram

text

---

## 📂 Estrutura do Projeto
totem-server/
│
├── server.js # Servidor principal + dashboard admin
├── package.json # Dependências
├── deploy.bat # Script de deploy
├── clientes/ # PASTA COM OS LINKS DOS CLIENTES
│ ├── 123.txt # Arquivo com link do Instagram
│ └── TOTEM47.txt # ID personalizado
└── views/ # Páginas do dashboard
├── login.html
├── admin.html
├── novo.html
├── editar.html
└── mensagem.html

text

---

## 🔧 Backend (Node.js + MQTT + Dashboard)

### Funcionalidades:

- **Rota pública:** `/totem/:id` → publica MQTT + redireciona
- **Dashboard admin:** `/admin/login` (senha: `159268`)
- **Gerenciamento de clientes:** Adicionar, editar, excluir via interface web
- **Link do QR Code visível e copiável** para cada totem
- **Armazenamento:** Arquivos `.txt` na pasta `clientes/` (ID → link)

---

## 📡 Comunicação MQTT

- **Broker:** `broker.hivemq.com`
- **Porta:** `1883`
- **Tópico:** `totem/{DEVICE_ID}`
- **Mensagem:** `play`

---

## 🔌 Firmware ESP32

```cpp
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#define DEVICE_ID "123"           // ⚠️ MUDAR AQUI POR CLIENTE
#define RESET_BUTTON 0             // Botão GPIO0 (segurar 5s para reset)
#define LED_PIN 2                  // LED interno

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wm;

unsigned long buttonPressTime = 0;
bool buttonPressed = false;

void executarAcao() {
  digitalWrite(LED_PIN, HIGH);
  delay(2000);
  digitalWrite(LED_PIN, LOW);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];

  Serial.print("Mensagem: ");
  Serial.println(message);

  if (message == "play") executarAcao();
}

void conectarMQTT() {
  while (!client.connected()) {
    Serial.println("Conectando MQTT...");
    String clientId = "TOTEM-" + String(DEVICE_ID);

    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!");
      String topic = "totem/" + String(DEVICE_ID);
      client.subscribe(topic.c_str());
      Serial.print("Inscrito: ");
      Serial.println(topic);
    } else {
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  // Reset segurando botão ao ligar
  if (digitalRead(RESET_BUTTON) == LOW) {
    delay(5000);
    if (digitalRead(RESET_BUTTON) == LOW) {
      Serial.println("Resetando WiFi...");
      wm.resetSettings();
      ESP.restart();
    }
  }

  WiFiManager wm;
  bool res = wm.autoConnect("TOTEM_SETUP", "12345678");

  if (!res) {
    Serial.println("Falha WiFi. Reiniciando...");
    ESP.restart();
  }

  Serial.println("WiFi conectado!");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Reset segurando botão durante operação
  if (digitalRead(RESET_BUTTON) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressTime = millis();
    }
    if (millis() - buttonPressTime > 5000) {
      Serial.println("Resetando WiFi...");
      wm.resetSettings();
      ESP.restart();
    }
  } else {
    buttonPressed = false;
  }

  if (!client.connected()) conectarMQTT();
  client.loop();
}
⚙️ Configuração do ESP32:
Item	Descrição
DEVICE_ID	ÚNICO POR TOTEM (ex: "123", "TOTEM47")
RESET_BUTTON	Botão no pino 0 (segurar 5s para resetar WiFi)
LED_PIN	Pino do LED (2 = LED interno)
WiFi	Gerenciado pelo WiFiManager (portal captive)
📱 Primeira inicialização do ESP32:
Liga o ESP32

Aparece rede WiFi TOTEM_SETUP

Conecta com senha 12345678

Configura o WiFi local

Pronto! Conecta automaticamente depois

🌐 URL para QR Code
text
https://SEUAPP.onrender.com/totem/ID_DO_TOTEM
Exemplos:

https://totem-server.onrender.com/totem/123

https://totem-server.onrender.com/totem/TOTEM47

No dashboard: O link completo já aparece na tabela e pode ser copiado com um clique.

🖥️ Dashboard Administrativo
Acessar:
text
https://SEUAPP.onrender.com/admin/login
Senha: 159268

Funcionalidades:
✅ Listar todos os totens cadastrados

✅ Ver o link do QR Code de cada totem

✅ Copiar link do QR Code com um clique

✅ Adicionar novo cliente (ID + Instagram)

✅ Editar link existente

✅ Excluir totem

✅ Login protegido

🚀 Fluxo Comercial (Novo Cliente)
Passo	Ação
1	Cliente compra o totem
2	Você define um ID (ex: TOTEM99)
3	Altera #define DEVICE_ID "TOTEM99" no código
4	Grava o firmware no ESP32
5	Acessa o dashboard: /admin/novo
6	Cadastra o mesmo ID e link do Instagram
7	Copia o link do QR Code no dashboard
8	Gera o QR Code e cola no totem
9	Entrega o totem instalado
Tempo total: < 5 minutos por cliente

🔄 Atualização do Servidor (Deploy)
Use o deploy.bat:

batch
@echo off
echo ===============================
echo   Deploy Totem Server v2.0
echo ===============================
set /p msg=Mensagem do commit:
git add .
git commit -m "%msg%"
git push
pause
O Render faz deploy automático.

🔐 Segurança
✅ Dashboard com senha (159268)

✅ Sessão expira ao fechar navegador

✅ Rotas admin protegidas

✅ Validação de link (precisa conter instagram.com)

📊 Escalabilidade (300+ totens)
Arquitetura atual suporta centenas de dispositivos:

IDs únicos por cliente

Arquivos individuais na pasta clientes/

Dashboard para gerenciamento fácil

Links de QR Code sempre disponíveis

⚠️ Observações Importantes
Plano Free do Render pode entrar em sleep (primeira requisição demora)

Broker público não é recomendado para produção em larga escala

LED pisca 2 segundos ao receber comando

🎯 Objetivo Comercial
Produto físico de engajamento para eventos:

✅ Configuração rápida (WiFiManager)

✅ Gerenciamento via dashboard

✅ Links de QR Code sempre acessíveis

✅ Escalável para centenas de clientes

✅ Sem dependência de APIs externas

📌 Status Atual v2.0
✔️ ESP32 com WiFiManager e botão de reset

✔️ MQTT funcional

✔️ Backend com rotas públicas

✔️ Dashboard administrativo completo

✔️ Links de QR Code visíveis e copiáveis

✔️ Gerenciamento via arquivos .txt

✔️ Sistema pronto para comercialização

Sistema completo, profissional e escalável! 🚀

text
