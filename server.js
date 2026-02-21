const express = require("express");
const mqtt = require("mqtt");
const session = require("express-session");
const bodyParser = require("body-parser");
const fs = require("fs-extra");
const path = require("path");

const app = express();
const PORT = process.env.PORT || 3000;

// Configurações
const SENHA_ADMIN = "159268";
const PASTA_CLIENTES = path.join(__dirname, "clientes");

// Garante que a pasta clientes existe
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

app.get("/totem/:id", (req, res) => {
    const id = req.params.id;
    const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);

    if (!fs.existsSync(arquivoCliente)) {
        return res.status(404).send("❌ Totem não encontrado");
    }

    const instagramLink = fs.readFileSync(arquivoCliente, "utf8").trim();

    const topic = `totem/${id}`;
    console.log(`📢 Publicando play em: ${topic}`);
    mqttClient.publish(topic, "play");

    res.redirect(instagramLink);
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
        const arquivos = await fs.readdir(PASTA_CLIENTES);
        const clientes = [];

        for (const arquivo of arquivos) {
            if (arquivo.endsWith(".txt")) {
                const id = arquivo.replace(".txt", "");
                const link = await fs.readFile(path.join(PASTA_CLIENTES, arquivo), "utf8");
                clientes.push({ id, link: link.trim() });
            }
        }

        clientes.sort((a, b) => a.id.localeCompare(b.id));

        const baseUrl = `${req.protocol}://${req.get('host')}`;

        let html = await fs.readFile(path.join(__dirname, "views", "admin.html"), "utf8");
        
        let linhas = "";
        for (const cliente of clientes) {
            const qrLink = `${baseUrl}/totem/${cliente.id}`;
            linhas += `
                <tr>
                    <td><strong>${cliente.id}</strong></td>
                    <td>
                        <a href="${cliente.link}" target="_blank" style="color: #17a2b8;">${cliente.link}</a>
                    </td>
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
                    <td colspan="4" class="vazio">📭 Nenhum totem cadastrado. Clique em "Novo Cliente" para começar.</td>
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
    const { id, link } = req.body;
    
    if (!id || !link) {
        return res.send("❌ ID e Link são obrigatórios");
    }

    if (!link.includes("instagram.com")) {
        return res.send("❌ O link precisa ser do Instagram");
    }

    const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);

    if (fs.existsSync(arquivoCliente)) {
        return res.send("❌ Este ID já está cadastrado");
    }

    await fs.writeFile(arquivoCliente, link.trim());
    
    res.redirect("/admin/dashboard?msg=sucesso");
});

app.get("/admin/editar/:id", verificarAuth, async (req, res) => {
    const id = req.params.id;
    const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);

    if (!fs.existsSync(arquivoCliente)) {
        return res.send("❌ Cliente não encontrado");
    }

    const link = await fs.readFile(arquivoCliente, "utf8");
    
    let html = await fs.readFile(path.join(__dirname, "views", "editar.html"), "utf8");
    html = html.replace("{{ID}}", id);
    html = html.replace("{{LINK}}", link.trim());
    
    res.send(html);
});

app.post("/admin/editar/:id", verificarAuth, async (req, res) => {
    const id = req.params.id;
    const { link } = req.body;
    
    if (!link) {
        return res.send("❌ Link é obrigatório");
    }

    const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);

    if (!fs.existsSync(arquivoCliente)) {
        return res.send("❌ Cliente não encontrado");
    }

    await fs.writeFile(arquivoCliente, link.trim());
    res.redirect("/admin/dashboard");
});

app.get("/admin/excluir/:id", verificarAuth, async (req, res) => {
    const id = req.params.id;
    const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);

    if (fs.existsSync(arquivoCliente)) {
        await fs.remove(arquivoCliente);
    }
    
    res.redirect("/admin/dashboard");
});

// ==============================================
// INICIA O SERVIDOR
// ==============================================
app.listen(PORT, () => {
    console.log(`🚀 Servidor rodando na porta ${PORT}`);
    console.log(`📊 Admin: http://localhost:${PORT}/admin/login`);
});