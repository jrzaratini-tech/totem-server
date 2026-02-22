const express = require("express");
const mqtt = require("mqtt");
const session = require("express-session");
const bodyParser = require("body-parser");
const fs = require("fs-extra");
const path = require("path");
const admin = require("firebase-admin");

const app = express();
const PORT = process.env.PORT || 3000;

// Configurações
const SENHA_ADMIN = "159268";
const PASTA_CLIENTES = path.join(__dirname, "clientes");

// 🔥 Inicializa Firebase
let serviceAccount;
try {
    // Tenta carregar do arquivo local (desenvolvimento)
    serviceAccount = require("./firebase-credentials.json");
} catch (e) {
    // Se não existir, tenta pegar da variável de ambiente (produção - Render)
    if (process.env.FIREBASE_CREDENTIALS) {
        serviceAccount = JSON.parse(process.env.FIREBASE_CREDENTIALS);
    } else {
        console.error("❌ Credenciais do Firebase não encontradas!");
        process.exit(1);
    }
}

admin.initializeApp({
    credential: admin.credential.cert(serviceAccount)
});

const db = admin.firestore();
console.log("✅ Conectado ao Firebase");

// Garante que a pasta clientes existe (para compatibilidade temporária)
fs.ensureDirSync(PASTA_CLIENTES);

// Middlewares
app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());
app.use(session({
    secret: "totem-secret-key",
    resave: false,
    saveUninitialized: true
}));

// Servir arquivos estáticos
app.use(express.static("public"));

// 🔹 Conecta no broker MQTT
const mqttClient = mqtt.connect("mqtt://broker.hivemq.com:1883");

mqttClient.on("connect", () => {
    console.log("✅ Conectado ao MQTT");
});

mqttClient.on("error", (err) => {
    console.error("❌ Erro MQTT:", err);
});

// ==============================================
// ROTAS PÚBLICAS (TOTENS)
// ==============================================

app.get("/", (req, res) => {
    res.redirect("/admin/login");
});

app.get("/totem/:id", async (req, res) => {
    const id = req.params.id;
    
    try {
        // 🔥 Busca no Firebase
        const docRef = db.collection("totens").doc(id);
        const doc = await docRef.get();

        if (!doc.exists) {
            return res.status(404).send("❌ Totem não encontrado");
        }

        const dados = doc.data();
        const instagramLink = dados.link;
        const dataExpiracao = dados.dataExpiracao;
        
        // 📅 Verifica data de expiração
        const hoje = new Date();
        const expiracao = new Date(dataExpiracao + "T23:59:59"); // Fim do dia
        
        if (hoje > expiracao) {
            console.log(`⛔ Totem ${id} expirado em ${dataExpiracao}`);
            return res.redirect("/expirado");
        }

        // ✅ Totem ativo - publica MQTT
        const topic = `totem/${id}`;
        console.log(`📢 Publicando play em: ${topic}`);
        mqttClient.publish(topic, "play");

        res.redirect(instagramLink);
        
    } catch (error) {
        console.error("Erro ao acessar Firebase:", error);
        res.status(500).send("Erro interno do servidor");
    }
});

app.get("/expirado", (req, res) => {
    res.sendFile(path.join(__dirname, "views", "expirado.html"));
});

// ==============================================
// ROTAS ADMIN (PROTEGIDAS)
// ==============================================

function verificarAuth(req, res, next) {
    if (req.session.autenticado) {
        next();
    } else {
        res.redirect("/admin/login");
    }
}

app.get("/admin/login", (req, res) => {
    res.sendFile(path.join(__dirname, "views", "login.html"));
});

app.post("/admin/login", (req, res) => {
    const { senha } = req.body;
    
    if (senha === SENHA_ADMIN) {
        req.session.autenticado = true;
        res.redirect("/admin/dashboard");
    } else {
        res.sendFile(path.join(__dirname, "views", "mensagem.html"));
    }
});

app.get("/admin/logout", (req, res) => {
    req.session.destroy();
    res.redirect("/admin/login");
});

