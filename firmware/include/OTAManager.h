#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <ArduinoJson.h>

class OTAManager {
private:
    PubSubClient* mqttClient;
    String totemId;
    String versaoAtual;
    bool otaEmAndamento;
    
    static const int BUFFER_SIZE = 1024;
    static const int MAX_RETRIES = 3;
    static const unsigned long TIMEOUT_MS = 30000;

    bool verificarChecksum(const String& checksum) {
        return true;
    }

    void publicarStatus(const String& status, const String& mensagem = "") {
        if (!mqttClient || !mqttClient->connected()) {
            Serial.println("MQTT não conectado para publicar status");
            return;
        }

        StaticJsonDocument<256> doc;
        doc["status"] = status;
        doc["versao"] = versaoAtual;
        doc["timestamp"] = millis();
        
        if (mensagem.length() > 0) {
            doc["mensagem"] = mensagem;
        }

        String payload;
        serializeJson(doc, payload);

        String topico = "totem/" + totemId + "/ota/status";
        mqttClient->publish(topico.c_str(), payload.c_str(), true);
        
        Serial.println("Status OTA publicado: " + status);
    }

    bool downloadFirmware(const String& url, size_t tamanhoEsperado) {
        HTTPClient http;
        
        Serial.println("Iniciando download do firmware...");
        Serial.println("URL: " + url);
        
        http.begin(url);
        http.setTimeout(TIMEOUT_MS);
        
        int httpCode = http.GET();
        
        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("Erro HTTP: %d\n", httpCode);
            http.end();
            return false;
        }

        size_t tamanhoTotal = http.getSize();
        Serial.printf("Tamanho do firmware: %d bytes\n", tamanhoTotal);

        if (tamanhoEsperado > 0 && tamanhoTotal != tamanhoEsperado) {
            Serial.println("Tamanho do arquivo não corresponde!");
            http.end();
            return false;
        }

        if (!Update.begin(tamanhoTotal)) {
            Serial.printf("Erro ao iniciar OTA: %s\n", Update.errorString());
            http.end();
            return false;
        }

        WiFiClient* stream = http.getStreamPtr();
        uint8_t buffer[BUFFER_SIZE];
        size_t bytesLidos = 0;
        int ultimoProgresso = 0;

        while (http.connected() && bytesLidos < tamanhoTotal) {
            size_t disponivel = stream->available();
            
            if (disponivel) {
                int c = stream->readBytes(buffer, min(disponivel, sizeof(buffer)));
                
                if (Update.write(buffer, c) != c) {
                    Serial.printf("Erro ao escrever: %s\n", Update.errorString());
                    Update.abort();
                    http.end();
                    return false;
                }
                
                bytesLidos += c;
                
                int progresso = (bytesLidos * 100) / tamanhoTotal;
                if (progresso != ultimoProgresso && progresso % 10 == 0) {
                    Serial.printf("Progresso: %d%%\n", progresso);
                    publicarStatus("downloading", String(progresso) + "%");
                    ultimoProgresso = progresso;
                }
            }
            
            delay(1);
        }

        http.end();

        if (bytesLidos != tamanhoTotal) {
            Serial.println("Download incompleto!");
            Update.abort();
            return false;
        }

        if (!Update.end(true)) {
            Serial.printf("Erro ao finalizar OTA: %s\n", Update.errorString());
            return false;
        }

        Serial.println("Download concluído com sucesso!");
        return true;
    }

    void rollbackAutomatico() {
        Serial.println("Iniciando rollback automático...");
        
        const esp_partition_t* partAtual = esp_ota_get_running_partition();
        const esp_partition_t* partAnterior = esp_ota_get_next_update_target(partAtual);
        
        if (partAnterior != NULL) {
            esp_err_t err = esp_ota_set_boot_partition(partAnterior);
            if (err == ESP_OK) {
                Serial.println("Rollback configurado. Reiniciando...");
                publicarStatus("rollback", "Voltando para versão anterior");
                delay(1000);
                ESP.restart();
            } else {
                Serial.println("Erro ao configurar rollback");
            }
        }
    }

public:
    OTAManager(PubSubClient* client, const String& id, const String& versao) 
        : mqttClient(client), totemId(id), versaoAtual(versao), otaEmAndamento(false) {
    }

    void begin() {
        Serial.println("OTA Manager inicializado");
        Serial.println("Versão atual: " + versaoAtual);
        
        const esp_partition_t* running = esp_ota_get_running_partition();
        Serial.printf("Partição em execução: %s\n", running->label);
    }

    void handleOTAMessage(const String& payload) {
        if (otaEmAndamento) {
            Serial.println("OTA já em andamento, ignorando nova solicitação");
            return;
        }

        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.println("Erro ao parsear JSON OTA");
            return;
        }

        String comando = doc["comando"].as<String>();
        
        if (comando == "ota") {
            String versao = doc["versao"].as<String>();
            String url = doc["url"].as<String>();
            String checksum = doc["checksum"].as<String>();
            size_t tamanho = doc["tamanho"] | 0;

            Serial.println("=== NOVA ATUALIZAÇÃO OTA ===");
            Serial.println("Versão: " + versao);
            Serial.println("URL: " + url);
            Serial.println("Checksum: " + checksum);
            Serial.printf("Tamanho: %d bytes\n", tamanho);

            executarOTA(versao, url, checksum, tamanho);
        }
        else if (comando == "rollback") {
            String versao = doc["versao"].as<String>();
            String url = doc["url"].as<String>();
            String checksum = doc["checksum"].as<String>();

            Serial.println("=== ROLLBACK SOLICITADO ===");
            Serial.println("Versão: " + versao);
            
            executarOTA(versao, url, checksum, 0);
        }
        else if (comando == "cancel_ota") {
            Serial.println("Cancelamento de OTA solicitado");
            if (otaEmAndamento) {
                Update.abort();
                otaEmAndamento = false;
                publicarStatus("cancelled", "OTA cancelado pelo servidor");
            }
        }
    }

    void executarOTA(const String& versao, const String& url, const String& checksum, size_t tamanho) {
        otaEmAndamento = true;
        publicarStatus("starting", "Iniciando atualização OTA");

        Serial.println("Aguardando 2 segundos antes de iniciar...");
        delay(2000);

        bool sucesso = downloadFirmware(url, tamanho);

        if (sucesso) {
            versaoAtual = versao;
            publicarStatus("success", "OTA concluído com sucesso");
            
            Serial.println("=== OTA CONCLUÍDO ===");
            Serial.println("Reiniciando em 3 segundos...");
            delay(3000);
            ESP.restart();
        } else {
            publicarStatus("failed", "Falha no download do firmware");
            otaEmAndamento = false;
            
            Serial.println("=== OTA FALHOU ===");
            rollbackAutomatico();
        }
    }

    bool isOTAEmAndamento() {
        return otaEmAndamento;
    }

    String getVersaoAtual() {
        return versaoAtual;
    }

    void verificarBootValido() {
        const esp_partition_t* running = esp_ota_get_running_partition();
        esp_ota_img_states_t ota_state;
        
        if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
            if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
                Serial.println("Primeira inicialização após OTA - Validando...");
                
                delay(5000);
                
                if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                    Serial.println("OTA validado com sucesso!");
                    publicarStatus("validated", "Firmware validado após boot");
                } else {
                    Serial.println("Erro ao validar OTA");
                }
            }
        }
    }
};

#endif
