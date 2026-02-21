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

// Servir arquivos estáticos (CSS se quiser depois)
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

// Rota principal - redireciona para admin (opcional)
app.get("/", (req, res) => {
    res.redirect("/admin/login");
});

// Rota do totem (pública)
app.get("/totem/:id", (req, res) => {
    const id = req.params.id;
    const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);

    // Verifica se o cliente existe
    if (!fs.existsSync(arquivoCliente)) {
        return res.status(404).send("❌ Totem não encontrado");
    }

    // Lê o link do Instagram do arquivo
    const instagramLink = fs.readFileSync(arquivoCliente, "utf8").trim();

    // Publica no MQTT
    const topic = `totem/${id}`;
    console.log(`📢 Publicando play em: ${topic}`);
    mqttClient.publish(topic, "play");

    // Redireciona para o Instagram
    res.redirect(instagramLink);
});

// ==============================================
// ROTAS ADMIN (PROTEGIDAS)
// ==============================================

// Middleware de autenticação
function verificarAuth(req, res, next) {
    if (req.session.autenticado) {
        next();
    } else {
        res.redirect("/admin/login");
    }
}

// Página de login
app.get("/admin/login", (req, res) => {
    res.sendFile(path.join(__dirname, "views", "login.html"));
});

// Processar login
app.post("/admin/login", (req, res) => {
    const { senha } = req.body;
    
    if (senha === SENHA_ADMIN) {
        req.session.autenticado = true;
        res.redirect("/admin/dashboard");
    } else {
        res.sendFile(path.join(__dirname, "views", "mensagem.html"), {
            root: __dirname
        });
    }
});

// Logout
app.get("/admin/logout", (req, res) => {
    req.session.destroy();
    res.redirect("/admin/login");
});

// Dashboard admin (lista todos os clientes)
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

        // Ordena por ID
        clientes.sort((a, b) => a.id.localeCompare(b.id));

        // Envia para o template (usando HTML simples)
        let html = await fs.readFile(path.join(__dirname, "views", "admin.html"), "utf8");
        
        // Gera as linhas da tabela
        let linhas = "";
        for (const cliente of clientes) {
            linhas += `
                <tr>
                    <td>${cliente.id}</td>
                    <td>${cliente.link}</td>
                    <td>
                        <a href="/admin/editar/${cliente.id}" class="btn-editar">✏️ Editar</a>
                        <a href="/admin/excluir/${cliente.id}" class="btn-excluir" onclick="return confirm('Tem certeza?')">🗑️ Excluir</a>
                    </td>
                </tr>
            `;
        }

        html = html.replace("{{LINHAS_TABELA}}", linhas);
        res.send(html);
        
    } catch (error) {
        res.status(500).send("Erro ao carregar dashboard");
    }
});

// Página para adicionar novo cliente
app.get("/admin/novo", verificarAuth, (req, res) => {
    res.sendFile(path.join(__dirname, "views", "novo.html"));
});

// Processa novo cliente
app.post("/admin/novo", verificarAuth, async (req, res) => {
    const { id, link } = req.body;
    
    if (!id || !link) {
        return res.send("❌ ID e Link são obrigatórios");
    }

    // Valida se o link é do Instagram (básico)
    if (!link.includes("instagram.com")) {
        return res.send("❌ O link precisa ser do Instagram");
    }

    const arquivoCliente = path.join(PASTA_CLIENTES, `${id}.txt`);

    // Verifica se já existe
    if (fs.existsSync(arquivoCliente)) {
        return res.send("❌ Este ID já está cadastrado");
    }

    // Salva o arquivo
    await fs.writeFile(arquivoCliente, link.trim());
    
    res.redirect("/admin/dashboard?msg=sucesso");
});

// Página para editar cliente
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

// Processa edição
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

// Excluir cliente
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