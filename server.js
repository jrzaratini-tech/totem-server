// ============================================
// TOTEM INTERATIVO IoT v4.0 - CORRIGIDO
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
const SENHA_ADMIN = '159268';
const SESSION_SECRET = process.env.SESSION_SECRET || 'totem-secret-key-v4';
const MQTT_BROKER = 'broker.hivemq.com';
const MQTT_PORT = 1883;
const SERVER_URL = process.env.SERVER_URL || 'https://totem-server.onrender.com';

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
    cookie: { maxAge: 30 * 60 * 1000 }
}));

// ========== FIREBASE ADMIN ==========
let db = null;
let bucket = null;
let firebaseInicializado = false;

try {
    let credentials;
    
    // CORREÇÃO: Verifica ambas as extensões possíveis
    if (fs.existsSync('./firebase-credentials.json')) {
        credentials = JSON.parse(fs.readFileSync('./firebase-credentials.json', 'utf8'));
        console.log('✅ Firebase: Credenciais carregadas do arquivo (firebase-credentials.json)');
    } 
    else if (fs.existsSync('./firebase-credentials.json.json')) {
        credentials = JSON.parse(fs.readFileSync('./firebase-credentials.json.json', 'utf8'));
        console.log('✅ Firebase: Credenciais carregadas do arquivo (firebase-credentials.json.json)');
    }
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

// Função para notificar ESP32 sobre novo áudio
function notificarAtualizacaoAudio(totemId, versao) {
    if (mqttClient && mqttClient.connected) {
        const topicoAudio = `totem/${totemId}/audio`;
        const mensagem = JSON.stringify({
            comando: 'atualizar',
            timestamp: Date.now(),
            versao: versao
        });
        mqttClient.publish(topicoAudio, mensagem);
        
        const topicoPlay = `totem/${totemId}`;
        mqttClient.publish(topicoPlay, 'atualizar_audio');
        
        console.log(`📤 Notificações MQTT enviadas para ${totemId}`);
        return true;
    }
    return false;
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
        const ext = path.extname(file.originalname).toLowerCase();
        // Garante extensão .mp3
        const finalExt = ext === '.mp3' ? '.mp3' : '.mp3';
        cb(null, `${totemId}_${uniqueId}${finalExt}`);
    }
});

const fileFilter = (req, file, cb) => {
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
        fileSize: 10 * 1024 * 1024 // 10MB
    }
});

// ========== FUNÇÕES AUXILIARES ==========

function logger(req, res, next) {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.url}`);
    next();
}

app.use(logger);

function dataExpirada(dataStr) {
    if (!dataStr) return true;
    const hoje = new Date();
    hoje.setHours(0, 0, 0, 0);
    const expiracao = new Date(dataStr + 'T00:00:00');
    return expiracao < hoje;
}

function formatarData(dataStr) {
    if (!dataStr) return 'Sem data';
    const [ano, mes, dia] = dataStr.split('-');
    return `${dia}/${mes}/${ano}`;
}

async function buscarTotem(id) {
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
    
    const caminho = path.join(__dirname, 'clientes', `${id}.txt`);
    if (fs.existsSync(caminho)) {
        const link = fs.readFileSync(caminho, 'utf8').trim();
        return { 
            id, 
            link, 
            dataExpiracao: '2099-12-31',
            _fallback: true 
        };
    }
    
    return null;
}

async function listarTotens() {
    const totens = [];
    
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

function adminAuth(req, res, next) {
    if (req.session && req.session.adminAutenticado) {
        next();
    } else {
        res.redirect('/admin/login');
    }
}

app.locals.db = db;
app.locals.bucket = bucket;
app.locals.firebaseInicializado = firebaseInicializado;
app.locals.mqttClient = mqttClient;

app.use('/uploads', express.static('uploads'));

// ========== ROTAS PÚBLICAS ==========

app.get('/', (req, res) => {
    res.redirect('/admin/login');
});

app.get('/totem/:id', async (req, res) => {
    const id = req.params.id;
    console.log(`🔍 Totem acessado: ${id}`);
    
    const totem = await buscarTotem(id);
    
    if (!totem) {
        console.log(`❌ Totem não encontrado: ${id}`);
        return res.status(404).send('Totem não encontrado');
    }
    
    if (dataExpirada(totem.dataExpiracao)) {
        console.log(`⛔ Totem expirado: ${id} (data: ${totem.dataExpiracao})`);
        return res.redirect('/expirado');
    }
    
    publicarPlay(id);
    
    console.log(`✅ Totem ativo: ${id} → ${totem.link}`);
    res.redirect(totem.link);
});

app.get('/expirado', (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'expirado.html'));
});

// ========== ROTAS DE CLIENTE ==========

app.get('/cliente/login', (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'cliente-login.html'));
});

app.post('/cliente/login', async (req, res) => {
    const { totemId, senha } = req.body;
    
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
                <p><a href="/cliente/login">Tentar novamente</a></p>
            </body></html>
        `);
    }
    
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
    
    req.session.clienteTotemId = totemId;
    req.session.isCliente = true;
    
    res.redirect(`/cliente/dashboard/${totemId}`);
});

