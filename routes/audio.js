// ============================================
// Rotas de Gerenciamento de Áudio
// TOTEM INTERATIVO IoT v4.0
// ============================================

const express = require('express');
const multer = require('multer');
const path = require('path');
const fs = require('fs-extra');
const admin = require('firebase-admin');
const { v4: uuidv4 } = require('uuid');
const router = express.Router();

// Configuração do multer para upload de áudio
const uploadDir = path.join(__dirname, '..', 'uploads');
fs.ensureDirSync(uploadDir);

const storage = multer.diskStorage({
    destination: function (req, file, cb) {
        cb(null, uploadDir);
    },
    filename: function (req, file, cb) {
        const totemId = req.params.id || 'temp';
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
        fileSize: 5 * 1024 * 1024, // 5MB
        files: 1
    }
});

// ========== ROTAS DE ÁUDIO ==========

/**
 * GET /api/audio/:id
 * Endpoint para ESP32 verificar se há novo áudio
 * Retorna URL do áudio atual e versão
 */
router.get('/:id', async (req, res) => {
    const id = req.params.id;
    const db = req.app.locals.db;
    const bucket = req.app.locals.bucket;
    const firebaseInicializado = req.app.locals.firebaseInicializado;
    
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
 * POST /api/audio/:id/upload
 * Upload de novo áudio (cliente autenticado)
 */
router.post('/:id/upload', 
    (req, res, next) => {
        // Verificar autenticação do cliente
        if (!req.session || !req.session.clienteTotemId || req.session.clienteTotemId !== req.params.id) {
            return res.status(401).json({ error: 'Não autorizado' });
        }
        next();
    },
    upload.single('audio'),
    async (req, res) => {
        const id = req.params.id;
        const file = req.file;
        
        if (!file) {
            return res.status(400).json({ error: 'Nenhum arquivo enviado' });
        }
        
        console.log(`📥 Upload de áudio para ${id}: ${file.filename}`);
        
        const db = req.app.locals.db;
        const bucket = req.app.locals.bucket;
        const mqttClient = req.app.locals.mqttClient;
        
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
    }
);

/**
 * DELETE /api/audio/:id
 * Remove áudio personalizado e volta ao padrão
 */
router.delete('/:id', async (req, res) => {
    // Verificar autenticação (admin ou cliente dono do totem)
    if (!req.session || 
        (!req.session.adminAutenticado && req.session.clienteTotemId !== req.params.id)) {
        return res.status(401).json({ error: 'Não autorizado' });
    }
    
    const id = req.params.id;
    const db = req.app.locals.db;
    const bucket = req.app.locals.bucket;
    
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
 * Retorna informações detalhadas do áudio atual
 */
router.get('/:id/info', async (req, res) => {
    const id = req.params.id;
    const db = req.app.locals.db;
    
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

module.exports = router;