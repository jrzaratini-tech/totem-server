// ============================================
// TOTEM INTERATIVO IoT v4.0
// Servidor Principal com Suporte a Áudio Personalizado
// Firebase + MQTT + Upload de MP3 + OTA
// ============================================

const express = require('express');
const session = require('express-session');
const bodyParser = require('body-parser');
const mqtt = require('mqtt');
const admin = require('firebase-admin');
const fs = require('fs-extra');
const path = require('path');
const multer = require('multer');
const { v4: uuidv4 } = require('uuid');
const cors = require('cors');
require('dotenv').config();

// ========== CONFIGURAÇÕES ==========
const PORT = process.env.PORT || 3000;
const SENHA_ADMIN = '159268'; // Senha fixa do dashboard administrativo
const SESSION_SECRET = process.env.SESSION_SECRET || 'totem-secret-key-v4';
const MQTT_BROKER = 'broker.hivemq.com';
const MQTT_PORT = 1883;

// ========== INICIALIZAÇÃO ==========
const app = express();

// Middlewares
app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());
app.use(cors());
app.use(express.static('public'));

// Sessões
app.use(session({
    secret: SESSION_SECRET,
    resave: false,
    saveUninitialized: true,
    cookie: { maxAge: 30 * 60 * 1000 } // 30 minutos
}));

// ========== FIREBASE ADMIN ==========
let db = null;
let bucket = null;
let firebaseInicializado = false;

try {
    let credentials;
    
    // Tenta carregar do arquivo local
    if (fs.existsSync('./firebase-credentials.json')) {
        credentials = JSON.parse(fs.readFileSync('./firebase-credentials.json', 'utf8'));
        console.log('✅ Firebase: Credenciais carregadas do arquivo');
    } 
    // Tenta carregar da variável de ambiente
    else if (process.env.FIREBASE_CREDENTIALS) {
        credentials = JSON.parse(process.env.FIREBASE_CREDENTIALS);
        console.log('✅ Firebase: Credenciais carregadas da env var');
    }
    else {
        throw new Error('Credenciais Firebase não encontradas');
    }
    
    admin.initializeApp({
        credential: admin.credential.cert(credentials),
        storageBucket: `${credentials.project_id}.appspot.com`
    });
    
    db = admin.firestore();
    bucket = admin.storage().bucket();
    firebaseInicializado = true;
    console.log('✅ Firebase conectado com sucesso!');
    
} catch (error) {
    console.error('❌ Erro ao inicializar Firebase:', error.message);
    console.warn('⚠️ Sistema continuará sem Firebase - apenas compatibilidade');
}

// ========== MQTT ==========
let mqttClient = null;

function conectarMQTT() {
    try {
        mqttClient = mqtt.connect(`mqtt://${MQTT_BROKER}:${MQTT_PORT}`);
        
        mqttClient.on('connect', () => {
            console.log('✅ MQTT conectado ao broker HiveMQ');
        });
        
        mqttClient.on('error', (err) => {
            console.error('❌ Erro MQTT:', err.message);
        });
    } catch (error) {
        console.error('❌ Falha ao conectar MQTT:', error.message);
    }
}

conectarMQTT();

// Função para publicar no MQTT
function publicarPlay(totemId) {
    if (mqttClient && mqttClient.connected) {
        const topico = `totem/${totemId}`;
        mqttClient.publish(topico, 'play');
        console.log(`📤 MQTT publicado: ${topico} = play`);
        return true;
    } else {
        console.warn(`⚠️ MQTT não disponível para publicar em ${totemId}`);
        return false;
    }
}

// ========== CONFIGURAÇÃO DE UPLOAD (MULTER) ==========
const uploadDir = path.join(__dirname, 'uploads');
fs.ensureDirSync(uploadDir);

const storage = multer.diskStorage({
    destination: function (req, file, cb) {
        cb(null, uploadDir);
    },
    filename: function (req, file, cb) {
        const totemId = req.params.id || req.body.totemId || 'temp';
        const uniqueId = uuidv4().slice(0, 8);
        const ext = path.extname(file.originalname);
        cb(null, `${totemId}_${uniqueId}${ext}`);
    }
});

