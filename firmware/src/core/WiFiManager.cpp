#include "core/WiFiManager.h"
#include "Config.h"

WiFiManager::WiFiManager() : server(80) {
    apMode = false;
    connected = false;
    lastReconnectAttempt = 0;
    connecting = false;
    connectStartMs = 0;
}

void WiFiManager::begin(const String &totemId) {
    this->totemId = totemId;
    pinMode(PIN_BTN_RESET_WIFI, INPUT_PULLUP);

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    if (!connectToSaved()) {
        startAPMode();
    }

    if (apMode) {
        dnsServer.start(53, "*", WiFi.softAPIP());
        setupWebServer();
    }
}

bool WiFiManager::connectToSaved() {
    preferences.begin("wifi", true);
    String ssid = preferences.getString("ssid", "");
    String pass = preferences.getString("pass", "");
    preferences.end();

    if (ssid.length() == 0) return false;
    startConnect(ssid, pass);
    return true;
}

bool WiFiManager::connect(const String &ssid, const String &password) {
    startConnect(ssid, password);
    return true;
}

void WiFiManager::startConnect(const String &ssid, const String &password) {
    apMode = false;
    connected = false;
    connecting = true;
    connectStartMs = millis();
    pendingSsid = ssid;
    pendingPass = password;

    WiFi.mode(WIFI_STA);
    WiFi.begin(pendingSsid.c_str(), pendingPass.c_str());
}

void WiFiManager::startAPMode() {
    WiFi.mode(WIFI_AP);
    String apSsid = String("Totem-Config-") + totemId;
    WiFi.softAP(apSsid.c_str(), "12345678");
    apMode = true;
    connected = false;
    connecting = false;
}

void WiFiManager::setupWebServer() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String html;
        html += "<html><head><meta name='viewport' content='width=device-width'>";
        html += "<style>body{font-family:Arial;text-align:center;padding:20px}input{padding:10px;margin:6px;width:90%}</style>";
        html += "</head><body>";
        html += "<h2>Configurar WiFi do Totem</h2>";
        html += "<form method='POST' action='/connect'>";
        html += "<input name='ssid' placeholder='SSID'/>";
        html += "<input type='password' name='password' placeholder='Senha'/>";
        html += "<hr style='margin:16px 0; opacity:0.3'/>";
        html += "<h3>Identificação do dispositivo</h3>";
        html += "<input name='totemId' placeholder='Totem ID' value='" + totemId + "'/>";
        html += "<input name='token' placeholder='Token (opcional)' value=''/>";
        html += "<input type='submit' value='Conectar'/>";
        html += "</form></body></html>";
        request->send(200, "text/html", html);
    });

    server.on("/connect", HTTP_POST, [this](AsyncWebServerRequest *request) {
        String ssid = request->arg("ssid");
        String password = request->arg("password");
        String newTotemId = request->arg("totemId");
        String token = request->arg("token");

        newTotemId.trim();
        token.trim();

        if (newTotemId.length() > 0) {
            // Salva o provisionamento do dispositivo
            preferences.begin("device", false);
            preferences.putString("id", newTotemId);
            preferences.putString("token", token);
            preferences.end();

            totemId = newTotemId;
        }
        request->send(200, "text/html", "<html><body><h3>Conectando...</h3><p>Se conectar, o AP vai desligar.</p></body></html>");

        startConnect(ssid, password);
    });

    server.begin();
}

void WiFiManager::loop() {
    if (apMode) {
        dnsServer.processNextRequest();
    }

    // Máquina de estados de conexão (não-bloqueante)
    if (connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            connecting = false;
            connected = true;

            // Salva credenciais
            preferences.begin("wifi", false);
            preferences.putString("ssid", pendingSsid);
            preferences.putString("pass", pendingPass);
            preferences.end();

            // Se estava em AP, encerra
            if (apMode) {
                apMode = false;
                dnsServer.stop();
                server.end();
            }
        } else if (millis() - connectStartMs > WIFI_TIMEOUT) {
            connecting = false;
            connected = false;
            WiFi.disconnect(true);
            startAPMode();
            dnsServer.start(53, "*", WiFi.softAPIP());
            setupWebServer();
        }
    }

    if (!apMode) {
        if (WiFi.status() != WL_CONNECTED) {
            connected = false;
            if (millis() - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
                // tenta reconectar usando credenciais salvas
                connectToSaved();
                lastReconnectAttempt = millis();
            }
        } else {
            connected = true;
        }
    }

    // Reset WiFi por botão (sem delay)
    static unsigned long resetPressStart = 0;
    const bool pressed = digitalRead(PIN_BTN_RESET_WIFI) == LOW;
    if (pressed) {
        if (resetPressStart == 0) resetPressStart = millis();
        if (millis() - resetPressStart >= (unsigned long)DEBOUNCE_DELAY) {
            resetWiFiSettings();
            resetPressStart = 0;
        }
    } else {
        resetPressStart = 0;
    }
}

void WiFiManager::resetWiFiSettings() {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();

    WiFi.disconnect(true);
    startAPMode();

    dnsServer.start(53, "*", WiFi.softAPIP());
    setupWebServer();
}

bool WiFiManager::isConnected() const {
    return connected && WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isAPMode() const { return apMode; }

String WiFiManager::getIP() const {
    if (WiFi.status() != WL_CONNECTED) return "0.0.0.0";
    return WiFi.localIP().toString();
}

int WiFiManager::getRSSI() const {
    if (WiFi.status() != WL_CONNECTED) return -127;
    return WiFi.RSSI();
}
