// ============================================
// AUDIO EQUALIZER ROUTES
// Sistema de equalização de áudio por totem
// ============================================

const express = require('express');
const router = express.Router();
const path = require('path');

module.exports = function(db, mqttClient) {
    
    // Middleware para verificar autenticação do cliente
    function clienteAuth(req, res, next) {
        if (!req.session.clienteTotemId || req.session.clienteTotemId !== req.params.id) {
            return res.status(401).json({ error: 'Não autorizado' });
        }
        next();
    }
    
    // ========== PÁGINA DO EQUALIZADOR ==========
    router.get('/cliente/equalizer/:id', (req, res) => {
        if (!req.session.clienteTotemId || req.session.clienteTotemId !== req.params.id) {
            return res.redirect('/cliente/login');
        }
        
        res.sendFile(path.join(__dirname, '..', 'views', 'audio-equalizer.html'));
    });
    
    // ========== GET AUDIO CONFIG ==========
    router.get('/api/cliente/audio-config/:id', clienteAuth, async (req, res) => {
        const id = req.params.id;
        
        if (!db) {
            return res.json({
                success: false,
                message: 'Firebase não disponível',
                audioProfile: getDefaultProfile()
            });
        }
        
        try {
            const doc = await db.collection('totens').doc(id).get();
            
            if (!doc.exists) {
                return res.status(404).json({ 
                    success: false,
                    error: 'Totem não encontrado' 
                });
            }
            
            const data = doc.data();
            const audioProfile = data.audioProfile || getDefaultProfile();
            
            return res.json({
                success: true,
                audioProfile: audioProfile,
                totemId: id
            });
        } catch (error) {
            console.error('❌ Erro ao buscar audio config:', error);
            return res.status(500).json({ 
                success: false,
                error: 'Erro interno no servidor' 
            });
        }
    });
    
    // ========== SAVE AUDIO CONFIG ==========
    router.post('/api/cliente/audio-config/:id', clienteAuth, async (req, res) => {
        const id = req.params.id;
        const body = req.body;
        
        try {
            // Validar e sanitizar dados
            const audioProfile = {
                volume: Math.max(0, Math.min(10, parseInt(body.volume) || 8)),
                equalizer: {
                    bass: Math.max(-12, Math.min(12, parseFloat(body.equalizer?.bass) || 0)),
                    mid: Math.max(-12, Math.min(12, parseFloat(body.equalizer?.mid) || 0)),
                    treble: Math.max(-12, Math.min(12, parseFloat(body.equalizer?.treble) || 0))
                },
                amplifierGain: [3, 9, 15].includes(parseInt(body.amplifierGain)) ? parseInt(body.amplifierGain) : 15,
                espModel: body.espModel || 'ESP32 DevKit V1',
                volumeCurve: body.volumeCurve || 'exponential',
                clippingProtection: body.clippingProtection !== false,
                customGainPoints: body.customGainPoints || [],
                lastCalibrated: new Date().toISOString()
            };
            
            // Salvar no Firestore
            if (db) {
                await db.collection('totens').doc(id).set({
                    audioProfile: audioProfile,
                    ultimaAtualizacaoAudioProfile: new Date()
                }, { merge: true });
                
                console.log(`✅ Audio profile salvo para ${id}`);
            }
            
            // Publicar via MQTT para o ESP32
            if (mqttClient && mqttClient.connected) {
                const mqttPayload = {
                    volume: audioProfile.volume,
                    equalizer: audioProfile.equalizer,
                    amplifierGain: audioProfile.amplifierGain,
                    espModel: mapESPModelToInt(audioProfile.espModel),
                    volumeCurve: mapVolumeCurveToInt(audioProfile.volumeCurve),
                    clippingProtection: audioProfile.clippingProtection,
                    timestamp: Date.now()
                };
                
                const topic = `totem/${id}/audioConfig`;
                mqttClient.publish(topic, JSON.stringify(mqttPayload), { retain: true });
                console.log(`📤 MQTT publicado: ${topic}`);
            }
            
            return res.json({
                success: true,
                message: 'Configuração de áudio salva com sucesso',
                audioProfile: audioProfile
            });
            
        } catch (error) {
            console.error('❌ Erro ao salvar audio config:', error);
            return res.status(500).json({
                success: false,
                error: 'Erro interno: ' + error.message
            });
        }
    });
    
    // ========== TEST AUDIO ==========
    router.post('/api/cliente/audio-test/:id', clienteAuth, async (req, res) => {
        const id = req.params.id;
        const testType = req.body.testType || 'mid'; // bass, mid, treble
        
        try {
            if (mqttClient && mqttClient.connected) {
                const topic = `totem/${id}/audioTest`;
                const payload = JSON.stringify({
                    command: 'playTestTone',
                    testType: testType,
                    duration: 3000,
                    timestamp: Date.now()
                });
                
                mqttClient.publish(topic, payload);
                console.log(`📤 Test audio MQTT: ${topic} - ${testType}`);
                
                return res.json({
                    success: true,
                    message: `Teste de áudio (${testType}) enviado`
                });
            } else {
                return res.status(503).json({
                    success: false,
                    error: 'MQTT não disponível'
                });
            }
        } catch (error) {
            console.error('❌ Erro ao enviar teste de áudio:', error);
            return res.status(500).json({
                success: false,
                error: 'Erro interno: ' + error.message
            });
        }
    });
    
    // ========== TRIGGER TOTEM (PLAY) ==========
    router.post('/api/trigger/:id', clienteAuth, async (req, res) => {
        const id = req.params.id;
        
        try {
            if (mqttClient && mqttClient.connected) {
                const topic = `totem/${id}/trigger`;
                mqttClient.publish(topic, 'play');
                console.log(`📤 Trigger MQTT: ${topic} = play`);
                
                return res.json({
                    success: true,
                    message: 'Totem disparado com sucesso'
                });
            } else {
                return res.status(503).json({
                    success: false,
                    error: 'MQTT não disponível'
                });
            }
        } catch (error) {
            console.error('❌ Erro ao disparar totem:', error);
            return res.status(500).json({
                success: false,
                error: 'Erro interno: ' + error.message
            });
        }
    });
    
    return router;
};

// ========== HELPER FUNCTIONS ==========

function getDefaultProfile() {
    return {
        volume: 10,
        equalizer: {
            bass: 0,
            mid: 2,
            treble: 3
        },
        amplifierGain: 9,
        espModel: 'ESP32-S3 DevKit',
        volumeCurve: 'exponential_aggressive',
        clippingProtection: true,
        customGainPoints: [
            { vol: 0, gain: 0.0 },
            { vol: 3, gain: 0.3 },
            { vol: 5, gain: 0.5 },
            { vol: 8, gain: 0.876 },
            { vol: 10, gain: 1.2 }
        ],
        lastCalibrated: new Date().toISOString()
    };
}

function mapESPModelToInt(model) {
    const mapping = {
        'ESP32 DevKit V1': 0,
        'ESP32 AudioKit': 1,
        'ESP32-S3 DevKit': 2,
        'ESP32 Generic': 3
    };
    return mapping[model] || 0;
}

function mapVolumeCurveToInt(curve) {
    const mapping = {
        'linear': 0,
        'exponential': 1,
        'exponential_soft': 1,
        'exponential_aggressive': 2
    };
    return mapping[curve] || 1;
}
