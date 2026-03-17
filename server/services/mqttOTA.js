class MQTTOTAService {
    constructor(mqttClient) {
        this.mqttClient = mqttClient;
        this.pendingPublications = new Map();
        this.maxRetries = 3;
        this.retryDelay = 5000;
    }

    async publicarOTA(totemId, payload, callback) {
        return new Promise((resolve, reject) => {
            if (!this.mqttClient || !this.mqttClient.connected) {
                const erro = 'MQTT não conectado';
                console.error(`❌ ${erro}`);
                if (callback) callback({ success: false, erro });
                return reject(new Error(erro));
            }

            const topico = `totem/${totemId}/ota`;
            const mensagem = JSON.stringify(payload);
            
            console.log(`📤 Publicando OTA para ${totemId}:`, payload);

            this.mqttClient.publish(topico, mensagem, { qos: 1, retain: false }, (err) => {
                if (err) {
                    console.error(`❌ Erro ao publicar OTA para ${totemId}:`, err.message);
                    if (callback) callback({ success: false, erro: err.message });
                    reject(err);
                } else {
                    console.log(`✅ OTA publicado com sucesso: ${topico}`);
                    if (callback) callback({ success: true, totemId, topico });
                    resolve({ success: true, totemId, topico });
                }
            });
        });
    }

    async publicarOTAComRetry(totemId, payload, tentativa = 1) {
        try {
            await this.publicarOTA(totemId, payload);
            return { success: true, tentativas: tentativa };
        } catch (error) {
            if (tentativa < this.maxRetries) {
                console.log(`🔄 Retry ${tentativa}/${this.maxRetries} para ${totemId} em ${this.retryDelay}ms...`);
                await this.sleep(this.retryDelay);
                return this.publicarOTAComRetry(totemId, payload, tentativa + 1);
            } else {
                console.error(`❌ Falha após ${this.maxRetries} tentativas para ${totemId}`);
                return { 
                    success: false, 
                    erro: `Falha após ${this.maxRetries} tentativas`,
                    tentativas: tentativa 
                };
            }
        }
    }

    async publicarOTAIndividual(totemId, versao, url, checksum, tamanho) {
        const payload = {
            comando: 'ota',
            versao: versao,
            url: url,
            checksum: checksum,
            tamanho: tamanho,
            timestamp: Date.now()
        };

        return this.publicarOTAComRetry(totemId, payload);
    }

    async publicarOTAMassa(totens, versao, url, checksum, tamanho, progressCallback) {
        const resultados = [];
        
        for (let i = 0; i < totens.length; i++) {
            const totemId = totens[i];
            
            console.log(`📊 Progresso: ${i + 1}/${totens.length} - Atualizando ${totemId}...`);
            
            const resultado = await this.publicarOTAIndividual(totemId, versao, url, checksum, tamanho);
            
            resultados.push({
                totemId,
                ...resultado
            });

            if (progressCallback) {
                progressCallback({
                    total: totens.length,
                    atual: i + 1,
                    totemId,
                    resultado
                });
            }

            await this.sleep(1000);
        }

        const sucessos = resultados.filter(r => r.success).length;
        const falhas = resultados.filter(r => !r.success).length;

        return {
            success: true,
            total: totens.length,
            sucessos,
            falhas,
            resultados
        };
    }

    async publicarRollback(totemId, versao, url, checksum) {
        const payload = {
            comando: 'rollback',
            versao: versao,
            url: url,
            checksum: checksum,
            timestamp: Date.now()
        };

        return this.publicarOTAComRetry(totemId, payload);
    }

    async publicarCancelamento(totemId) {
        const payload = {
            comando: 'cancel_ota',
            timestamp: Date.now()
        };

        return this.publicarOTA(totemId, payload);
    }

    inscreverStatusOTA(totemId, callback) {
        const topico = `totem/${totemId}/ota/status`;
        
        this.mqttClient.subscribe(topico, { qos: 1 }, (err) => {
            if (err) {
                console.error(`❌ Erro ao inscrever em ${topico}:`, err.message);
            } else {
                console.log(`✅ Inscrito em ${topico}`);
            }
        });

        this.mqttClient.on('message', (topic, message) => {
            if (topic === topico) {
                try {
                    const status = JSON.parse(message.toString());
                    callback(status);
                } catch (error) {
                    console.error('Erro ao parsear status OTA:', error.message);
                }
            }
        });
    }

    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    isConnected() {
        return this.mqttClient && this.mqttClient.connected;
    }
}

module.exports = MQTTOTAService;
