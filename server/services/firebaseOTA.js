const admin = require('firebase-admin');
const fs = require('fs');
const path = require('path');

class FirebaseOTAService {
    constructor() {
        this.db = admin.firestore();
        this.bucket = admin.storage().bucket();
    }

    async uploadFirmware(filePath, totemId, versao, checksum) {
        try {
            const fileName = `${versao}.bin`;
            const destination = `firmware/${totemId}/${fileName}`;
            
            await this.bucket.upload(filePath, {
                destination: destination,
                metadata: {
                    contentType: 'application/octet-stream',
                    metadata: {
                        totemId: totemId,
                        versao: versao,
                        checksum: checksum,
                        uploadedAt: new Date().toISOString()
                    }
                }
            });

            const file = this.bucket.file(destination);
            const [url] = await file.getSignedUrl({
                action: 'read',
                expires: Date.now() + 60 * 60 * 1000
            });

            return {
                success: true,
                url: url,
                path: destination
            };

        } catch (error) {
            console.error('Erro ao fazer upload do firmware:', error);
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async gerarURLAssinada(storagePath, expiracaoHoras = 1) {
        try {
            const file = this.bucket.file(storagePath);
            const [url] = await file.getSignedUrl({
                action: 'read',
                expires: Date.now() + expiracaoHoras * 60 * 60 * 1000
            });

            return url;
        } catch (error) {
            throw new Error(`Erro ao gerar URL assinada: ${error.message}`);
        }
    }

    async salvarMetadadosOTA(totemId, versao, checksum, url, storagePath, tamanho) {
        try {
            const totemRef = this.db.collection('totens').doc(totemId);
            const totemDoc = await totemRef.get();

            if (!totemDoc.exists) {
                throw new Error(`Totem ${totemId} não encontrado`);
            }

            const metadados = {
                versao: versao,
                data: new Date().toISOString(),
                status: 'pending',
                checksum: checksum,
                url: url,
                storagePath: storagePath,
                tamanho: tamanho
            };

            const totemData = totemDoc.data();
            const historicoAtual = totemData.firmware?.historico || [];
            
            await totemRef.update({
                'firmware.versaoDisponivel': versao,
                'firmware.urlDownload': url,
                'firmware.checksum': checksum,
                'firmware.storagePath': storagePath,
                'firmware.tamanho': tamanho,
                'firmware.historico': admin.firestore.FieldValue.arrayUnion(metadados),
                'firmware.ultimaAtualizacao': new Date().toISOString()
            });

            return { success: true };

        } catch (error) {
            console.error('Erro ao salvar metadados OTA:', error);
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async atualizarStatusOTA(totemId, status, versao = null) {
        try {
            const updateData = {
                'firmware.status': status,
                'firmware.ultimaAtualizacao': new Date().toISOString()
            };

            if (status === 'success' && versao) {
                updateData['firmware.atual'] = versao;
                updateData['firmware.ultimoBoot'] = new Date().toISOString();
            }

            await this.db.collection('totens').doc(totemId).update(updateData);

            const historicoRef = this.db.collection('totens').doc(totemId);
            const doc = await historicoRef.get();
            const historico = doc.data().firmware?.historico || [];
            
            const historicoAtualizado = historico.map(item => {
                if (item.versao === versao) {
                    return { ...item, status: status, dataAtualizacao: new Date().toISOString() };
                }
                return item;
            });

            await historicoRef.update({
                'firmware.historico': historicoAtualizado
            });

            return { success: true };

        } catch (error) {
            console.error('Erro ao atualizar status OTA:', error);
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async obterHistoricoVersoes(totemId) {
        try {
            const doc = await this.db.collection('totens').doc(totemId).get();
            
            if (!doc.exists) {
                throw new Error(`Totem ${totemId} não encontrado`);
            }

            const firmware = doc.data().firmware || {};
            return {
                success: true,
                versaoAtual: firmware.atual || 'Desconhecida',
                historico: firmware.historico || []
            };

        } catch (error) {
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async criarJobMassa(totens, versao, agendamento = null, criadoPor = 'admin') {
        try {
            const jobId = `job_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
            
            const jobData = {
                id: jobId,
                status: agendamento ? 'scheduled' : 'pending',
                totens: totens,
                versao: versao,
                progresso: {
                    total: totens.length,
                    concluidos: 0,
                    falhas: 0,
                    emAndamento: 0
                },
                agendamento: agendamento,
                criadoEm: new Date().toISOString(),
                criadoPor: criadoPor,
                resultados: []
            };

            await this.db.collection('ota_jobs').doc(jobId).set(jobData);

            return {
                success: true,
                jobId: jobId
            };

        } catch (error) {
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async atualizarProgressoJob(jobId, totemId, status, mensagem = '') {
        try {
            const jobRef = this.db.collection('ota_jobs').doc(jobId);
            const jobDoc = await jobRef.get();

            if (!jobDoc.exists) {
                throw new Error(`Job ${jobId} não encontrado`);
            }

            const jobData = jobDoc.data();
            const progresso = jobData.progresso;

            if (status === 'success') {
                progresso.concluidos++;
            } else if (status === 'failed') {
                progresso.falhas++;
            }

            progresso.emAndamento = progresso.total - progresso.concluidos - progresso.falhas;

            const resultado = {
                totemId: totemId,
                status: status,
                mensagem: mensagem,
                timestamp: new Date().toISOString()
            };

            const jobStatus = progresso.concluidos + progresso.falhas >= progresso.total 
                ? 'completed' 
                : 'in_progress';

            await jobRef.update({
                progresso: progresso,
                status: jobStatus,
                resultados: admin.firestore.FieldValue.arrayUnion(resultado),
                atualizadoEm: new Date().toISOString()
            });

            return { success: true };

        } catch (error) {
            console.error('Erro ao atualizar progresso do job:', error);
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async obterStatusJob(jobId) {
        try {
            const doc = await this.db.collection('ota_jobs').doc(jobId).get();
            
            if (!doc.exists) {
                throw new Error(`Job ${jobId} não encontrado`);
            }

            return {
                success: true,
                job: doc.data()
            };

        } catch (error) {
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async registrarAuditoria(acao, usuario, detalhes) {
        try {
            await this.db.collection('ota_auditoria').add({
                acao: acao,
                usuario: usuario,
                detalhes: detalhes,
                timestamp: new Date().toISOString(),
                ip: detalhes.ip || 'unknown'
            });

            return { success: true };

        } catch (error) {
            console.error('Erro ao registrar auditoria:', error);
            return { success: false };
        }
    }
}

module.exports = FirebaseOTAService;
