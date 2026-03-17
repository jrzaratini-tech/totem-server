const fs = require('fs');
const crypto = require('crypto');
const path = require('path');

const ESP32_MAGIC_NUMBER = 0xE9;
const MAX_FIRMWARE_SIZE = 4 * 1024 * 1024;
const MIN_FIRMWARE_SIZE = 100 * 1024;

function validarFirmware(filePath) {
    try {
        if (!fs.existsSync(filePath)) {
            return {
                valido: false,
                erro: 'Arquivo não encontrado'
            };
        }

        const stats = fs.statSync(filePath);
        
        if (stats.size > MAX_FIRMWARE_SIZE) {
            return {
                valido: false,
                erro: `Arquivo muito grande (${(stats.size / 1024 / 1024).toFixed(2)}MB). Máximo: 4MB`
            };
        }

        if (stats.size < MIN_FIRMWARE_SIZE) {
            return {
                valido: false,
                erro: `Arquivo muito pequeno (${(stats.size / 1024).toFixed(2)}KB). Mínimo: 100KB`
            };
        }

        const buffer = fs.readFileSync(filePath);
        
        if (buffer[0] !== ESP32_MAGIC_NUMBER) {
            return {
                valido: false,
                erro: `Magic number inválido. Esperado: 0xE9, Recebido: 0x${buffer[0].toString(16)}`
            };
        }

        const hash = crypto.createHash('md5');
        hash.update(buffer);
        const checksum = hash.digest('hex');

        const fileName = path.basename(filePath);
        const versionMatch = fileName.match(/v?(\d+\.\d+\.\d+)/i);
        const versao = versionMatch ? versionMatch[1] : null;

        return {
            valido: true,
            tamanho: stats.size,
            tamanhoMB: (stats.size / 1024 / 1024).toFixed(2),
            checksum: checksum,
            versao: versao,
            nomeArquivo: fileName
        };

    } catch (error) {
        return {
            valido: false,
            erro: `Erro ao validar firmware: ${error.message}`
        };
    }
}

function calcularChecksum(filePath) {
    try {
        const buffer = fs.readFileSync(filePath);
        const hash = crypto.createHash('md5');
        hash.update(buffer);
        return hash.digest('hex');
    } catch (error) {
        throw new Error(`Erro ao calcular checksum: ${error.message}`);
    }
}

function extrairVersao(fileName) {
    const versionMatch = fileName.match(/v?(\d+\.\d+\.\d+)/i);
    return versionMatch ? versionMatch[1] : null;
}

module.exports = {
    validarFirmware,
    calcularChecksum,
    extrairVersao,
    ESP32_MAGIC_NUMBER,
    MAX_FIRMWARE_SIZE,
    MIN_FIRMWARE_SIZE
};
