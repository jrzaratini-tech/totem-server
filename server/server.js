// ============================================
// TOTEM INTERATIVO IoT v4.2.1 - SISTEMA ROBUSTO
// Servidor com Validação de Áudio em Múltiplas Camadas
// Firebase + MQTT + Upload de MP3 + OTA + FFprobe
// ============================================

const express = require('express');
const session = require('express-session');
const bodyParser = require('body-parser');
const mqtt = require('mqtt');
const admin = require('firebase-admin');
const fs = require('fs-extra');
const path = require('path');
const multer = require('multer');
const ffmpeg = require('fluent-ffmpeg');
const ffmpegPath = require('ffmpeg-static');

if (!ffmpegPath) {
    throw new Error('FFmpeg não encontrado. Instale ffmpeg-static corretamente.');
}

ffmpeg.setFfmpegPath(ffmpegPath);
console.log('✅ FFmpeg configurado:', ffmpegPath);
const { v4: uuidv4 } = require('uuid');
const cors = require('cors');
const { adminAuth, logger, verificarAcessoCliente } = require('./middlewares/auth');
const projectRoot = path.resolve(__dirname, '..');
require('dotenv').config({ path: path.join(projectRoot, '.env') });

// ========== CONFIGURAÇÕES ==========
const PORT = process.env.PORT || 3000;
const SESSION_SECRET = process.env.SESSION_SECRET || 'totem-secret-key-default-' + Math.random().toString(36);
const MQTT_BROKER = 'broker.hivemq.com';
const MQTT_PORT = 1883;
const SERVER_URL = process.env.SERVER_URL || 'https://totem-server.onrender.com';

// ========== INICIALIZAÇÃO ==========
const app = express();

// Middlewares
app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());
app.use(cors());
app.use(express.static(path.join(projectRoot, 'public')));
app.use(express.static(path.join(__dirname, 'public')));

// Sessões
app.use(session({
    secret: SESSION_SECRET,
    resave: false,
    saveUninitialized: true,
    cookie: { maxAge: 30 * 60 * 1000 }
}));

app.use(logger);

// ========== FIREBASE ADMIN ==========
let db = null;
let bucket = null;
let storageDisponivel = null;
let firebaseInicializado = false;

try {
    let credentials;
    
    // CORREÇÃO: Verifica ambas as extensões possíveis
    const firebaseCredsPath = path.join(projectRoot, 'firebase-credentials.json');
    const firebaseCredsPathAlt = path.join(projectRoot, 'firebase-credentials.json.json');

    if (fs.existsSync(firebaseCredsPath)) {
        credentials = JSON.parse(fs.readFileSync(firebaseCredsPath, 'utf8'));
        console.log('✅ Firebase: Credenciais carregadas do arquivo (firebase-credentials.json)');
    } 
    else if (fs.existsSync(firebaseCredsPathAlt)) {
        credentials = JSON.parse(fs.readFileSync(firebaseCredsPathAlt, 'utf8'));
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

    bucket.exists()
        .then(([exists]) => {
            storageDisponivel = !!exists;
            if (storageDisponivel) {
                console.log(`✅ Firebase Storage disponível (bucket: ${bucket.name})`);
            } else {
                console.error(`❌ Firebase Storage indisponível: bucket não existe (${bucket.name})`);
            }
        })
        .catch((err) => {
            storageDisponivel = false;
            console.error(`❌ Falha ao verificar bucket do Firebase Storage (${bucket.name}):`, err && err.message ? err.message : err);
        });
     
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
            
            // Verificar se deve publicar atualização de firmware
            setTimeout(() => {
                try {
                    const configPath = path.join(__dirname, 'firmware-config.json');
                    if (fs.existsSync(configPath)) {
                        const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
                        
                        if (config.autoPublish === true) {
                            console.log(`🔄 Auto-publish habilitado para firmware ${config.firmwareVersion}`);
                            publicarAtualizacaoFirmware(config.totemId, config.firmwareUrl);
                            
                            // Desabilitar auto-publish após publicar (evita loops)
                            config.autoPublish = false;
                            fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
                            console.log('✅ Auto-publish desabilitado automaticamente');
                        } else {
                            console.log('ℹ️ Auto-publish de firmware desabilitado (firmware-config.json)');
                        }
                    } else {
                        console.log('ℹ️ Arquivo firmware-config.json não encontrado - sem auto-publish');
                    }
                } catch (error) {
                    console.error('❌ Erro ao verificar firmware-config.json:', error.message);
                }
            }, 2000); // Aguarda 2s para garantir que a conexão está estável
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
        const topico = `totem/${totemId}/trigger`;
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
        const topicoAudio = `totem/${totemId}/audioUpdate`;
        const mensagem = JSON.stringify({
            comando: 'audioUpdate',
            timestamp: Date.now(),
            versao: versao
        });
        mqttClient.publish(topicoAudio, mensagem, { retain: true });
        
        console.log(`📤 MQTT publicado: ${topicoAudio} = ${mensagem}`);
        return true;
    }
    return false;
}

