let firmwareAtual = null;
let totensData = [];

document.addEventListener('DOMContentLoaded', () => {
    inicializarUpload();
    carregarTotens();
    inicializarMonitoramentoJobs();
});

function inicializarUpload() {
    const uploadArea = document.getElementById('uploadArea');
    const fileInput = document.getElementById('firmwareFile');

    uploadArea.addEventListener('click', () => fileInput.click());

    uploadArea.addEventListener('dragover', (e) => {
        e.preventDefault();
        uploadArea.classList.add('dragging');
    });

    uploadArea.addEventListener('dragleave', () => {
        uploadArea.classList.remove('dragging');
    });

    uploadArea.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadArea.classList.remove('dragging');
        
        const files = e.dataTransfer.files;
        if (files.length > 0) {
            fileInput.files = files;
            handleFileUpload(files[0]);
        }
    });

    fileInput.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
            handleFileUpload(e.target.files[0]);
        }
    });
}

async function handleFileUpload(file) {
    if (!file.name.endsWith('.bin')) {
        mostrarAlerta('Apenas arquivos .bin são permitidos', 'error');
        return;
    }

    const formData = new FormData();
    formData.append('firmware', file);
    
    const manualVersion = document.getElementById('manualVersion')?.value;
    if (manualVersion) {
        formData.append('versao', manualVersion);
    }

    try {
        mostrarAlerta('Validando firmware...', 'info');

        const response = await fetch('/admin/ota/upload', {
            method: 'POST',
            body: formData
        });

        const resultado = await response.json();

        if (resultado.success) {
            firmwareAtual = resultado.firmware;
            mostrarInfoFirmware(resultado.firmware);
            mostrarAlerta('Firmware validado com sucesso! Selecione os totens para atualizar.', 'success');
        } else {
            mostrarAlerta(`Erro: ${resultado.erro}`, 'error');
        }
    } catch (error) {
        mostrarAlerta(`Erro ao fazer upload: ${error.message}`, 'error');
    }
}

function mostrarInfoFirmware(firmware) {
    document.getElementById('firmwareInfo').style.display = 'block';
    document.getElementById('fileName').textContent = firmware.nomeArquivo;
    document.getElementById('fileVersion').textContent = firmware.versao || 'Não detectada';
    document.getElementById('fileSize').textContent = `${firmware.tamanhoMB} MB`;
    document.getElementById('fileChecksum').textContent = firmware.checksum;
    
    // Mostrar campo de versão manual se a versão for 'custom' ou não detectada
    const versionContainer = document.getElementById('versionInputContainer');
    if (!firmware.versao || firmware.versao === 'custom' || firmware.versao.includes('firmware_')) {
        versionContainer.style.display = 'block';
    } else {
        versionContainer.style.display = 'none';
    }
}

async function carregarTotens() {
    try {
        const response = await fetch('/admin/totens');
        const data = await response.json();

        if (data.success) {
            totensData = data.totens || [];
            renderizarTotens(totensData);
        } else {
            mostrarAlerta('Erro ao carregar totens', 'error');
        }
    } catch (error) {
        mostrarAlerta(`Erro: ${error.message}`, 'error');
    }
}

function renderizarTotens(totens) {
    const tbody = document.getElementById('totensBody');
    
    if (totens.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" style="text-align: center;">Nenhum totem encontrado</td></tr>';
        return;
    }

    tbody.innerHTML = totens.map(totem => {
        const firmware = totem.firmware || {};
        const versaoAtual = firmware.atual || 'Desconhecida';
        let status = firmware.status || 'offline';
        
        // Normalizar status: se não for 'updating' ou 'offline', considerar como 'online'
        if (status !== 'updating' && status !== 'offline' && status !== 'failed') {
            status = 'online';
        }
        
        const ultimaAtualizacao = firmware.ultimaAtualizacao 
            ? new Date(firmware.ultimaAtualizacao).toLocaleString('pt-BR')
            : 'Nunca';

        return `
            <tr>
                <td><input type="checkbox" class="totem-checkbox" value="${totem.id}"></td>
                <td>${totem.id}</td>
                <td>${versaoAtual}</td>
                <td><span class="status-badge status-${status}">${status.toUpperCase()}</span></td>
                <td>${ultimaAtualizacao}</td>
                <td>
                    <button class="btn btn-ota" onclick="abrirModalIndividual('${totem.id}')">Atualizar</button>
                    <button class="btn btn-info" onclick="mostrarHistorico('${totem.id}')">Histórico</button>
                </td>
            </tr>
        `;
    }).join('');
}

function selecionarTodos() {
    const selectAll = document.getElementById('selectAll');
    const checkboxes = document.querySelectorAll('.totem-checkbox');
    checkboxes.forEach(cb => cb.checked = selectAll.checked);
}

