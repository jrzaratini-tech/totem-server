const express = require('express');
const router = express.Router();
const multer = require('multer');
const path = require('path');
const fs = require('fs-extra');
const { adminAuth } = require('../middlewares/auth');
const { validarFirmware } = require('../utils/firmwareValidator');
const FirebaseOTAService = require('../services/firebaseOTA');
const MQTTOTAService = require('../services/mqttOTA');
const JobManager = require('../services/jobManager');

const uploadDir = path.join(__dirname, '..', 'uploads', 'firmware');
fs.ensureDirSync(uploadDir);

const storage = multer.diskStorage({
    destination: function (req, file, cb) {
        cb(null, uploadDir);
    },
    filename: function (req, file, cb) {
        const timestamp = Date.now();
        const ext = path.extname(file.originalname);
        cb(null, `firmware_${timestamp}${ext}`);
    }
});

const uploadFirmware = multer({
    storage: storage,
    limits: { fileSize: 4 * 1024 * 1024 },
    fileFilter: (req, file, cb) => {
        if (path.extname(file.originalname).toLowerCase() === '.bin') {
            cb(null, true);
        } else {
            cb(new Error('Apenas arquivos .bin são permitidos'));
        }
    }
});

const rateLimitMap = new Map();
const RATE_LIMIT_UPLOADS = 5;
const RATE_LIMIT_WINDOW = 60 * 60 * 1000;

function checkRateLimit(req, res, next) {
    const userId = req.session?.adminAutenticado || req.ip;
    const agora = Date.now();
    
    if (!rateLimitMap.has(userId)) {
        rateLimitMap.set(userId, []);
    }
    
    const uploads = rateLimitMap.get(userId).filter(timestamp => agora - timestamp < RATE_LIMIT_WINDOW);
    
    if (uploads.length >= RATE_LIMIT_UPLOADS) {
        return res.status(429).json({
            success: false,
            erro: `Limite de ${RATE_LIMIT_UPLOADS} uploads por hora atingido`
        });
    }
    
    uploads.push(agora);
    rateLimitMap.set(userId, uploads);
    next();
}

