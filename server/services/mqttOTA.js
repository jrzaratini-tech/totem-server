class MQTTOTAService {
    constructor(mqttClient) {
        this.mqttClient = mqttClient;
        this.pendingPublications = new Map();
        this.maxRetries = 3;
        this.retryDelay = 5000;
    }

    async publicarFirmwareUpdate(totemId, url, callback) {
        return new Promise((resolve, reject) => {
            if (!this.mqttClient || !this.mqttClient.connected) {
                const erro = 'MQTT nao conectado';
                console.error(`MQTT OTA erro: ${erro}`);
                if (callback) callback({ success: false, erro });
                return reject(new Error(erro));
            }

            const topico = `totem/${totemId}/firmwareUpdate`;
            const mensagem = String(url || '');

            console.log(`Publicando firmwareUpdate para ${totemId}: ${mensagem}`);

            this.mqttClient.publish(topico, mensagem, { qos: 1, retain: false }, (err) => {
                if (err) {
                    console.error(`Erro ao publicar firmwareUpdate para ${totemId}:`, err.message);
                    if (callback) callback({ success: false, erro: err.message });
                    reject(err);
                } else {
                    console.log(`firmwareUpdate publicado com sucesso: ${topico}`);
                    if (callback) callback({ success: true, totemId, topico });
                    resolve({ success: true, totemId, topico });
                }
            });
        });
    }

    async publicarFirmwareUpdateComRetry(totemId, url, tentativa = 1) {
        try {
            await this.publicarFirmwareUpdate(totemId, url);
            return { success: true, tentativas: tentativa };
        } catch (error) {
            if (tentativa < this.maxRetries) {
                console.log(`Retry ${tentativa}/${this.maxRetries} para ${totemId} em ${this.retryDelay}ms...`);
                await this.sleep(this.retryDelay);
                return this.publicarFirmwareUpdateComRetry(totemId, url, tentativa + 1);
            }

            console.error(`Falha apos ${this.maxRetries} tentativas para ${totemId}`);
            return {
                success: false,
                erro: `Falha apos ${this.maxRetries} tentativas`,
                tentativas: tentativa
            };
        }
    }

    async publicarOTAIndividual(totemId, versao, url, checksum, tamanho) {
        void versao;
        void checksum;
        void tamanho;
        return this.publicarFirmwareUpdateComRetry(totemId, url);
    }

    async publicarOTAMassa(totens, versao, url, checksum, tamanho, progressCallback) {
        const resultados = [];

        for (let i = 0; i < totens.length; i++) {
            const totemId = totens[i];

            console.log(`Progresso OTA: ${i + 1}/${totens.length} - Atualizando ${totemId}...`);

            const resultado = await this.publicarOTAIndividual(totemId, versao, url, checksum, tamanho);

            resultados.push({
                totemId,
                ...resultado
            });

            if (progressCallback) {
                await progressCallback({
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
        void versao;
        void checksum;
        return this.publicarFirmwareUpdateComRetry(totemId, url);
    }

    async publicarCancelamento(totemId) {
        const erro = 'Cancelamento OTA nao suportado pelo firmware atual';
        console.warn(`OTA cancelamento indisponivel para ${totemId}: ${erro}`);
        return { success: false, erro };
    }

    inscreverStatusOTA(totemId, callback) {
        const topico = `totem/${totemId}/ota/status`;

        this.mqttClient.subscribe(topico, { qos: 1 }, (err) => {
            if (err) {
                console.error(`Erro ao inscrever em ${topico}:`, err.message);
            } else {
                console.log(`Inscrito em ${topico}`);
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