const fileFilter = (req, file, cb) => {
    // Aceitar apenas MP3
    const allowedTypes = ['audio/mpeg', 'audio/mp3', 'audio/mpeg3'];
    const allowedExt = ['.mp3'];
    
    const ext = path.extname(file.originalname).toLowerCase();
    const mimetype = file.mimetype.toLowerCase();
    
    if (allowedTypes.includes(mimetype) || allowedExt.includes(ext)) {
        cb(null, true);
    } else {
        cb(new Error('Apenas arquivos MP3 são permitidos!'), false);
    }
};

const upload = multer({ 
    storage: storage,
    fileFilter: fileFilter,
    limits: { 
        fileSize: 5 * 1024 * 1024 // 5MB max
    }
});

// ========== FUNÇÕES AUXILIARES ==========

// Middleware de logging
function logger(req, res, next) {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.url}`);
    next();
}

app.use(logger);

// Verificar se uma data está expirada
function dataExpirada(dataStr) {
    if (!dataStr) return true;
    const hoje = new Date();
    hoje.setHours(0, 0, 0, 0);
    const expiracao = new Date(dataStr + 'T00:00:00');
    return expiracao < hoje;
}

// Obter data atual no formato YYYY-MM-DD
function hojeStr() {
    const hoje = new Date();
    return hoje.toISOString().split('T')[0];
}

// Formatar data para exibição
function formatarData(dataStr) {
    if (!dataStr) return 'Sem data';
    const [ano, mes, dia] = dataStr.split('-');
    return `${dia}/${mes}/${ano}`;
}

// Buscar totem no Firebase (com fallback para arquivo)
async function buscarTotem(id) {
    // Tenta Firebase primeiro
    if (firebaseInicializado && db) {
        try {
            const doc = await db.collection('totens').doc(id).get();
            if (doc.exists) {
                return { id: doc.id, ...doc.data() };
            }
        } catch (error) {
            console.error('Erro ao buscar no Firebase:', error.message);
        }
    }
    
    // Fallback: arquivo texto
    const caminho = path.join(__dirname, 'clientes', `${id}.txt`);
    if (fs.existsSync(caminho)) {
        const link = fs.readFileSync(caminho, 'utf8').trim();
        return { 
            id, 
            link, 
            dataExpiracao: '2099-12-31', // Compatibilidade: nunca expira
            _fallback: true 
        };
    }
    
    return null;
}

// Listar todos os totens
async function listarTotens() {
    const totens = [];
    
    // Do Firebase
    if (firebaseInicializado && db) {
        try {
            const snapshot = await db.collection('totens').get();
            snapshot.forEach(doc => {
                totens.push({ id: doc.id, ...doc.data() });
            });
        } catch (error) {
            console.error('Erro ao listar Firebase:', error.message);
        }
    }
    
    // Se não há totens no Firebase, carrega dos arquivos
    if (totens.length === 0) {
        const clientesDir = path.join(__dirname, 'clientes');
        if (fs.existsSync(clientesDir)) {
            const arquivos = fs.readdirSync(clientesDir).filter(f => f.endsWith('.txt'));
            arquivos.forEach(arquivo => {
                const id = arquivo.replace('.txt', '');
                const link = fs.readFileSync(path.join(clientesDir, arquivo), 'utf8').trim();
                totens.push({ 
                    id, 
                    link, 
                    dataExpiracao: '2099-12-31',
                    _fallback: true 
                });
            });
        }
    }
    
    return totens.sort((a, b) => a.id.localeCompare(b.id));
}

// Middleware de autenticação admin
function adminAuth(req, res, next) {
    if (req.session && req.session.adminAutenticado) {
        next();
    } else {
        res.redirect('/admin/login');
    }
}

// Middleware de autenticação cliente
function clienteAuth(req, res, next) {
    if (req.session && req.session.clienteTotemId) {
        const idUrl = req.params.id || req.body.totemId;
        if (idUrl && idUrl === req.session.clienteTotemId) {
            next();
        } else {
            res.status(403).send('Acesso negado: ID do totem não corresponde');
        }
    } else {
        res.redirect('/cliente/login');
    }
}

// Disponibilizar dependências para as rotas
app.locals.db = db;
app.locals.bucket = bucket;
app.locals.firebaseInicializado = firebaseInicializado;
app.locals.mqttClient = mqttClient;

// Servir arquivos de upload (se existirem)
app.use('/uploads', express.static('uploads'));

// ========== ROTAS PÚBLICAS ==========

// Rota raiz redireciona para admin
app.get('/', (req, res) => {
    res.redirect('/admin/login');
});

// Rota principal do totem (acionada pelo QR Code)
app.get('/totem/:id', async (req, res) => {
    const id = req.params.id;
    console.log(`🔍 Totem acessado: ${id}`);
    
    const totem = await buscarTotem(id);
    
    if (!totem) {
        console.log(`❌ Totem não encontrado: ${id}`);
        return res.status(404).send('Totem não encontrado');
    }
    
    // Verificar expiração
    if (dataExpirada(totem.dataExpiracao)) {
        console.log(`⛔ Totem expirado: ${id} (data: ${totem.dataExpiracao})`);
        return res.redirect('/expirado');
    }
    
    // Publicar no MQTT
    publicarPlay(id);
    
    // Redirecionar para o link
    console.log(`✅ Totem ativo: ${id} → ${totem.link}`);
    res.redirect(totem.link);
});

// Página de totem expirado
app.get('/expirado', (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'expirado.html'));
});

// ========== ROTAS DE CLIENTE (ÁUDIO PERSONALIZADO) ==========

// Página de login do cliente
app.get('/cliente/login', (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'cliente-login.html'));
});

// Processar login do cliente
app.post('/cliente/login', async (req, res) => {
    const { totemId, senha } = req.body;
    
    // Validar entrada
    if (!totemId || !senha) {
        return res.send(`
            <html><body style="font-family: Arial; text-align: center; padding: 50px;">
                <h2>❌ ID e senha são obrigatórios</h2>
                <p><a href="/cliente/login">Voltar</a></p>
            </body></html>
        `);
    }
    
    const totem = await buscarTotem(totemId);
    
    if (!totem) {
        return res.send(`
            <html><body style="font-family: Arial; text-align: center; padding: 50px;">
                <h2>❌ Totem não encontrado</h2>
                <p>ID inválido. <a href="/cliente/login">Tentar novamente</a></p>
            </body></html>
        `);
    }
    
    // Senha temporária: últimos 4 dígitos do ID + 2026
    const ultimos4 = totemId.slice(-4);
    const senhaEsperada = ultimos4 + '2026';
    
    if (senha !== senhaEsperada) {
        return res.send(`
            <html><body style="font-family: Arial; text-align: center; padding: 50px;">
                <h2>❌ Senha incorreta</h2>
                <p><a href="/cliente/login">Tentar novamente</a></p>
            </body></html>
        `);
    }
    
    // Criar sessão do cliente
    req.session.clienteTotemId = totemId;
    req.session.isCliente = true;
    
    res.redirect(`/cliente/dashboard/${totemId}`);
});

// Dashboard do cliente
app.get('/cliente/dashboard/:id', async (req, res) => {
    // Verificar se está logado
    if (!req.session.clienteTotemId || req.session.clienteTotemId !== req.params.id) {
        return res.redirect('/cliente/login');
    }
    
    const id = req.params.id;
    const totem = await buscarTotem(id);
    
    if (!totem) {
        return res.status(404).send('Totem não encontrado');
    }
    
    // Buscar informações de áudio
    let audioInfo = { nome: 'padrao.mp3', url: null, dataUpload: null };
    
    if (firebaseInicializado && db) {
        try {
            const doc = await db.collection('totens').doc(id).get();
            if (doc.exists && doc.data().audio) {
                audioInfo = doc.data().audio;
            }
        } catch (error) {
            console.error('Erro ao buscar áudio:', error);
        }
    }
    
    // Renderizar dashboard
    let html = fs.readFileSync(path.join(__dirname, 'views', 'cliente-dashboard.html'), 'utf8');
    
    // Substituir placeholders
    html = html.replace(/{{ID}}/g, id);
    html = html.replace(/{{LINK}}/g, totem.link || '');
    html = html.replace(/{{DATA_EXPIRACAO}}/g, formatarData(totem.dataExpiracao));
    html = html.replace(/{{AUDIO_NOME}}/g, audioInfo.nome || 'Nenhum áudio personalizado');
    html = html.replace(/{{AUDIO_DATA}}/g, audioInfo.dataUpload ? formatarData(audioInfo.dataUpload.split('T')[0]) : '---');
    
    res.send(html);
});

// Upload de áudio
app.post('/cliente/audio/:id', upload.single('audio'), async (req, res) => {
    // Verificar login
    if (!req.session.clienteTotemId || req.session.clienteTotemId !== req.params.id) {
        return res.status(401).json({ error: 'Não autorizado' });
    }
    
    const id = req.params.id;
    const file = req.file;
    
    if (!file) {
        return res.status(400).json({ error: 'Nenhum arquivo enviado' });
    }
    
    console.log(`📥 Upload de áudio para ${id}: ${file.filename}`);
    
    try {
        let audioUrl = null;
        let audioData = {
            nome: file.originalname,
            tamanho: file.size,
            dataUpload: new Date().toISOString(),
            filename: file.filename
        };
        
        // Se Firebase Storage está disponível, faz upload
        if (bucket) {
            try {
                const destination = `audios/${id}/${file.filename}`;
                await bucket.upload(file.path, { 
                    destination,
                    metadata: {
                        contentType: 'audio/mpeg',
                        metadata: {
                            totemId: id,
                            originalName: file.originalname,
                            uploadDate: audioData.dataUpload
                        }
                    }
                });
                
                // Gerar URL assinada (válida por 1 ano)
                const [url] = await bucket.file(destination).getSignedUrl({
                    action: 'read',
                    expires: Date.now() + 365 * 24 * 60 * 60 * 1000
                });
                
                audioUrl = url;
                audioData.url = url;
                audioData.storagePath = destination;
                
                console.log(`✅ Áudio enviado para Firebase Storage: ${destination}`);
            } catch (storageError) {
                console.error('Erro no Firebase Storage:', storageError);
                // Fallback: salvar localmente
                const localUrl = `/uploads/${file.filename}`;
                audioUrl = localUrl;
                audioData.url = localUrl;
            }
        } else {
            // Fallback: servir localmente
            const localUrl = `/uploads/${file.filename}`;
            audioUrl = localUrl;
            audioData.url = localUrl;
        }
        
        // Salvar referência no Firestore
        if (db) {
            await db.collection('totens').doc(id).set({
                audio: audioData,
                ultimaAtualizacaoAudio: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
            console.log(`✅ Referência de áudio salva no Firestore para ${id}`);
        }
        
        // Notificar ESP32 via MQTT
        if (mqttClient && mqttClient.connected) {
            const topico = `totem/${id}/audio`;
            mqttClient.publish(topico, JSON.stringify({
                comando: 'atualizar',
                timestamp: Date.now(),
                versao: audioData.dataUpload
            }));
            console.log(`📤 Notificação MQTT enviada para ${id}`);
        }
        
        // Resposta de sucesso
        return res.json({
            success: true,
            message: 'Áudio enviado com sucesso',
            audio: {
                nome: file.originalname,
                tamanho: file.size,
                url: audioUrl
            }
        });
        
    } catch (error) {
        console.error('Erro no processo de upload:', error);
        return res.status(500).json({ 
            error: 'Erro interno ao processar upload' 
        });
    } finally {
        // Limpar arquivo temporário (se ainda existir)
        if (file && file.path && fs.existsSync(file.path)) {
            try {
                fs.removeSync(file.path);
            } catch (cleanError) {
                console.warn('Erro ao limpar arquivo temporário:', cleanError);
            }
        }
    }
});

// ========== ROTAS DE API PARA ÁUDIO ==========

/**
 * GET /api/audio/:id
 * Endpoint para ESP32 verificar se há novo áudio
 */
app.get('/api/audio/:id', async (req, res) => {
    const id = req.params.id;
    
    if (!firebaseInicializado || !db) {
        return res.json({ 
            url: null, 
            versao: '1.0',
            message: 'Firebase não disponível'
        });
    }
    
    try {
        const doc = await db.collection('totens').doc(id).get();
        
        if (doc.exists) {
            const data = doc.data();
            const audio = data.audio || {};
            
            // Se tem URL salva, retorna
            if (audio.url) {
                return res.json({
                    url: audio.url,
                    nome: audio.nome || 'audio.mp3',
                    versao: audio.dataUpload || data.ultimaRenovacao || '1.0',
                    tamanho: audio.tamanho || 0
                });
            }
            
            // Se não tem áudio personalizado, retorna áudio padrão
            return res.json({
                url: null,
                versao: '1.0',
                message: 'Usar áudio padrão do totem'
            });
        } else {
            return res.status(404).json({ 
                error: 'Totem não encontrado' 
            });
        }
    } catch (error) {
        console.error('Erro ao buscar áudio:', error);
        return res.status(500).json({ 
            error: 'Erro interno no servidor' 
        });
    }
});

/**
 * DELETE /api/audio/:id
 * Remove áudio personalizado
 */
app.delete('/api/audio/:id', async (req, res) => {
    // Verificar autenticação
    if (!req.session || 
        (!req.session.adminAutenticado && req.session.clienteTotemId !== req.params.id)) {
        return res.status(401).json({ error: 'Não autorizado' });
    }
    
    const id = req.params.id;
    
    try {
        if (db) {
            const doc = await db.collection('totens').doc(id).get();
            
            if (doc.exists && doc.data().audio) {
                const audio = doc.data().audio;
                
                // Remover do Storage se existir
                if (bucket && audio.storagePath) {
                    try {
                        await bucket.file(audio.storagePath).delete();
                        console.log(`🗑️ Áudio removido do Storage: ${audio.storagePath}`);
                    } catch (storageError) {
                        console.warn('Erro ao remover do Storage:', storageError);
                    }
                }
                
                // Remover referência do Firestore
                await db.collection('totens').doc(id).update({
                    audio: admin.firestore.FieldValue.delete()
                });
                
                console.log(`🗑️ Áudio removido do Firestore para ${id}`);
            }
        }
        
        return res.json({
            success: true,
            message: 'Áudio removido, totem voltará a usar áudio padrão'
        });
        
    } catch (error) {
        console.error('Erro ao remover áudio:', error);
        return res.status(500).json({ error: 'Erro interno ao remover áudio' });
    }
});

/**
 * GET /api/audio/:id/info
 * Informações detalhadas do áudio
 */
app.get('/api/audio/:id/info', async (req, res) => {
    const id = req.params.id;
    
    if (!db) {
        return res.json({ 
            personalizado: false,
            message: 'Firebase não disponível'
        });
    }
    
    try {
        const doc = await db.collection('totens').doc(id).get();
        
        if (!doc.exists) {
            return res.status(404).json({ error: 'Totem não encontrado' });
        }
        
        const data = doc.data();
        const audio = data.audio || {};
        
        const info = {
            totemId: id,
            personalizado: !!audio.url,
            audioAtual: audio.nome || 'áudio padrão',
            dataUpload: audio.dataUpload || null,
            tamanho: audio.tamanho || 0,
            ultimaAtualizacao: data.ultimaAtualizacaoAudio || data.ultimaRenovacao || null
        };
        
        return res.json(info);
        
    } catch (error) {
        console.error('Erro ao buscar info do áudio:', error);
        return res.status(500).json({ error: 'Erro interno' });
    }
});

// ========== ROTAS ADMIN (DASHBOARD) ==========

// Página de login admin
app.get('/admin/login', (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'login.html'));
});

// Processar login admin
app.post('/admin/login', (req, res) => {
    const { senha } = req.body;
    
    if (senha === SENHA_ADMIN) {
        req.session.adminAutenticado = true;
        res.redirect('/admin/dashboard');
    } else {
        res.sendFile(path.join(__dirname, 'views', 'mensagem.html'));
    }
});

// Logout admin
app.get('/admin/logout', (req, res) => {
    req.session.destroy();
    res.redirect('/admin/login');
});

// Dashboard admin
app.get('/admin/dashboard', adminAuth, async (req, res) => {
    const totens = await listarTotens();
    
    let linhasTabela = '';
    
    for (const totem of totens) {
        const expirada = dataExpirada(totem.dataExpiracao);
        const statusIcon = expirada ? '❌' : '✅';
        const statusText = expirada ? 'Expirado' : 'Ativo';
        
        // Verificar se tem áudio personalizado
        const temAudio = totem.audio ? '🎵' : '';
        
        linhasTabela += `
            <tr>
                <td>${totem.id} ${temAudio}</td>
                <td><a href="${totem.link}" target="_blank">${totem.link}</a></td>
                <td>${formatarData(totem.dataExpiracao)}</td>
                <td>${statusIcon} ${statusText}</td>
                <td>
                    <a href="/admin/editar/${totem.id}">✏️ Editar</a> | 
                    <a href="/admin/excluir/${totem.id}" onclick="return confirm('Excluir totem?')">🗑️ Excluir</a>
                </td>
            </tr>
        `;
    }
    
    let html = fs.readFileSync(path.join(__dirname, 'views', 'admin.html'), 'utf8');
    html = html.replace('{{LINHAS_TABELA}}', linhasTabela);
    
    res.send(html);
});

// Página de novo totem
app.get('/admin/novo', adminAuth, (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'novo.html'));
});

// Criar novo totem
app.post('/admin/novo', adminAuth, async (req, res) => {
    const { id, link, dataExpiracao } = req.body;
    
    // Validações básicas
    if (!id || !link || !dataExpiracao) {
        return res.status(400).send('Todos os campos são obrigatórios');
    }
    
    if (!link.includes('instagram.com')) {
        return res.status(400).send('Link deve ser do Instagram');
    }
    
    if (dataExpirada(dataExpiracao)) {
        return res.status(400).send('Data de expiração deve ser futura');
    }
    
    // Dados do totem
    const totemData = {
        link,
        dataExpiracao,
        status: 'ativo',
        criadoEm: admin.firestore.FieldValue.serverTimestamp(),
        ultimaRenovacao: admin.firestore.FieldValue.serverTimestamp()
    };
    
    // Salvar no Firebase
    if (firebaseInicializado && db) {
        try {
            await db.collection('totens').doc(id).set(totemData);
            console.log(`✅ Totem ${id} criado no Firebase`);
        } catch (error) {
            console.error('Erro ao criar no Firebase:', error);
        }
    }
    
    // Compatibilidade: salvar arquivo
    const clientesDir = path.join(__dirname, 'clientes');
    fs.ensureDirSync(clientesDir);
    fs.writeFileSync(path.join(clientesDir, `${id}.txt`), link);
    
    res.redirect('/admin/dashboard');
});

// Página de editar totem
app.get('/admin/editar/:id', adminAuth, async (req, res) => {
    const id = req.params.id;
    const totem = await buscarTotem(id);
    
    if (!totem) {
        return res.status(404).send('Totem não encontrado');
    }
    
    let html = fs.readFileSync(path.join(__dirname, 'views', 'editar.html'), 'utf8');
    html = html.replace(/{{ID}}/g, totem.id);
    html = html.replace(/{{LINK}}/g, totem.link || '');
    html = html.replace(/{{DATA_EXPIRACAO}}/g, totem.dataExpiracao || '');
    
    res.send(html);
});

// Atualizar totem
app.post('/admin/editar/:id', adminAuth, async (req, res) => {
    const id = req.params.id;
    const { link, dataExpiracao } = req.body;
    
    if (!link || !dataExpiracao) {
        return res.status(400).send('Todos os campos são obrigatórios');
    }
    
    // Atualizar no Firebase
    if (firebaseInicializado && db) {
        try {
            await db.collection('totens').doc(id).set({
                link,
                dataExpiracao,
                ultimaRenovacao: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
            console.log(`✅ Totem ${id} atualizado no Firebase`);
        } catch (error) {
            console.error('Erro ao atualizar no Firebase:', error);
        }
    }
    
    // Atualizar arquivo
    fs.writeFileSync(path.join(__dirname, 'clientes', `${id}.txt`), link);
    
    res.redirect('/admin/dashboard');
});

// Excluir totem
app.get('/admin/excluir/:id', adminAuth, async (req, res) => {
    const id = req.params.id;
    
    // Excluir do Firebase
    if (firebaseInicializado && db) {
        try {
            await db.collection('totens').doc(id).delete();
            console.log(`✅ Totem ${id} excluído do Firebase`);
        } catch (error) {
            console.error('Erro ao excluir do Firebase:', error);
        }
    }
    
    // Excluir arquivo
    const caminho = path.join(__dirname, 'clientes', `${id}.txt`);
    if (fs.existsSync(caminho)) {
        fs.removeSync(caminho);
    }
    
    res.redirect('/admin/dashboard');
});

// ========== INICIAR SERVIDOR ==========
app.listen(PORT, () => {
    console.log('\n' + '='.repeat(50));
    console.log('🚀 TOTEM SERVER v4.0 rodando!');
    console.log('='.repeat(50));
    console.log(`📡 Porta: ${PORT}`);
    console.log(`📊 Admin: http://localhost:${PORT}/admin/login`);
    console.log(`👤 Cliente: http://localhost:${PORT}/cliente/login`);
    console.log(`📡 MQTT: ${MQTT_BROKER}:${MQTT_PORT}`);
    console.log(`🔥 Firebase: ${firebaseInicializado ? '✅ Conectado' : '❌ Não disponível'}`);
    console.log('='.repeat(50) + '\n');
});