function initOTARoutes(mqttClient) {
    const firebaseOTA = new FirebaseOTAService();
    const mqttOTA = new MQTTOTAService(mqttClient);
    const jobManager = new JobManager(mqttOTA);

    router.post('/upload', adminAuth, checkRateLimit, uploadFirmware.single('firmware'), async (req, res) => {
        let filePath = null;
        
        try {
            if (!req.file) {
                return res.status(400).json({
                    success: false,
                    erro: 'Nenhum arquivo enviado'
                });
            }

            filePath = req.file.path;

            const validacao = validarFirmware(filePath);
            
            if (!validacao.valido) {
                fs.unlinkSync(filePath);
                return res.status(400).json({
                    success: false,
                    erro: validacao.erro
                });
            }

            const versao = validacao.versao || req.body.versao;
            
            if (!versao) {
                fs.unlinkSync(filePath);
                return res.status(400).json({
                    success: false,
                    erro: 'Versão não encontrada no nome do arquivo. Use formato: firmware_vX.X.X.bin'
                });
            }

            await firebaseOTA.registrarAuditoria('upload_firmware', req.session.adminAutenticado || 'admin', {
                versao,
                tamanho: validacao.tamanho,
                checksum: validacao.checksum,
                ip: req.ip
            });

            res.json({
                success: true,
                message: 'Firmware validado com sucesso',
                firmware: {
                    versao,
                    tamanho: validacao.tamanho,
                    tamanhoMB: validacao.tamanhoMB,
                    checksum: validacao.checksum,
                    nomeArquivo: validacao.nomeArquivo,
                    path: filePath
                }
            });

        } catch (error) {
            console.error('Erro no upload de firmware:', error);
            if (filePath && fs.existsSync(filePath)) {
                fs.unlinkSync(filePath);
            }
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.post('/individual/:totemId', adminAuth, async (req, res) => {
        try {
            const { totemId } = req.params;
            const { versao, firmwarePath, checksum, tamanho } = req.body;

            if (!versao || !firmwarePath) {
                return res.status(400).json({
                    success: false,
                    erro: 'Versão e caminho do firmware são obrigatórios'
                });
            }

            if (!fs.existsSync(firmwarePath)) {
                return res.status(400).json({
                    success: false,
                    erro: 'Arquivo de firmware não encontrado'
                });
            }

            const validacao = validarFirmware(firmwarePath);
            if (!validacao.valido) {
                return res.status(400).json({
                    success: false,
                    erro: validacao.erro
                });
            }

            const uploadResult = await firebaseOTA.uploadFirmware(
                firmwarePath,
                totemId,
                versao,
                checksum || validacao.checksum
            );

            if (!uploadResult.success) {
                return res.status(500).json({
                    success: false,
                    erro: uploadResult.erro
                });
            }

            await firebaseOTA.salvarMetadadosOTA(
                totemId,
                versao,
                checksum || validacao.checksum,
                uploadResult.url,
                uploadResult.path,
                tamanho || validacao.tamanho
            );

            const resultado = await jobManager.executarJobIndividual(
                totemId,
                versao,
                uploadResult.url,
                checksum || validacao.checksum,
                tamanho || validacao.tamanho,
                req.session.adminAutenticado || 'admin'
            );

            if (fs.existsSync(firmwarePath)) {
                fs.unlinkSync(firmwarePath);
            }

            res.json(resultado);

        } catch (error) {
            console.error('Erro ao atualizar totem individual:', error);
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.post('/massa', adminAuth, async (req, res) => {
        try {
            const { totens, versao, firmwarePath, agendamento, checksum, tamanho } = req.body;

            if (!totens || !Array.isArray(totens) || totens.length === 0) {
                return res.status(400).json({
                    success: false,
                    erro: 'Lista de totens inválida'
                });
            }

            if (!versao || !firmwarePath) {
                return res.status(400).json({
                    success: false,
                    erro: 'Versão e caminho do firmware são obrigatórios'
                });
            }

            if (!fs.existsSync(firmwarePath)) {
                return res.status(400).json({
                    success: false,
                    erro: 'Arquivo de firmware não encontrado'
                });
            }

            const validacao = validarFirmware(firmwarePath);
            if (!validacao.valido) {
                return res.status(400).json({
                    success: false,
                    erro: validacao.erro
                });
            }

            const jobResult = await firebaseOTA.criarJobMassa(
                totens,
                versao,
                agendamento,
                req.session.adminAutenticado || 'admin'
            );

            if (!jobResult.success) {
                return res.status(500).json(jobResult);
            }

            const jobId = jobResult.jobId;

            const uploadPromises = totens.map(totemId => 
                firebaseOTA.uploadFirmware(
                    firmwarePath,
                    totemId,
                    versao,
                    checksum || validacao.checksum
                )
            );

            const uploadResults = await Promise.all(uploadPromises);
            const primeiroUpload = uploadResults.find(r => r.success);

            if (!primeiroUpload) {
                return res.status(500).json({
                    success: false,
                    erro: 'Falha ao fazer upload do firmware'
                });
            }

            const metadadosPromises = totens.map(totemId =>
                firebaseOTA.salvarMetadadosOTA(
                    totemId,
                    versao,
                    checksum || validacao.checksum,
                    primeiroUpload.url,
                    primeiroUpload.path,
                    tamanho || validacao.tamanho
                )
            );

            await Promise.all(metadadosPromises);

            if (agendamento) {
                const agendamentoResult = await jobManager.agendarJob(
                    jobId,
                    totens,
                    versao,
                    primeiroUpload.url,
                    checksum || validacao.checksum,
                    tamanho || validacao.tamanho,
                    agendamento,
                    req.session.adminAutenticado || 'admin'
                );

                if (fs.existsSync(firmwarePath)) {
                    fs.unlinkSync(firmwarePath);
                }

                return res.json({
                    success: true,
                    jobId,
                    message: `Job agendado para ${totens.length} totens`,
                    agendamento: agendamentoResult.agendamento
                });
            } else {
                setImmediate(async () => {
                    await jobManager.executarJobMassa(
                        jobId,
                        totens,
                        versao,
                        primeiroUpload.url,
                        checksum || validacao.checksum,
                        tamanho || validacao.tamanho,
                        req.session.adminAutenticado || 'admin'
                    );

                    if (fs.existsSync(firmwarePath)) {
                        fs.unlinkSync(firmwarePath);
                    }
                });

                res.json({
                    success: true,
                    jobId,
                    message: `Job criado para ${totens.length} totens`,
                    total: totens.length
                });
            }

        } catch (error) {
            console.error('Erro ao criar job em massa:', error);
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.get('/job/:jobId', adminAuth, async (req, res) => {
        try {
            const { jobId } = req.params;
            const resultado = await firebaseOTA.obterStatusJob(jobId);
            res.json(resultado);
        } catch (error) {
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.get('/status/:totemId', adminAuth, async (req, res) => {
        try {
            const { totemId } = req.params;
            const admin = require('firebase-admin');
            const doc = await admin.firestore().collection('totens').doc(totemId).get();
            
            if (!doc.exists) {
                return res.status(404).json({
                    success: false,
                    erro: 'Totem não encontrado'
                });
            }

            const firmware = doc.data().firmware || {};
            
            res.json({
                success: true,
                totemId,
                firmware: {
                    versaoAtual: firmware.atual || 'Desconhecida',
                    versaoDisponivel: firmware.versaoDisponivel || null,
                    status: firmware.status || 'online',
                    ultimaAtualizacao: firmware.ultimaAtualizacao || null,
                    ultimoBoot: firmware.ultimoBoot || null
                }
            });

        } catch (error) {
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.post('/rollback/:totemId', adminAuth, async (req, res) => {
        try {
            const { totemId } = req.params;
            const { versao } = req.body;

            if (!versao) {
                return res.status(400).json({
                    success: false,
                    erro: 'Versão para rollback é obrigatória'
                });
            }

            const historicoResult = await firebaseOTA.obterHistoricoVersoes(totemId);
            
            if (!historicoResult.success) {
                return res.status(404).json(historicoResult);
            }

            const versaoAnterior = historicoResult.historico.find(v => v.versao === versao);
            
            if (!versaoAnterior) {
                return res.status(404).json({
                    success: false,
                    erro: `Versão ${versao} não encontrada no histórico`
                });
            }

            let url = versaoAnterior.url;
            
            if (!url || url.includes('X-Goog-Signature')) {
                url = await firebaseOTA.gerarURLAssinada(versaoAnterior.storagePath);
            }

            const resultado = await mqttOTA.publicarRollback(
                totemId,
                versao,
                url,
                versaoAnterior.checksum
            );

            if (resultado.success) {
                await firebaseOTA.atualizarStatusOTA(totemId, 'updating', versao);
                await firebaseOTA.registrarAuditoria('rollback', req.session.adminAutenticado || 'admin', {
                    totemId,
                    versao,
                    ip: req.ip
                });
            }

            res.json(resultado);

        } catch (error) {
            console.error('Erro ao fazer rollback:', error);
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.get('/versoes/:totemId', adminAuth, async (req, res) => {
        try {
            const { totemId } = req.params;
            const resultado = await firebaseOTA.obterHistoricoVersoes(totemId);
            res.json(resultado);
        } catch (error) {
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.post('/cancelar/:jobId', adminAuth, async (req, res) => {
        try {
            const { jobId } = req.params;
            const resultado = await jobManager.cancelarJobAgendado(jobId);
            res.json(resultado);
        } catch (error) {
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.get('/jobs/ativos', adminAuth, async (req, res) => {
        try {
            const jobs = jobManager.obterJobsAtivos();
            res.json({
                success: true,
                jobs
            });
        } catch (error) {
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    router.get('/jobs/agendados', adminAuth, async (req, res) => {
        try {
            const jobs = jobManager.obterJobsAgendados();
            res.json({
                success: true,
                jobs
            });
        } catch (error) {
            res.status(500).json({
                success: false,
                erro: error.message
            });
        }
    });

    return router;
}

module.exports = initOTARoutes;