function abrirModalIndividual(totemId) {
    if (!firmwareAtual) {
        mostrarAlerta('Faça upload de um firmware primeiro', 'error');
        return;
    }

    const modal = document.getElementById('modalIndividual');
    const body = document.getElementById('modalIndividualBody');

    body.innerHTML = `
        <p><strong>Totem ID:</strong> ${totemId}</p>
        <p><strong>Versão do Firmware:</strong> ${firmwareAtual.versao}</p>
        <p><strong>Tamanho:</strong> ${firmwareAtual.tamanhoMB} MB</p>
        <div style="margin-top: 20px;">
            <button class="btn btn-ota" onclick="executarOTAIndividual('${totemId}')">Confirmar Atualização</button>
            <button class="btn btn-voltar" onclick="fecharModal('modalIndividual')">Cancelar</button>
        </div>
    `;

    modal.classList.add('active');
}

async function executarOTAIndividual(totemId) {
    if (!firmwareAtual) {
        mostrarAlerta('Firmware não carregado', 'error');
        return;
    }

    try {
        mostrarAlerta('Iniciando atualização...', 'info');
        fecharModal('modalIndividual');

        const response = await fetch(`/admin/ota/individual/${totemId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                versao: firmwareAtual.versao,
                firmwarePath: firmwareAtual.path,
                checksum: firmwareAtual.checksum,
                tamanho: firmwareAtual.tamanho
            })
        });

        const resultado = await response.json();

        if (resultado.success) {
            mostrarAlerta(`OTA iniciado com sucesso para ${totemId}`, 'success');
            carregarTotens();
        } else {
            mostrarAlerta(`Erro: ${resultado.erro}`, 'error');
        }
    } catch (error) {
        mostrarAlerta(`Erro: ${error.message}`, 'error');
    }
}

function abrirModalMassa() {
    if (!firmwareAtual) {
        mostrarAlerta('Faça upload de um firmware primeiro', 'error');
        return;
    }

    const modal = document.getElementById('modalMassa');
    const checkboxList = document.getElementById('totensCheckboxList');

    checkboxList.innerHTML = totensData.map(totem => `
        <div class="checkbox-item">
            <input type="checkbox" id="massa_${totem.id}" value="${totem.id}">
            <label for="massa_${totem.id}">${totem.id} (${totem.firmware?.atual || 'Desconhecida'})</label>
        </div>
    `).join('');

    modal.classList.add('active');
}

function toggleAgendamento() {
    const checkbox = document.getElementById('agendarCheckbox');
    const group = document.getElementById('agendamentoGroup');
    group.style.display = checkbox.checked ? 'block' : 'none';
}

async function executarOTAMassa() {
    const checkboxes = document.querySelectorAll('#totensCheckboxList input[type="checkbox"]:checked');
    const totens = Array.from(checkboxes).map(cb => cb.value);

    if (totens.length === 0) {
        mostrarAlerta('Selecione pelo menos um totem', 'error');
        return;
    }

    const agendarCheckbox = document.getElementById('agendarCheckbox');
    const agendamentoInput = document.getElementById('agendamentoInput');
    const agendamento = agendarCheckbox.checked ? agendamentoInput.value : null;

    if (agendarCheckbox.checked && !agendamento) {
        mostrarAlerta('Selecione uma data e hora para agendamento', 'error');
        return;
    }

    try {
        mostrarAlerta(`Iniciando atualização em massa para ${totens.length} totens...`, 'info');
        fecharModal('modalMassa');

        const response = await fetch('/admin/ota/massa', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                totens,
                versao: firmwareAtual.versao,
                firmwarePath: firmwareAtual.path,
                checksum: firmwareAtual.checksum,
                tamanho: firmwareAtual.tamanho,
                agendamento: agendamento ? new Date(agendamento).toISOString() : null
            })
        });

        const resultado = await response.json();

        if (resultado.success) {
            if (agendamento) {
                mostrarAlerta(`Job agendado com sucesso! ID: ${resultado.jobId}`, 'success');
            } else {
                mostrarAlerta(`Job criado com sucesso! ID: ${resultado.jobId}`, 'success');
                monitorarJob(resultado.jobId);
            }
        } else {
            mostrarAlerta(`Erro: ${resultado.erro}`, 'error');
        }
    } catch (error) {
        mostrarAlerta(`Erro: ${error.message}`, 'error');
    }
}

async function mostrarHistorico(totemId) {
    try {
        const response = await fetch(`/admin/ota/versoes/${totemId}`);
        const resultado = await response.json();

        if (resultado.success) {
            const modal = document.getElementById('modalHistorico');
            const body = document.getElementById('historicoBody');

            body.innerHTML = `
                <p><strong>Versão Atual:</strong> ${resultado.versaoAtual}</p>
                <h3 style="margin-top: 20px; margin-bottom: 10px;">Histórico:</h3>
                ${resultado.historico.length === 0 
                    ? '<p style="color: #aaa;">Nenhum histórico disponível</p>'
                    : resultado.historico.map(v => `
                        <div class="firmware-card">
                            <div>
                                <strong>${v.versao}</strong><br>
                                <small>${new Date(v.data).toLocaleString('pt-BR')}</small><br>
                                <span class="status-badge status-${v.status}">${v.status}</span>
                            </div>
                            <div>
                                ${v.versao !== resultado.versaoAtual 
                                    ? `<button class="btn btn-info" onclick="rollback('${totemId}', '${v.versao}')">Rollback</button>`
                                    : '<span style="color: #28a745;">✓ Atual</span>'
                                }
                            </div>
                        </div>
                    `).join('')
                }
            `;

            modal.classList.add('active');
        } else {
            mostrarAlerta(`Erro: ${resultado.erro}`, 'error');
        }
    } catch (error) {
        mostrarAlerta(`Erro: ${error.message}`, 'error');
    }
}

async function rollback(totemId, versao) {
    if (!confirm(`Deseja realmente fazer rollback para a versão ${versao}?`)) {
        return;
    }

    try {
        mostrarAlerta('Iniciando rollback...', 'info');
        fecharModal('modalHistorico');

        const response = await fetch(`/admin/ota/rollback/${totemId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ versao })
        });

        const resultado = await response.json();

        if (resultado.success) {
            mostrarAlerta(`Rollback iniciado para ${totemId}`, 'success');
            carregarTotens();
        } else {
            mostrarAlerta(`Erro: ${resultado.erro}`, 'error');
        }
    } catch (error) {
        mostrarAlerta(`Erro: ${error.message}`, 'error');
    }
}

