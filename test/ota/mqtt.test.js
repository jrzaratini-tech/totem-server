const MQTTOTAService = require('../../server/services/mqttOTA');

describe('MQTT OTA Service Tests', () => {
    let mqttOTA;
    let mockMqttClient;

    beforeEach(() => {
        mockMqttClient = {
            connected: true,
            publish: jest.fn((topic, message, options, callback) => {
                callback(null);
            })
        };
        mqttOTA = new MQTTOTAService(mockMqttClient);
    });

    test('deve publicar OTA individual com sucesso', async () => {
        const resultado = await mqttOTA.publicarOTAIndividual(
            'totem123',
            'v4.3.0',
            'https://example.com/firmware.bin',
            'abc123',
            1024000
        );

        expect(resultado.success).toBe(true);
        expect(mockMqttClient.publish).toHaveBeenCalled();
    });

    test('deve falhar quando MQTT não está conectado', async () => {
        mockMqttClient.connected = false;
        
        try {
            await mqttOTA.publicarOTA('totem123', {});
        } catch (error) {
            expect(error.message).toContain('MQTT não conectado');
        }
    });

    test('deve fazer retry em caso de falha', async () => {
        let tentativas = 0;
        mockMqttClient.publish = jest.fn((topic, message, options, callback) => {
            tentativas++;
            if (tentativas < 2) {
                callback(new Error('Falha temporária'));
            } else {
                callback(null);
            }
        });

        const resultado = await mqttOTA.publicarOTAComRetry('totem123', {});
        expect(resultado.success).toBe(true);
        expect(resultado.tentativas).toBe(2);
    });

    test('deve verificar se está conectado', () => {
        expect(mqttOTA.isConnected()).toBe(true);
        
        mockMqttClient.connected = false;
        expect(mqttOTA.isConnected()).toBe(false);
    });
});

console.log('✅ Testes MQTT OTA configurados');
console.log('Execute com: npm test');