app.get('/cliente/dashboard/:id', async (req, res) => {
    if (!req.session.clienteTotemId || req.session.clienteTotemId !== req.params.id) {
        return res.redirect('/cliente/login');
    }
    
    const id = req.params.id;
    const totem = await buscarTotem(id);
    
    if (!totem) {
        return res.status(404).send('Totem não encontrado');
    }
    
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
    
    let html = fs.readFileSync(path.join(__dirname, 'views', 'cliente-dashboard.html'), 'utf8');
    
    html = html.replace(/{{ID}}/g, id);
    html = html.replace(/{{LINK}}/g, totem.link || '');
    html = html.replace(/{{DATA_EXPIRACAO}}/g, formatarData(totem.dataExpiracao));
    html = html.replace(/{{AUDIO_NOME}}/g, audioInfo.nome || 'Nenhum áudio personalizado');
    html = html.replace(/{{AUDIO_DATA}}/g, audioInfo.dataUpload ? formatarData(audioInfo.dataUpload.split('T')[0]) : '---');
    
    res.send(html);
});

app.get('/cliente/logout', (req, res) => {
    req.session.destroy(() => {
        res.redirect('/cliente/login');
    });
});

// ========== ROTA DE UPLOAD CORRIGIDA ==========
app.post('/cliente/audio/:id', upload.single('audio'), async (req, res) => {
    if (!req.session.clienteTotemId || req.session.clienteTotemId !== req.params.id) {
        return res.status(401).json({ error: 'Não autorizado' });
    }
    
    const id = req.params.id;
    const file = req.file;
    
    if (!file) {
        return res.status(400).json({ error: 'Nenhum arquivo enviado' });
    }
    
    console.log(`📥 Upload de áudio para ${id}: ${file.filename}`);
    console.log(`📁 Arquivo salvo em: ${file.path}`);
    console.log(`📏 Tamanho: ${file.size} bytes`);
    console.log(`🏷️ Tipo: ${file.mimetype}`);
    
    try {
        let audioUrl = null;
        let versao = new Date().toISOString();
        let audioData = {
            nome: file.originalname,
            tamanho: file.size,
            dataUpload: versao,
            filename: file.filename,
            mimetype: file.mimetype
        };
        
        if (bucket) {
            try {
                const destination = `audios/${id}/${file.filename}`;
                console.log(`📤 Enviando para Firebase Storage: ${destination}`);
                
                await bucket.upload(file.path, { 
                    destination,
                    metadata: {
                        contentType: file.mimetype || 'audio/mpeg',
                        metadata: {
                            totemId: id,
                            originalName: file.originalname,
                            uploadDate: audioData.dataUpload
                        }
                    }
                });
                
                const fileRef = bucket.file(destination);
                await fileRef.makePublic();
                
                audioUrl = `https://storage.googleapis.com/${bucket.name}/${destination}`;
                audioData.url = audioUrl;
                audioData.storagePath = destination;
                
                console.log(`✅ Áudio público em: ${audioUrl}`);
                
                try {
                    fs.removeSync(file.path);
                    console.log('🗑️ Arquivo temporário removido');
                } catch (e) {
                    console.warn('Erro ao remover temporário:', e);
                }
                
            } catch (storageError) {
                console.error('❌ Erro no Firebase Storage:', storageError);
                console.log('⚠️ Usando fallback local');
                audioUrl = `${SERVER_URL}/uploads/${file.filename}`;
                audioData.url = audioUrl;
            }
        } else {
            console.log('⚠️ Firebase Storage não disponível, servindo localmente');
            audioUrl = `${SERVER_URL}/uploads/${file.filename}`;
            audioData.url = audioUrl;
        }
        
        if (db) {
            await db.collection('totens').doc(id).set({
                audio: audioData,
                ultimaAtualizacaoAudio: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
            console.log(`✅ Referência salva no Firestore para ${id}`);
        }
        
        // NOTIFICA O ESP32 IMEDIATAMENTE
        notificarAtualizacaoAudio(id, versao);
        
        return res.json({
            success: true,
            message: 'Áudio enviado com sucesso',
            audio: {
                nome: file.originalname,
                tamanho: file.size,
                url: audioUrl,
                versao: versao
            }
        });
        
    } catch (error) {
        console.error('❌ Erro no upload:', error);
        return res.status(500).json({ 
            error: 'Erro interno: ' + error.message 
        });
    }
});

// ========== API PARA ESP32 ==========
app.get('/api/audio/:id', async (req, res) => {
    const id = req.params.id;
    
    console.log(`🔍 ESP32 consultando áudio para ${id}`);
    
    if (!firebaseInicializado || !db) {
        console.log('⚠️ Firebase não disponível');
        return res.json({ 
            url: null, 
            versao: '1.0'
        });
    }
    
    try {
        const doc = await db.collection('totens').doc(id).get();
        
        if (doc.exists) {
            const data = doc.data();
            const audio = data.audio || {};
            
            if (audio.url) {
                let urlFinal = audio.url;
                
                if (urlFinal.startsWith('/uploads/')) {
                    urlFinal = `${SERVER_URL}${urlFinal}`;
                }
                
                console.log(`✅ Retornando URL: ${urlFinal}`);
                
                return res.json({
                    url: urlFinal,
                    nome: audio.nome || 'audio.mp3',
                    versao: audio.dataUpload || data.ultimaRenovacao || '1.0',
                    tamanho: audio.tamanho || 0
                });
            }
            
            console.log(`ℹ️ ${id} não tem áudio personalizado`);
            return res.json({
                url: null,
                versao: '1.0'
            });
        } else {
            console.log(`❌ Totem ${id} não encontrado`);
            return res.status(404).json({ 
                error: 'Totem não encontrado' 
            });
        }
    } catch (error) {
        console.error('❌ Erro na API:', error);
        return res.status(500).json({ 
            error: 'Erro interno' 
        });
    }
});

app.delete('/api/audio/:id', async (req, res) => {
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
                
                if (bucket && audio.storagePath) {
                    try {
                        await bucket.file(audio.storagePath).delete();
                        console.log(`🗑️ Áudio removido do Storage`);
                    } catch (storageError) {
                        console.warn('Erro ao remover do Storage:', storageError);
                    }
                }
                
                await db.collection('totens').doc(id).update({
                    audio: admin.firestore.FieldValue.delete()
                });
                
                console.log(`🗑️ Áudio removido do Firestore`);
                
                // Notifica ESP32 que o áudio foi removido
                notificarAtualizacaoAudio(id, 'removido');
            }
        }
        
        return res.json({
            success: true,
            message: 'Áudio removido'
        });
        
    } catch (error) {
        console.error('Erro ao remover áudio:', error);
        return res.status(500).json({ error: 'Erro interno' });
    }
});