app.get("/admin/dashboard", verificarAuth, async (req, res) => {
    try {
        // 🔥 Busca todos os totens do Firebase
        const snapshot = await db.collection("totens").get();
        const clientes = [];
        
        snapshot.forEach(doc => {
            clientes.push({
                id: doc.id,
                ...doc.data()
            });
        });

        // Ordena por ID
        clientes.sort((a, b) => a.id.localeCompare(b.id));

        const baseUrl = `${req.protocol}://${req.get('host')}`;
        const hoje = new Date();

        let html = await fs.readFile(path.join(__dirname, "views", "admin.html"), "utf8");
        
        let linhas = "";
        for (const cliente of clientes) {
            const qrLink = `${baseUrl}/totem/${cliente.id}`;
            
            // Verifica status
            const expiracao = new Date(cliente.dataExpiracao + "T23:59:59");
            const ativo = hoje <= expiracao;
            const statusClass = ativo ? "status-ativo" : "status-bloqueado";
            const statusText = ativo ? "✅ Ativo" : "❌ Bloqueado";
            
            // Formata data
            const dataExp = cliente.dataExpiracao.split("-").reverse().join("/");
            
            linhas += `
                <tr>
                    <td><strong>${cliente.id}</strong></td>
                    <td>
                        <a href="${cliente.link}" target="_blank" style="color: #17a2b8;">${cliente.link}</a>
                    </td>
                    <td>${dataExp}</td>
                    <td class="${statusClass}">${statusText}</td>
                    <td>
                        <div class="qr-link">${qrLink}</div>
                    </td>
                    <td>
                        <a href="/admin/editar/${cliente.id}" class="btn-editar">✏️ Editar</a>
                        <button onclick="copiarLink('${cliente.id}')" class="btn-copiar">📋 Copiar QR Link</button>
                        <a href="/admin/excluir/${cliente.id}" class="btn-excluir" onclick="return confirm('Tem certeza que deseja excluir?')">🗑️ Excluir</a>
                    </td>
                </tr>
            `;
        }

        if (clientes.length === 0) {
            linhas = `
                <tr>
                    <td colspan="6" class="vazio">📭 Nenhum totem cadastrado. Clique em "Novo Cliente" para começar.</td>
                </tr>
            `;
        }

        html = html.replace("{{LINHAS_TABELA}}", linhas);
        res.send(html);
        
    } catch (error) {
        console.error(error);
        res.status(500).send("Erro ao carregar dashboard");
    }
});

app.get("/admin/novo", verificarAuth, (req, res) => {
    res.sendFile(path.join(__dirname, "views", "novo.html"));
});

app.post("/admin/novo", verificarAuth, async (req, res) => {
    const { id, link, dataExpiracao } = req.body;
    
    if (!id || !link || !dataExpiracao) {
        return res.send("❌ ID, Link e Data de Expiração são obrigatórios");
    }

    if (!link.includes("instagram.com")) {
        return res.send("❌ O link precisa ser do Instagram");
    }

    // Valida data
    const hoje = new Date();
    const expiracao = new Date(dataExpiracao + "T23:59:59");
    
    if (expiracao <= hoje) {
        return res.send("❌ A data de expiração deve ser futura");
    }

    try {
        // 🔥 Salva no Firebase
        const docRef = db.collection("totens").doc(id);
        const doc = await docRef.get();
        
        if (doc.exists) {
            return res.send("❌ Este ID já está cadastrado");
        }

        await docRef.set({
            link: link.trim(),
            dataExpiracao: dataExpiracao,
            criadoEm: admin.firestore.FieldValue.serverTimestamp(),
            status: "ativo"
        });

        // Mantém compatibilidade com arquivo .txt (opcional)
        const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);
        await fs.writeFile(arquivoCliente, link.trim());
        
        res.redirect("/admin/dashboard?msg=sucesso");
        
    } catch (error) {
        console.error(error);
        res.send("❌ Erro ao salvar no Firebase");
    }
});

app.get("/admin/editar/:id", verificarAuth, async (req, res) => {
    const id = req.params.id;
    
    try {
        // 🔥 Busca no Firebase
        const docRef = db.collection("totens").doc(id);
        const doc = await docRef.get();

        if (!doc.exists) {
            return res.send("❌ Cliente não encontrado");
        }

        const dados = doc.data();
        
        let html = await fs.readFile(path.join(__dirname, "views", "editar.html"), "utf8");
        html = html.replace("{{ID}}", id);
        html = html.replace("{{LINK}}", dados.link);
        html = html.replace("{{DATA_EXPIRACAO}}", dados.dataExpiracao);
        
        res.send(html);
        
    } catch (error) {
        console.error(error);
        res.send("❌ Erro ao buscar cliente");
    }
});

app.post("/admin/editar/:id", verificarAuth, async (req, res) => {
    const id = req.params.id;
    const { link, dataExpiracao } = req.body;
    
    if (!link || !dataExpiracao) {
        return res.send("❌ Link e Data de Expiração são obrigatórios");
    }

    try {
        // 🔥 Atualiza no Firebase
        const docRef = db.collection("totens").doc(id);
        
        await docRef.update({
            link: link.trim(),
            dataExpiracao: dataExpiracao,
            ultimaRenovacao: admin.firestore.FieldValue.serverTimestamp()
        });

        // Mantém compatibilidade com arquivo .txt
        const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);
        await fs.writeFile(arquivoCliente, link.trim());
        
        res.redirect("/admin/dashboard");
        
    } catch (error) {
        console.error(error);
        res.send("❌ Erro ao atualizar no Firebase");
    }
});

app.get("/admin/excluir/:id", verificarAuth, async (req, res) => {
    const id = req.params.id;
    
    try {
        // 🔥 Remove do Firebase
        await db.collection("totens").doc(id).delete();
        
        // Remove arquivo .txt se existir
        const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);
        if (fs.existsSync(arquivoCliente)) {
            await fs.remove(arquivoCliente);
        }
        
        res.redirect("/admin/dashboard");
        
    } catch (error) {
        console.error(error);
        res.send("❌ Erro ao excluir");
    }
});

// ==============================================
// INICIA O SERVIDOR
// ==============================================
app.listen(PORT, () => {
    console.log(`🚀 Servidor rodando na porta ${PORT}`);
    console.log(`📊 Admin: http://localhost:${PORT}/admin/login`);
});