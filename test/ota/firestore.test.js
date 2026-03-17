const FirebaseOTAService = require('../../server/services/firebaseOTA');

describe('Firebase OTA Service Tests', () => {
    let firebaseOTA;
    let mockDb;
    let mockBucket;

    beforeEach(() => {
        mockDb = {
            collection: jest.fn().mockReturnThis(),
            doc: jest.fn().mockReturnThis(),
            get: jest.fn(),
            set: jest.fn(),
            update: jest.fn()
        };

        mockBucket = {
            upload: jest.fn().mockResolvedValue([]),
            file: jest.fn().mockReturnValue({
                getSignedUrl: jest.fn().mockResolvedValue(['https://signed-url.com'])
            })
        };

        firebaseOTA = new FirebaseOTAService();
        firebaseOTA.db = mockDb;
        firebaseOTA.bucket = mockBucket;
    });

    test('deve criar job em massa', async () => {
        mockDb.set.mockResolvedValue();

        const resultado = await firebaseOTA.criarJobMassa(
            ['totem1', 'totem2', 'totem3'],
            'v4.3.0',
            null,
            'admin@test.com'
        );

        expect(resultado.success).toBe(true);
        expect(resultado.jobId).toBeDefined();
    });

    test('deve atualizar status OTA', async () => {
        mockDb.update.mockResolvedValue();
        mockDb.get.mockResolvedValue({
            data: () => ({ firmware: { historico: [] } })
        });

        const resultado = await firebaseOTA.atualizarStatusOTA('totem123', 'success', 'v4.3.0');
        expect(resultado.success).toBe(true);
    });

    test('deve obter histórico de versões', async () => {
        mockDb.get.mockResolvedValue({
            exists: true,
            data: () => ({
                firmware: {
                    atual: 'v4.3.0',
                    historico: [
                        { versao: 'v4.2.0', status: 'success' },
                        { versao: 'v4.3.0', status: 'success' }
                    ]
                }
            })
        });

        const resultado = await firebaseOTA.obterHistoricoVersoes('totem123');
        expect(resultado.success).toBe(true);
        expect(resultado.versaoAtual).toBe('v4.3.0');
        expect(resultado.historico.length).toBe(2);
    });

    test('deve atualizar progresso do job', async () => {
        mockDb.get.mockResolvedValue({
            exists: true,
            data: () => ({
                progresso: {
                    total: 10,
                    concluidos: 5,
                    falhas: 0
                }
            })
        });
        mockDb.update.mockResolvedValue();

        const resultado = await firebaseOTA.atualizarProgressoJob('job123', 'totem1', 'success');
        expect(resultado.success).toBe(true);
    });

    test('deve registrar auditoria', async () => {
        mockDb.collection.mockReturnValue({
            add: jest.fn().mockResolvedValue()
        });

        const resultado = await firebaseOTA.registrarAuditoria('ota_individual', 'admin', {
            totemId: 'totem123',
            versao: 'v4.3.0'
        });

        expect(resultado.success).toBe(true);
    });
});

console.log('✅ Testes Firebase OTA configurados');
console.log('Execute com: npm test');