// ========== ROTAS ADMIN ==========

app.get('/admin/login', (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'login.html'));
});

app.post('/admin/login', (req, res) => {
    const { senha } = req.body;
    
    if (senha === SENHA_ADMIN) {
        req.session.adminAutenticado = true;
        res.redirect('/admin/dashboard');
    } else {
        res.sendFile(path.join(__dirname, 'views', 'mensagem.html'));
    }
});

app.get('/admin/logout', (req, res) => {
    req.session.destroy();
    res.redirect('/admin/login');
});

app.get('/admin/dashboard', adminAuth, async (req, res) => {
    const totens = await listarTotens();
    
    let linhasTabela = '';
    
    for (const totem of totens) {
        const expirada = dataExpirada(totem.dataExpiracao);
        const statusIcon = expirada ? '❌' : '✅';
        const statusText = expirada ? 'Expirado' : 'Ativo';
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

app.get('/admin/novo', adminAuth, (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'novo.html'));
});

app.post('/admin/novo', adminAuth, async (req, res) => {
    const { id, link, dataExpiracao } = req.body;
    
    if (!id || !link || !dataExpiracao) {
        return res.status(400).send('Todos os campos são obrigatórios');
    }
    
    if (!link.includes('instagram.com')) {
        return res.status(400).send('Link deve ser do Instagram');
    }
    
    if (dataExpirada(dataExpiracao)) {
        return res.status(400).send('Data de expiração deve ser futura');
    }
    
    const totemData = {
        link,
        dataExpiracao,
        status: 'ativo',
        criadoEm: admin.firestore.FieldValue.serverTimestamp(),
        ultimaRenovacao: admin.firestore.FieldValue.serverTimestamp()
    };
    
    if (firebaseInicializado && db) {
        try {
            await db.collection('totens').doc(id).set(totemData);
            console.log(`✅ Totem ${id} criado no Firebase`);
        } catch (error) {
            console.error('Erro ao criar no Firebase:', error);
        }
    }
    
    const clientesDir = path.join(__dirname, 'clientes');
    fs.ensureDirSync(clientesDir);
    fs.writeFileSync(path.join(clientesDir, `${id}.txt`), link);
    
    res.redirect('/admin/dashboard');
});

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

app.post('/admin/editar/:id', adminAuth, async (req, res) => {
    const id = req.params.id;
    const { link, dataExpiracao } = req.body;
    
    if (!link || !dataExpiracao) {
        return res.status(400).send('Todos os campos são obrigatórios');
    }
    
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
    
    fs.writeFileSync(path.join(__dirname, 'clientes', `${id}.txt`), link);
    
    res.redirect('/admin/dashboard');
});

app.get('/admin/excluir/:id', adminAuth, async (req, res) => {
    const id = req.params.id;
    
    if (firebaseInicializado && db) {
        try {
            await db.collection('totens').doc(id).delete();
            console.log(`✅ Totem ${id} excluído do Firebase`);
        } catch (error) {
            console.error('Erro ao excluir do Firebase:', error);
        }
    }
    
    const caminho = path.join(__dirname, 'clientes', `${id}.txt`);
    if (fs.existsSync(caminho)) {
        fs.removeSync(caminho);
    }
    
    res.redirect('/admin/dashboard');
});

// ========== INICIAR SERVIDOR ==========
app.listen(PORT, () => {
    console.log('\n' + '='.repeat(50));
    console.log('🚀 TOTEM SERVER v4.0 CORRIGIDO rodando!');
    console.log('='.repeat(50));
    console.log(`📡 Porta: ${PORT}`);
    console.log(`🌐 URL: ${SERVER_URL}`);
    console.log(`📊 Admin: ${SERVER_URL}/admin/login`);
    console.log(`👤 Cliente: ${SERVER_URL}/cliente/login`);
    console.log(`📡 MQTT: ${MQTT_BROKER}:${MQTT_PORT}`);
    console.log(`🔥 Firebase: ${firebaseInicializado ? '✅ Conectado' : '❌ Não disponível'}`);
    console.log('='.repeat(50) + '\n');
});