// Função para notificar ESP32 sobre nova configuração (efeito/LED)
function notificarAtualizacaoConfig(totemId, payload) {
    if (mqttClient && mqttClient.connected) {
        const topico = `totem/${totemId}/configUpdate`;
        const mensagem = JSON.stringify(payload);

        // Retained para o ESP32 receber ao reconectar
        mqttClient.publish(topico, mensagem, { retain: true });
        console.log(`📤 Config MQTT publicada: ${topico} = ${mensagem}`);
        return true;
    }
    console.warn(`⚠️ MQTT não disponível para publicar config em ${totemId}`);
    return false;
}

// Função para publicar atualização de firmware automaticamente
function publicarAtualizacaoFirmware(totemId, firmwareUrl) {
    if (mqttClient && mqttClient.connected) {
        const topico = `totem/${totemId}/firmwareUpdate`;
        mqttClient.publish(topico, firmwareUrl, { retain: true });
        console.log(`📤 Firmware Update MQTT publicado: ${topico} = ${firmwareUrl}`);
        return true;
    }
    console.warn(`⚠️ MQTT não disponível para publicar firmware update em ${totemId}`);
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
        const finalExt = ext || '.bin';
        cb(null, `${totemId}_${uniqueId}${finalExt}`);
    }
});

const fileFilter = (req, file, cb) => {
    const allowedTypes = [
        'audio/mpeg',
        'audio/mp3',
        'audio/mpeg3',
        'audio/webm',
        'audio/ogg',
        'audio/wav',
        'audio/x-wav',
        'audio/mp4',
        'audio/x-m4a'
    ];
    
    // Com streaming HTTP, podemos aceitar diversos formatos de áudio
    if (allowedTypes.includes(file.mimetype) || file.mimetype.startsWith('audio/')) {
        cb(null, true);
    } else {
        cb(new Error('Formato não suportado. Use arquivos de áudio (MP3, WAV, OGG, WebM).'));
    }
};

const upload = multer({ 
    storage: storage,
    fileFilter: fileFilter,
    limits: { 
        fileSize: 10 * 1024 * 1024 // 10MB - streaming HTTP permite arquivos maiores
    }
});

// ========== FUNÇÕES AUXILIARES ==========

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

