// ============================================
// Middleware de Autenticação
// TOTEM INTERATIVO IoT v4.0
// ============================================

const admin = require('firebase-admin');

// Middleware para verificar se admin está autenticado
function adminAuth(req, res, next) {
    if (req.session && req.session.adminAutenticado) {
        next();
    } else {
        res.redirect('/admin/login');
    }
}

// Middleware para verificar se cliente está autenticado
function clienteAuth(req, res, next) {
    if (req.session && req.session.clienteTotemId) {
        // Verificar se o ID da URL corresponde ao da sessão
        const idUrl = req.params.id || req.body.totemId;
        if (idUrl && idUrl === req.session.clienteTotemId) {
            next();
        } else {
            res.status(403).send('Acesso negado: ID do totem não corresponde');
        }
    } else {
        res.redirect('/cliente/login');
    }
}

// Middleware para verificar acesso direto do cliente (SEM LOGIN)
async function verificarAcessoCliente(req, res, next) {
    const { id } = req.params;
    
    if (!id) {
        return res.status(400).send('ID do totem não fornecido');
    }

    try {
        // Buscar totem no Firestore
        const totemDoc = await admin.firestore()
            .collection('totens')
            .doc(id)
            .get();

        if (!totemDoc.exists) {
            return res.status(404).send('Totem não encontrado');
        }

        const totemData = totemDoc.data();
        
        // Verificar se está expirado
        if (totemData.dataExpiracao) {
            const hoje = new Date();
            hoje.setHours(0, 0, 0, 0);
            const expiracao = new Date(totemData.dataExpiracao + 'T00:00:00');
            
            if (expiracao < hoje) {
                return res.redirect('/expirado');
            }
        }

        // Adicionar dados do totem à requisição
        req.totem = {
            id,
            ...totemData
        };
        
        next();
    } catch (error) {
        console.error('Erro ao verificar totem:', error);
        res.status(500).send('Erro interno do servidor');
    }
}

// Middleware para logging de requisições
function logger(req, res, next) {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.url} - IP: ${req.ip}`);
    next();
}

module.exports = {
    adminAuth,
    clienteAuth,
    verificarAcessoCliente,
    logger
};