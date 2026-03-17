const fs = require('fs-extra');
const path = require('path');

console.log('=== DIAGNÓSTICO DE UPLOAD DE FIRMWARE ===\n');

const serverDir = path.join(__dirname, 'server');
const uploadDir = path.join(serverDir, 'uploads', 'firmware');

console.log('1. Verificando diretórios:');
console.log(`   Server dir: ${serverDir}`);
console.log(`   Existe: ${fs.existsSync(serverDir) ? '✅' : '❌'}`);

console.log(`\n   Upload dir: ${uploadDir}`);
console.log(`   Existe: ${fs.existsSync(uploadDir) ? '✅' : '❌'}`);

if (!fs.existsSync(uploadDir)) {
    console.log('\n   Tentando criar diretório...');
    try {
        fs.ensureDirSync(uploadDir);
        console.log('   ✅ Diretório criado com sucesso!');
    } catch (error) {
        console.log(`   ❌ Erro ao criar diretório: ${error.message}`);
    }
}

console.log('\n2. Verificando permissões de escrita:');
try {
    const testFile = path.join(uploadDir, 'test.txt');
    fs.writeFileSync(testFile, 'test');
    fs.unlinkSync(testFile);
    console.log('   ✅ Permissões de escrita OK');
} catch (error) {
    console.log(`   ❌ Erro de permissão: ${error.message}`);
}

console.log('\n3. Verificando dependências:');
try {
    require('multer');
    console.log('   ✅ multer instalado');
} catch (error) {
    console.log('   ❌ multer NÃO instalado');
}

try {
    require('fs-extra');
    console.log('   ✅ fs-extra instalado');
} catch (error) {
    console.log('   ❌ fs-extra NÃO instalado');
}

console.log('\n4. Verificando rotas OTA:');
const otaRoutePath = path.join(serverDir, 'routes', 'ota.js');
console.log(`   Arquivo ota.js: ${fs.existsSync(otaRoutePath) ? '✅' : '❌'}`);

console.log('\n5. Verificando utils:');
const validatorPath = path.join(serverDir, 'utils', 'firmwareValidator.js');
console.log(`   firmwareValidator.js: ${fs.existsSync(validatorPath) ? '✅' : '❌'}`);

console.log('\n6. Verificando services:');
const servicesDir = path.join(serverDir, 'services');
if (fs.existsSync(servicesDir)) {
    const services = fs.readdirSync(servicesDir);
    console.log(`   Services encontrados: ${services.join(', ')}`);
} else {
    console.log('   ❌ Diretório services não encontrado');
}

console.log('\n=== FIM DO DIAGNÓSTICO ===');