async function monitorarJob(jobId) {
    const container = document.getElementById('jobsContainer');
    
    const jobCard = document.createElement('div');
    jobCard.className = 'job-card';
    jobCard.id = `job_${jobId}`;
    jobCard.innerHTML = `
        <div class="job-header">
            <h3>Job: ${jobId}</h3>
            <span class="status-badge status-updating">EM ANDAMENTO</span>
        </div>
        <div class="progress-bar-ota">
            <div class="progress-bar-fill" id="progress_${jobId}" style="width: 0%">0%</div>
        </div>
        <div class="job-stats" id="stats_${jobId}">
            <div class="stat-box">
                <div class="stat-number" id="total_${jobId}">0</div>
                <div class="stat-label">Total</div>
            </div>
            <div class="stat-box">
                <div class="stat-number" id="concluidos_${jobId}">0</div>
                <div class="stat-label">Concluídos</div>
            </div>
            <div class="stat-box">
                <div class="stat-number" id="falhas_${jobId}">0</div>
                <div class="stat-label">Falhas</div>
            </div>
        </div>
    `;

    container.innerHTML = '';
    container.appendChild(jobCard);

    const interval = setInterval(async () => {
        try {
            const response = await fetch(`/admin/ota/job/${jobId}`);
            const resultado = await response.json();

            if (resultado.success) {
                const job = resultado.job;
                const progresso = job.progresso;
                const porcentagem = Math.round((progresso.concluidos + progresso.falhas) / progresso.total * 100);

                document.getElementById(`progress_${jobId}`).style.width = `${porcentagem}%`;
                document.getElementById(`progress_${jobId}`).textContent = `${porcentagem}%`;
                document.getElementById(`total_${jobId}`).textContent = progresso.total;
                document.getElementById(`concluidos_${jobId}`).textContent = progresso.concluidos;
                document.getElementById(`falhas_${jobId}`).textContent = progresso.falhas;

                if (job.status === 'completed') {
                    clearInterval(interval);
                    jobCard.querySelector('.status-badge').className = 'status-badge status-online';
                    jobCard.querySelector('.status-badge').textContent = 'CONCLUÍDO';
                    mostrarAlerta(`Job ${jobId} concluído! ${progresso.concluidos} sucessos, ${progresso.falhas} falhas`, 'success');
                    carregarTotens();
                }
            }
        } catch (error) {
            console.error('Erro ao monitorar job:', error);
        }
    }, 3000);
}

function inicializarMonitoramentoJobs() {
    setInterval(async () => {
        try {
            const response = await fetch('/admin/ota/jobs/ativos');
            const resultado = await response.json();

            if (resultado.success && resultado.jobs.length > 0) {
                resultado.jobs.forEach(job => {
                    if (!document.getElementById(`job_${job.jobId}`)) {
                        monitorarJob(job.jobId);
                    }
                });
            }
        } catch (error) {
            console.error('Erro ao verificar jobs ativos:', error);
        }
    }, 10000);
}

function fecharModal(modalId) {
    document.getElementById(modalId).classList.remove('active');
}

function mostrarAlerta(mensagem, tipo) {
    const container = document.getElementById('alertContainer');
    const alert = document.createElement('div');
    alert.className = `alert alert-${tipo}`;
    alert.textContent = mensagem;
    
    container.innerHTML = '';
    container.appendChild(alert);

    setTimeout(() => {
        alert.remove();
    }, 5000);
}