async function buscarSenhaAdmin() {
    console.log('🔍 Buscando senha admin...');
    console.log('Firebase inicializado:', firebaseInicializado);
    console.log('DB disponível:', !!db);
    
    // Prioridade 1: Senha do Firebase
    if (firebaseInicializado && db) {
        try {
            // Tenta primeiro o novo caminho: adminConfig/auth
            let doc = await db.collection('adminConfig').doc('auth').get();
            console.log('Documento adminConfig/auth existe:', doc.exists);
            
            // Se não encontrar, tenta o caminho antigo: config/admin
            if (!doc.exists) {
                doc = await db.collection('config').doc('admin').get();
                console.log('Documento config/admin existe:', doc.exists);
            }
            
            if (doc.exists) {
                const data = doc.data();
                console.log('Dados do documento admin encontrados:', JSON.stringify(data));
                
                // Aceita tanto 'senha' quanto 'password' como campo
                const senhaEncontrada = data && (data.senha || data.password);
                
                if (senhaEncontrada) {
                    console.log('✅ Senha encontrada no Firebase:', senhaEncontrada);
                    return senhaEncontrada;
                } else {
                    console.log('⚠️ Documento existe mas não tem campo "senha" ou "password"');
                    console.log('Campos disponíveis:', data ? Object.keys(data) : 'nenhum');
                }
            } else {
                console.log('⚠️ Documento NÃO EXISTE no Firestore');
                console.log('💡 Crie o documento: Firestore > adminConfig > auth > password: "sua_senha_segura"');
            }
        } catch (error) {
            console.error('❌ Erro ao buscar senha admin do Firebase:', error.message);
        }
    } else {
        console.log('⚠️ Firebase não está inicializado');
    }
    
    // Prioridade 2: Variável de ambiente
    if (process.env.ADMIN_PASSWORD) {
        console.log('✅ Usando senha da variável de ambiente ADMIN_PASSWORD');
        return process.env.ADMIN_PASSWORD;
    }
    
    // Sem senha configurada - retorna null para bloquear acesso
    console.error('❌ NENHUMA SENHA CONFIGURADA!');
    console.error('💡 Configure a senha no Firebase (config/admin/senha) ou na variável de ambiente ADMIN_PASSWORD');
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

app.locals.db = db;
app.locals.bucket = bucket;
app.locals.firebaseInicializado = firebaseInicializado;
app.locals.mqttClient = mqttClient;

app.use('/uploads', express.static(uploadDir));

// ========== ROTAS PÚBLICAS ==========

app.get('/', (req, res) => {
    res.redirect('/admin/login');
});

app.get('/totem/:id', async (req, res) => {
    const id = req.params.id;
    console.log(`========================================`);
    console.log(`🔍 Totem acessado: ${id}`);
    console.log(`📱 User-Agent: ${req.headers['user-agent']}`);
    console.log(`🌐 IP: ${req.ip}`);
    
    const totem = await buscarTotem(id);
    
    if (!totem) {
        console.log(`❌ Totem não encontrado: ${id}`);
        console.log(`========================================`);
        return res.status(404).send('Totem não encontrado');
    }
    
    console.log(`✅ Totem encontrado no Firebase`);
    console.log(`📅 Data expiração: ${totem.dataExpiracao}`);
    
    if (dataExpirada(totem.dataExpiracao)) {
        console.log(`⛔ Totem expirado: ${id} (data: ${totem.dataExpiracao})`);
        console.log(`========================================`);
        return res.redirect('/expirado');
    }
    
    console.log(`📡 MQTT conectado: ${mqttClient && mqttClient.connected ? 'SIM' : 'NÃO'}`);
    const mqttPublished = publicarPlay(id);
    console.log(`📤 MQTT publicado: ${mqttPublished ? 'SUCESSO' : 'FALHOU'}`);
    
    console.log(`✅ Redirecionando para: ${totem.link}`);
    console.log(`========================================`);
    
    // Redirect direto e imperceptível para o Instagram (sem tela intermediária)
    res.redirect(302, totem.link);
});

app.get('/expirado', (req, res) => {
    res.sendFile(path.join(__dirname, 'views', 'expirado.html'));
});

// ========== ROTAS DE CLIENTE ==========

app.get('/app/:id', verificarAcessoCliente, async (req, res) => {
    try {
        const totem = req.totem;
        
        // Buscar configurações atuais do totem
        let configuracoes = {
            volume: 8,
            idle: {
                mode: 'BREATH',
                color: '#ff3366',
                maxBrightness: 120,
                speed: 50
            },
            trigger: {
                mode: 'RAINBOW',
                color: '#00ff00',
                maxBrightness: 150,
                speed: 70,
                duration: 30
            }
        };
        
        try {
            const configRef = await admin.firestore()
                .collection('totens')
                .doc(req.params.id)
                .get();
            
            if (configRef.exists) {
                const data = configRef.data();
                configuracoes.volume = data.volume ?? 8;
                if (data.idleConfig) configuracoes.idle = { ...configuracoes.idle, ...data.idleConfig };
                if (data.triggerConfig) configuracoes.trigger = { ...configuracoes.trigger, ...data.triggerConfig };
            }
        } catch (e) {
            console.log('Usando configurações padrão');
        }
        
        // Preparar dados para o template
        const dados = {
            ID: req.params.id,
            LINK: totem.link || 'Não configurado',
            DATA_EXPIRACAO: totem.dataExpiracao ? 
                new Date(totem.dataExpiracao).toLocaleDateString('pt-BR') : 
                'Sem expiração',
            AUDIO_NOME: totem.audio?.nome || 'Nenhum áudio personalizado',
            VOLUME: configuracoes.volume || 8,
            IDLE_MODE: configuracoes.idle?.mode || 'BREATH',
            IDLE_COLOR: configuracoes.idle?.color || '#ff3366',
            IDLE_BRIGHTNESS: configuracoes.idle?.maxBrightness || 120,
            IDLE_SPEED: configuracoes.idle?.speed || 50,
            TRIGGER_MODE: configuracoes.trigger?.mode || 'RAINBOW',
            TRIGGER_COLOR: configuracoes.trigger?.color || '#00ff00',
            TRIGGER_BRIGHTNESS: configuracoes.trigger?.maxBrightness || 150,
            TRIGGER_SPEED: configuracoes.trigger?.speed || 70,
            TRIGGER_DURATION: configuracoes.trigger?.duration || 30
        };

        // Renderizar o dashboard do cliente
        res.send(renderClienteDashboard(dados));
        
    } catch (error) {
        console.error('Erro ao carregar dashboard:', error);
        res.status(500).send('Erro ao carregar dashboard');
    }
});

function renderClienteDashboard(dados) {
    try {
        // Carregar o template HTML
        const template = fs.readFileSync(path.join(__dirname, 'views', 'cliente-dashboard.html'), 'utf8');
        
        // Substituir as variáveis
        return template
            .replace(/\{\{ID\}\}/g, dados.ID)
            .replace(/\{\{LINK\}\}/g, dados.LINK)
            .replace(/\{\{DATA_EXPIRACAO\}\}/g, dados.DATA_EXPIRACAO)
            .replace(/\{\{AUDIO_NOME\}\}/g, dados.AUDIO_NOME)
            .replace(/\{\{VOLUME\}\}/g, dados.VOLUME)
            .replace(/\{\{IDLE_MODE\}\}/g, dados.IDLE_MODE)
            .replace(/\{\{IDLE_COLOR\}\}/g, dados.IDLE_COLOR)
            .replace(/\{\{IDLE_BRIGHTNESS\}\}/g, dados.IDLE_BRIGHTNESS)
            .replace(/\{\{IDLE_SPEED\}\}/g, dados.IDLE_SPEED)
            .replace(/\{\{TRIGGER_MODE\}\}/g, dados.TRIGGER_MODE)
            .replace(/\{\{TRIGGER_COLOR\}\}/g, dados.TRIGGER_COLOR)
            .replace(/\{\{TRIGGER_BRIGHTNESS\}\}/g, dados.TRIGGER_BRIGHTNESS)
            .replace(/\{\{TRIGGER_SPEED\}\}/g, dados.TRIGGER_SPEED)
            .replace(/\{\{TRIGGER_DURATION\}\}/g, dados.TRIGGER_DURATION);
    } catch (error) {
        return '<h1>Erro ao carregar dashboard</h1>';
    }
}

// Rota de redirecionamento para compatibilidade (antiga URL cliente/dashboard)
app.get('/cliente/dashboard/:id', (req, res) => {
    res.redirect(`/app/${req.params.id}`);
});

// ========== ROTA DE CONFIGURAÇÃO (ILUMINAÇÃO / EFEITOS) ==========

app.post('/cliente/config/:id', async (req, res) => {
    const id = req.params.id;
    
    // Verificar se o totem existe
    if (!id) {
        return res.status(400).json({ error: 'ID do totem não fornecido' });
    }
    const body = req.body || {};

    // Suporta tanto o formato antigo (único config) quanto o novo (idle + trigger + volume)
    let idleConfig, triggerConfig, volume;

    if (body.idle && body.trigger) {
        // Novo formato: configurações duais
        idleConfig = {
            mode: String(body.idle.mode || 'BREATH').toUpperCase(),
            color: String(body.idle.color || '#FF3366'),
            speed: Number(body.idle.speed ?? 50),
            maxBrightness: Number(body.idle.maxBrightness ?? 120),
            updatedAt: new Date().toISOString()
        };

        triggerConfig = {
            mode: String(body.trigger.mode || 'RAINBOW').toUpperCase(),
            color: String(body.trigger.color || '#00FF00'),
            speed: Number(body.trigger.speed ?? 70),
            duration: Number(body.trigger.duration ?? 30),
            maxBrightness: Number(body.trigger.maxBrightness ?? 150),
            updatedAt: new Date().toISOString()
        };

        volume = Number(body.volume ?? 8);

        // Sanitização
        idleConfig.speed = Math.max(1, Math.trunc(idleConfig.speed));
        idleConfig.maxBrightness = Math.max(0, Math.min(180, Math.trunc(idleConfig.maxBrightness)));

        triggerConfig.speed = Math.max(1, Math.trunc(triggerConfig.speed));
        triggerConfig.duration = Math.max(1, Math.trunc(triggerConfig.duration));
        triggerConfig.maxBrightness = Math.max(0, Math.min(180, Math.trunc(triggerConfig.maxBrightness)));

        volume = Math.max(0, Math.min(10, Math.trunc(volume)));
    } else {
        // Formato antigo: compatibilidade retroativa (aplica ao trigger)
        const payload = {
            mode: String(body.mode || 'BREATH').toUpperCase(),
            color: String(body.color || '#FF3366'),
            speed: Number(body.speed ?? 50),
            duration: Number(body.duration ?? 30),
            maxBrightness: Number(body.maxBrightness ?? 120)
        };

        payload.speed = Math.max(1, Math.trunc(payload.speed));
        payload.duration = Math.max(1, Math.trunc(payload.duration));
        payload.maxBrightness = Math.max(0, Math.min(180, Math.trunc(payload.maxBrightness)));

        // Usa o mesmo para idle e trigger (compatibilidade)
        idleConfig = { ...payload, updatedAt: new Date().toISOString() };
        triggerConfig = { ...payload, updatedAt: new Date().toISOString() };
        volume = 8;
    }

    try {
        if (db) {
            await db.collection('totens').doc(id).set({
                idleConfig,
                triggerConfig,
                volume,
                ultimaAtualizacaoConfig: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
        }

        // Publica configurações via MQTT em tópicos separados (retained)
        if (mqttClient && mqttClient.connected) {
            mqttClient.publish(`totem/${id}/config/idle`, JSON.stringify(idleConfig), { retain: true });
            mqttClient.publish(`totem/${id}/config/trigger`, JSON.stringify(triggerConfig), { retain: true });
            mqttClient.publish(`totem/${id}/config/volume`, String(volume), { retain: true });
            console.log(`📤 Config MQTT publicada: idle, trigger e volume para ${id}`);
        }

        return res.json({
            success: true,
            message: 'Configuração enviada com sucesso',
            idleConfig,
            triggerConfig,
            volume
        });
    } catch (error) {
        console.error('❌ Erro ao salvar/enviar config:', error);
        return res.status(500).json({
            success: false,
            error: 'Erro interno: ' + error.message
        });
    }
});

// Endpoint para cliente carregar configuração atual
app.get('/cliente/config/:id', async (req, res) => {
    const id = req.params.id;
    
    if (!id) {
        return res.status(400).json({ error: 'ID do totem não fornecido' });
    }

    if (!db) {
        return res.json({
            volume: 8,
            idle: null,
            trigger: null,
            message: 'Firebase não disponível'
        });
    }

    try {
        const doc = await db.collection('totens').doc(id).get();
        if (!doc.exists) {
            return res.status(404).json({ error: 'Totem não encontrado' });
        }

        const data = doc.data();
        return res.json({
            volume: data.volume ?? 8,
            idle: data.idleConfig || null,
            trigger: data.triggerConfig || null,
            updatedAt: data.ultimaAtualizacaoConfig || null
        });
    } catch (error) {
        console.error('Erro ao buscar config:', error);
        return res.status(500).json({ error: 'Erro interno no servidor' });
    }
});

// Endpoint para atualizar apenas o volume
app.post('/cliente/volume/:id', async (req, res) => {
    const id = req.params.id;
    
    if (!id) {
        return res.status(400).json({ error: 'ID do totem não fornecido' });
    }
    const volume = Math.max(0, Math.min(10, Math.trunc(Number(req.body.volume ?? 8))));

    try {
        if (db) {
            await db.collection('totens').doc(id).set({
                volume,
                ultimaAtualizacaoVolume: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
        }

        // Publica volume via MQTT (retained)
        if (mqttClient && mqttClient.connected) {
            mqttClient.publish(`totem/${id}/config/volume`, String(volume), { retain: true });
            console.log(`📤 Volume MQTT publicado: ${volume} para ${id}`);
        }

        return res.json({
            success: true,
            message: 'Volume atualizado com sucesso',
            volume
        });
    } catch (error) {
        console.error('❌ Erro ao salvar volume:', error);
        return res.status(500).json({
            success: false,
            error: 'Erro interno: ' + error.message
        });
    }
});

// Endpoint para atualizar apenas LED Idle
app.post('/cliente/led-idle/:id', async (req, res) => {
    const id = req.params.id;
    
    if (!id) {
        return res.status(400).json({ error: 'ID do totem não fornecido' });
    }

    const body = req.body || {};
    
    if (!body.idle) {
        return res.status(400).json({ error: 'Configuração idle não fornecida' });
    }

    const idleConfig = {
        mode: String(body.idle.mode || 'BREATH').toUpperCase(),
        color: String(body.idle.color || '#FF3366'),
        speed: Number(body.idle.speed ?? 50),
        maxBrightness: Number(body.idle.maxBrightness ?? 120),
        updatedAt: new Date().toISOString()
    };

    // Sanitização
    idleConfig.speed = Math.max(1, Math.trunc(idleConfig.speed));
    idleConfig.maxBrightness = Math.max(0, Math.min(180, Math.trunc(idleConfig.maxBrightness)));

    try {
        if (db) {
            await db.collection('totens').doc(id).set({
                idleConfig,
                ultimaAtualizacaoConfig: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
        }

        // Publica configuração idle via MQTT (retained)
        if (mqttClient && mqttClient.connected) {
            mqttClient.publish(`totem/${id}/config/idle`, JSON.stringify(idleConfig), { retain: true });
            console.log(`📤 Config Idle MQTT publicada para ${id}`);
        }

        return res.json({
            success: true,
            message: 'Configuração de iluminação em espera atualizada com sucesso',
            idleConfig
        });
    } catch (error) {
        console.error('❌ Erro ao salvar config idle:', error);
        return res.status(500).json({
            success: false,
            error: 'Erro interno: ' + error.message
        });
    }
});

// Endpoint para atualizar apenas LED Trigger
app.post('/cliente/led-trigger/:id', async (req, res) => {
    const id = req.params.id;
    
    if (!id) {
        return res.status(400).json({ error: 'ID do totem não fornecido' });
    }

    const body = req.body || {};
    
    if (!body.trigger) {
        return res.status(400).json({ error: 'Configuração trigger não fornecida' });
    }

    const triggerConfig = {
        mode: String(body.trigger.mode || 'RAINBOW').toUpperCase(),
        color: String(body.trigger.color || '#00FF00'),
        speed: Number(body.trigger.speed ?? 70),
        duration: Number(body.trigger.duration ?? 30),
        maxBrightness: Number(body.trigger.maxBrightness ?? 150),
        updatedAt: new Date().toISOString()
    };

    // Sanitização
    triggerConfig.speed = Math.max(1, Math.trunc(triggerConfig.speed));
    triggerConfig.duration = Math.max(1, Math.trunc(triggerConfig.duration));
    triggerConfig.maxBrightness = Math.max(0, Math.min(180, Math.trunc(triggerConfig.maxBrightness)));

    try {
        if (db) {
            await db.collection('totens').doc(id).set({
                triggerConfig,
                ultimaAtualizacaoConfig: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
        }

        // Publica configuração trigger via MQTT (retained)
        if (mqttClient && mqttClient.connected) {
            mqttClient.publish(`totem/${id}/config/trigger`, JSON.stringify(triggerConfig), { retain: true });
            console.log(`📤 Config Trigger MQTT publicada para ${id}`);
        }

        return res.json({
            success: true,
            message: 'Configuração de iluminação ao disparar atualizada com sucesso',
            triggerConfig
        });
    } catch (error) {
        console.error('❌ Erro ao salvar config trigger:', error);
        return res.status(500).json({
            success: false,
            error: 'Erro interno: ' + error.message
        });
    }
});

// ========== ROTA DE UPLOAD COM VALIDAÇÃO RIGOROSA (v4.2.1) ==========

app.post('/cliente/audio/:id', upload.single('audio'), async (req, res) => {
    const id = req.params.id;
    
    if (!id) {
        return res.status(400).json({ error: 'ID do totem não fornecido' });
    }
    const file = req.file;
    
    if (!file) {
        return res.status(400).json({ error: 'Nenhum arquivo enviado' });
    }
    
    console.log(`📥 Upload de áudio para ${id}: ${file.filename}`);
    console.log(`📁 Arquivo salvo em: ${file.path}`);
    console.log(`📏 Tamanho: ${file.size} bytes`);
    console.log(`🏷️ Tipo: ${file.mimetype}`);
    
    let finalFilePath = null;
    let finalFileName = null;

    try {
        // ========== CAMADA 2: VALIDAÇÃO SERVIDOR (FFPROBE) ==========
        console.log('🔍 Validando arquivo com ffprobe...');
        
        await new Promise((resolve, reject) => {
            ffmpeg.ffprobe(file.path, (err, metadata) => {
                if (err) {
                    console.error('❌ FFprobe falhou:', err.message);
                    reject(new Error('Arquivo inválido ou corrompido'));
                    return;
                }
                
                // Verificar se tem stream de áudio
                const audioStream = metadata.streams.find(s => s.codec_type === 'audio');
                if (!audioStream) {
                    console.error('❌ Arquivo não contém stream de áudio');
                    reject(new Error('Arquivo não contém áudio'));
                    return;
                }
                
                // Verificar codec (deve ser MP3 ou conversível)
                const codecsSuportados = ['mp3', 'aac', 'vorbis', 'opus', 'pcm_s16le'];
                if (!codecsSuportados.includes(audioStream.codec_name)) {
                    console.error(`❌ Codec não suportado: ${audioStream.codec_name}`);
                    reject(new Error(`Codec não suportado: ${audioStream.codec_name}. Use MP3, AAC, OGG ou WAV.`));
                    return;
                }
                
                // Log da duração (sem limite - streaming HTTP suporta arquivos longos)
                const duracao = metadata.format.duration;
                console.log(`✅ Validação ffprobe OK: codec=${audioStream.codec_name}, duração=${Math.round(duracao)}s`);
                resolve();
            });
        });
        
        // Se passou na validação, prosseguir com conversão/upload
        const storagePodeSerUsado = !!bucket;

        if (storagePodeSerUsado && storageDisponivel === null) {
            try {
                const [exists] = await bucket.exists();
                storageDisponivel = !!exists;
                if (!storageDisponivel) {
                    console.error(`❌ Firebase Storage indisponível: bucket não existe (${bucket.name})`);
                }
            } catch (e) {
                storageDisponivel = false;
                console.error(`❌ Falha ao verificar bucket do Firebase Storage (${bucket.name}):`, e && e.message ? e.message : e);
            }
        }

        const usarFirebaseStorage = storagePodeSerUsado && storageDisponivel !== false;

        const ext = path.extname(file.filename).toLowerCase();
        finalFilePath = file.path;
        finalFileName = file.filename;

        // Se não for MP3, converte para MP3 (mantém compatibilidade com ESP32)
        if (ext !== '.mp3') {
            const mp3Name = file.filename.replace(new RegExp(`${ext}$`, 'i'), '') + '.mp3';
            const mp3Path = path.join(uploadDir, mp3Name);

            console.log(`🎛️ Convertendo para MP3: ${file.filename} -> ${mp3Name}`);

            await new Promise((resolve, reject) => {
                ffmpeg(file.path)
                    .outputOptions([
                        '-vn',
                        '-acodec', 'libmp3lame',
                        '-b:a', '128k',
                        '-ac', '2',
                        '-ar', '44100'
                    ])
                    .toFormat('mp3')
                    .on('end', resolve)
                    .on('error', reject)
                    .save(mp3Path);
            });

            try {
                fs.removeSync(file.path);
            } catch (e) {
                console.warn('Erro ao remover arquivo original após conversão:', e);
            }

            finalFilePath = mp3Path;
            finalFileName = mp3Name;
        }

        let audioData = {
            nome: file.originalname,
            tamanho: fs.existsSync(finalFilePath) ? fs.statSync(finalFilePath).size : file.size,
            dataUpload: new Date().toISOString(),
            filename: finalFileName,
            mimetype: 'audio/mpeg'
        };

        let audioUrl = null;

        if (usarFirebaseStorage) {
            const destination = `audios/${id}/${finalFileName}`;
            console.log(`📤 Enviando para Firebase Storage: ${destination}`);

            await bucket.upload(finalFilePath, { 
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

            const fileRef = bucket.file(destination);
            await fileRef.makePublic();

            audioUrl = `https://storage.googleapis.com/${bucket.name}/${destination}`;
            audioData.url = audioUrl;
            audioData.storagePath = destination;

            console.log(`✅ Áudio público em: ${audioUrl}`);

            // Limpa arquivo temporário após upload bem-sucedido
            try {
                fs.removeSync(finalFilePath);
                console.log('🗑️ Arquivo temporário removido');
            } catch (e) {
                console.warn('Erro ao remover temporário:', e);
            }
        } else {
            let base = String(SERVER_URL || '').replace(/\/$/, '');
            // Garantir que a URL use HTTPS (ESP32 requer HTTPS para streaming)
            if (base.startsWith('http://')) {
                base = base.replace('http://', 'https://');
                console.log('⚠️ Convertendo HTTP para HTTPS para compatibilidade com ESP32');
            }
            audioUrl = `${base}/uploads/${encodeURIComponent(finalFileName)}`;
            audioData.url = audioUrl;
            audioData.storagePath = null;
            console.warn(`⚠️ Firebase Storage indisponível. Usando fallback local: ${audioUrl}`);
        }

        if (db) {
            await db.collection('totens').doc(id).set({
                audio: audioData,
                ultimaAtualizacaoAudio: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
            console.log(`✅ Referência salva no Firestore para ${id}`);
        }

        // NOTIFICA O ESP32 IMEDIATAMENTE
        notificarAtualizacaoAudio(id, audioData.dataUpload);

        return res.json({
            success: true,
            message: 'Áudio enviado com sucesso',
            audio: {
                nome: file.originalname,
                tamanho: audioData.tamanho,
                url: audioUrl,
                versao: audioData.dataUpload
            }
        });

    } catch (error) {
        console.error('❌ Erro no upload:', error);

        // Remover arquivo inválido imediatamente
        try {
            if (file && file.path && fs.existsSync(file.path)) {
                fs.removeSync(file.path);
                console.log('🗑️ Arquivo inválido removido');
            }
        } catch (e) {
            console.warn('Erro ao limpar arquivo temporário (file.path) após falha:', e);
        }
        try {
            if (typeof finalFilePath === 'string' && fs.existsSync(finalFilePath)) {
                fs.removeSync(finalFilePath);
            }
        } catch (e) {
            console.warn('Erro ao limpar arquivo temporário (finalFilePath) após falha:', e);
        }

        // Retornar erro específico
        if (error && error.code === 404 && String(error.message || '').toLowerCase().includes('bucket')) {
            console.error(`❌ Firebase Storage retornou 404 (bucket não existe). Bucket configurado: ${bucket && bucket.name ? bucket.name : 'desconhecido'}`);
            return res.status(500).json({ 
                success: false,
                error: 'Erro no Firebase Storage' 
            });
        }

        return res.status(400).json({ 
            success: false,
            error: error.message || 'Arquivo de áudio inválido'
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
                console.log(`✅ Retornando URL: ${audio.url}`);
                
                return res.json({
                    url: audio.url,
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
    const id = req.params.id;
    
    // Permitir apenas se for admin autenticado
    if (!req.session || !req.session.adminAutenticado) {
        return res.status(401).json({ error: 'Não autorizado - apenas admin' });
    }
    
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

app.post('/admin/login', async (req, res) => {
    const { senha } = req.body;
    
    const senhaCorreta = await buscarSenhaAdmin();
    
    if (senha === senhaCorreta) {
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
        
        let audioCell = '';
        if (totem.audio && totem.audio.url) {
            const audioUrl = totem.audio.url;
            const audioNome = (totem.audio.nome || 'audio.mp3').replace(/</g, '&lt;').replace(/>/g, '&gt;');
            
            // Escapar URL corretamente para JavaScript string dentro de HTML onclick
            const audioUrlEscaped = audioUrl
                .replace(/\\/g, '\\\\')
                .replace(/'/g, "\\'")
                .replace(/"/g, '\\"')
                .replace(/\n/g, '\\n')
                .replace(/\r/g, '\\r');
            
            audioCell = `
                <div class="audio-info">
                    <span class="audio-nome" title="${audioNome}">${audioNome}</span>
                    <button class="btn-play" onclick="playAudio('${audioUrlEscaped}')">▶️ Play</button>
                </div>
            `;
        } else {
            audioCell = '<span class="sem-audio">Sem áudio personalizado</span>';
        }
        
        linhasTabela += `
            <tr>
                <td>${totem.id} ${temAudio}</td>
                <td><a href="${totem.link}" target="_blank">${totem.link}</a></td>
                <td>${formatarData(totem.dataExpiracao)}</td>
                <td class="${expirada ? 'status-bloqueado' : 'status-ativo'}">${statusIcon} ${statusText}</td>
                <td>${audioCell}</td>
                <td>
                    <button class="btn-qr" onclick="copiarLinkQR('${totem.id}')">📋 Copiar Link QR</button>
                    <button class="btn-qr" onclick="gerarLinkDireto('${totem.id}')" style="margin-left: 5px; background: #17a2b8;">🔗 Link App</button>
                </td>
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

app.post('/admin/disparar/:id', adminAuth, async (req, res) => {
    const id = req.params.id;
    
    console.log(`🎯 Admin disparando totem ${id}`);
    
    if (mqttClient && mqttClient.connected) {
        mqttClient.publish(`totem/${id}/trigger`, 'play');
        console.log(`📤 MQTT publicado: totem/${id}/trigger = play`);
        
        return res.json({ success: true, message: 'Totem disparado!' });
    } else {
        console.log('❌ MQTT não conectado');
        return res.status(500).json({ success: false, message: 'MQTT não conectado' });
    }
});

// ========== INICIAR SERVIDOR ==========

const server = app.listen(PORT, () => {
    console.log('\n' + '='.repeat(50));
    console.log('🚀 TOTEM SERVER v4.2.1 - SISTEMA ROBUSTO');
    console.log('='.repeat(50));
    console.log(`📡 Porta: ${PORT}`);
    console.log(`🌐 URL: ${SERVER_URL}`);
    console.log(`📊 Admin: ${SERVER_URL}/admin/login`);
    console.log(`👤 Cliente: ${SERVER_URL}/cliente/login`);
    console.log(`📡 MQTT: ${MQTT_BROKER}:${MQTT_PORT}`);
    console.log(`🔥 Firebase: ${firebaseInicializado ? '✅ Conectado' : '❌ Não disponível'}`);
    console.log('='.repeat(50) + '\n');
});

// Configurar timeout do servidor para evitar conexões travadas
server.timeout = 180000; // 3 minutos (suficiente para upload de 5MB)
server.keepAliveTimeout = 65000; // 65 segundos
server.headersTimeout = 70000; // 70 segundos