const FirebaseOTAService = require('./firebaseOTA');

class JobManager {
    constructor(mqttOTAService) {
        this.mqttOTAService = mqttOTAService;
        this.firebaseOTA = new FirebaseOTAService();
        this.jobsAtivos = new Map();
        this.jobsAgendados = new Map();
    }

    async executarJobIndividual(totemId, versao, url, checksum, tamanho, usuario = 'admin') {
        try {
            console.log(`🚀 Iniciando OTA individual para ${totemId} - versão ${versao}`);

            await this.firebaseOTA.registrarAuditoria('ota_individual', usuario, {
                totemId,
                versao,
                ip: 'server'
            });

            await this.firebaseOTA.atualizarStatusOTA(totemId, 'updating', versao);

            const resultado = await this.mqttOTAService.publicarOTAIndividual(
                totemId, 
                versao, 
                url, 
                checksum, 
                tamanho
            );

            if (resultado.success) {
                console.log(`✅ OTA enviado com sucesso para ${totemId}`);
                return {
                    success: true,
                    message: `OTA iniciado para ${totemId}`,
                    totemId,
                    versao
                };
            } else {
                await this.firebaseOTA.atualizarStatusOTA(totemId, 'failed', versao);
                return {
                    success: false,
                    erro: resultado.erro,
                    totemId
                };
            }

        } catch (error) {
            console.error(`❌ Erro ao executar OTA individual para ${totemId}:`, error);
            await this.firebaseOTA.atualizarStatusOTA(totemId, 'failed', versao);
            return {
                success: false,
                erro: error.message,
                totemId
            };
        }
    }

    async executarJobMassa(jobId, totens, versao, url, checksum, tamanho, usuario = 'admin') {
        try {
            console.log(`🚀 Iniciando job em massa ${jobId} para ${totens.length} totens`);

            this.jobsAtivos.set(jobId, {
                status: 'in_progress',
                inicio: new Date(),
                totens: totens.length
            });

            await this.firebaseOTA.registrarAuditoria('ota_massa', usuario, {
                jobId,
                totens: totens.length,
                versao,
                ip: 'server'
            });

            const resultado = await this.mqttOTAService.publicarOTAMassa(
                totens,
                versao,
                url,
                checksum,
                tamanho,
                async (progresso) => {
                    const status = progresso.resultado.success ? 'success' : 'failed';
                    const mensagem = progresso.resultado.erro || 'OTA enviado com sucesso';
                    
                    await this.firebaseOTA.atualizarProgressoJob(
                        jobId,
                        progresso.totemId,
                        status,
                        mensagem
                    );

                    await this.firebaseOTA.atualizarStatusOTA(
                        progresso.totemId,
                        status === 'success' ? 'updating' : 'failed',
                        versao
                    );
                }
            );

            this.jobsAtivos.delete(jobId);

            console.log(`✅ Job ${jobId} concluído: ${resultado.sucessos} sucessos, ${resultado.falhas} falhas`);

            return {
                success: true,
                jobId,
                ...resultado
            };

        } catch (error) {
            console.error(`❌ Erro ao executar job em massa ${jobId}:`, error);
            this.jobsAtivos.delete(jobId);
            
            return {
                success: false,
                jobId,
                erro: error.message
            };
        }
    }

    async agendarJob(jobId, totens, versao, url, checksum, tamanho, dataAgendamento, usuario = 'admin') {
        try {
            const agendamento = new Date(dataAgendamento);
            const agora = new Date();

            if (agendamento <= agora) {
                return {
                    success: false,
                    erro: 'Data de agendamento deve ser no futuro'
                };
            }

            const delay = agendamento.getTime() - agora.getTime();

            const timeoutId = setTimeout(async () => {
                console.log(`⏰ Executando job agendado ${jobId}`);
                await this.executarJobMassa(jobId, totens, versao, url, checksum, tamanho, usuario);
                this.jobsAgendados.delete(jobId);
            }, delay);

            this.jobsAgendados.set(jobId, {
                timeoutId,
                agendamento,
                totens: totens.length,
                versao
            });

            console.log(`📅 Job ${jobId} agendado para ${agendamento.toISOString()}`);

            return {
                success: true,
                jobId,
                agendamento: agendamento.toISOString(),
                delay: Math.round(delay / 1000 / 60)
            };

        } catch (error) {
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async cancelarJobAgendado(jobId) {
        try {
            const job = this.jobsAgendados.get(jobId);
            
            if (!job) {
                return {
                    success: false,
                    erro: 'Job não encontrado ou já executado'
                };
            }

            clearTimeout(job.timeoutId);
            this.jobsAgendados.delete(jobId);

            console.log(`❌ Job ${jobId} cancelado`);

            return {
                success: true,
                message: `Job ${jobId} cancelado com sucesso`
            };

        } catch (error) {
            return {
                success: false,
                erro: error.message
            };
        }
    }

    async pausarJob(jobId) {
        return {
            success: false,
            erro: 'Funcionalidade de pausa não implementada nesta versão'
        };
    }

    async retomarJob(jobId) {
        return {
            success: false,
            erro: 'Funcionalidade de retomada não implementada nesta versão'
        };
    }

    obterJobsAtivos() {
        return Array.from(this.jobsAtivos.entries()).map(([id, job]) => ({
            jobId: id,
            ...job
        }));
    }

    obterJobsAgendados() {
        return Array.from(this.jobsAgendados.entries()).map(([id, job]) => ({
            jobId: id,
            agendamento: job.agendamento.toISOString(),
            totens: job.totens,
            versao: job.versao
        }));
    }
}

module.exports = JobManager;
