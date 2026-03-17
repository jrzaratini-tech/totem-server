const { validarFirmware, calcularChecksum, extrairVersao } = require('../../server/utils/firmwareValidator');
const fs = require('fs');
const path = require('path');

describe('Firmware Validator Tests', () => {
    const testFirmwarePath = path.join(__dirname, 'test-firmware-v1.0.0.bin');

    beforeAll(() => {
        const buffer = Buffer.alloc(200 * 1024);
        buffer[0] = 0xE9;
        fs.writeFileSync(testFirmwarePath, buffer);
    });

    afterAll(() => {
        if (fs.existsSync(testFirmwarePath)) {
            fs.unlinkSync(testFirmwarePath);
        }
    });

    test('deve validar firmware válido', () => {
        const resultado = validarFirmware(testFirmwarePath);
        expect(resultado.valido).toBe(true);
        expect(resultado.versao).toBe('1.0.0');
        expect(resultado.checksum).toBeDefined();
    });

    test('deve rejeitar arquivo inexistente', () => {
        const resultado = validarFirmware('arquivo-inexistente.bin');
        expect(resultado.valido).toBe(false);
        expect(resultado.erro).toContain('não encontrado');
    });

    test('deve extrair versão do nome do arquivo', () => {
        const versao = extrairVersao('firmware-v4.3.0.bin');
        expect(versao).toBe('4.3.0');
    });

    test('deve calcular checksum MD5', () => {
        const checksum = calcularChecksum(testFirmwarePath);
        expect(checksum).toBeDefined();
        expect(checksum.length).toBe(32);
    });
});

console.log('✅ Testes de validação de firmware configurados');
console.log('Execute com: npm test');
