// ============================================
// Middleware de Autenticação
// TOTEM INTERATIVO IoT v4.0
// ============================================

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

// Middleware para logging de requisições
function logger(req, res, next) {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.url} - IP: ${req.ip}`);
    next();
}

module.exports = {
    adminAuth,
    clienteAuth,
    logger
};