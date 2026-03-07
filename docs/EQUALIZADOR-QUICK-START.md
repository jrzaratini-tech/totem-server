# 🎵 Equalizador de Áudio - Quick Start

## 🚀 Início Rápido (5 minutos)

### 1. Compilar Firmware
```bash
cd firmware
pio run -t upload
```

### 2. Reiniciar Servidor
```bash
cd ..
npm start
```

### 3. Adicionar Link no Dashboard do Cliente

Edite `server/views/cliente-dashboard.html` e adicione após o card de upload:

```html
<div class="card">
    <h2>🎵 Equalizador de Áudio</h2>
    <p style="color: #666; margin-bottom: 15px;">
        Ajuste fino das configurações de áudio do seu totem
    </p>
    <a href="/cliente/equalizer/{{ID}}" class="button button-primary" style="text-decoration: none; display: block;">
        🎛️ Abrir Equalizador
    </a>
</div>
```

### 4. Testar

1. Acesse: `http://localhost:3000/cliente/login`
2. Login com ID do totem (ex: "printpixel")
3. Clique em "🎛️ Abrir Equalizador"
4. Ajuste configurações
5. Clique em "💾 SALVAR CONFIGURAÇÃO"

---

## 📡 URLs Importantes

- **Login Cliente:** `/cliente/login`
- **Dashboard:** `/cliente/dashboard/:id`
- **Equalizador:** `/cliente/equalizer/:id`
- **API Config (GET):** `/api/cliente/audio-config/:id`
- **API Config (POST):** `/api/cliente/audio-config/:id`
- **API Test:** `/api/cliente/audio-test/:id`

---

## 🎛️ Configurações Recomendadas

### Ambiente Silencioso (Escritório)
```
Volume: 3-5
Graves: -3dB
Médios: 0dB
Agudos: +2dB
Ganho: 3dB
Curva: Linear
```

### Ambiente Ruidoso (Loja)
```
Volume: 8-10
Graves: +6dB
Médios: +3dB
Agudos: +6dB
Ganho: 15dB
Curva: Exponencial Agressiva
```

### Áudio Limpo (Padrão)
```
Volume: 8
Graves: 0dB
Médios: 0dB
Agudos: 0dB
Ganho: 15dB
Curva: Exponencial Suave
```

---

## 🔧 Troubleshooting Rápido

| Problema | Solução |
|----------|---------|
| Página não carrega | Verificar login do cliente |
| Configuração não salva | Verificar Firebase conectado |
| ESP32 não recebe | Verificar MQTT conectado |
| Som distorcido | Ativar proteção, reduzir ganho |
| Volume baixo | Ganho 15dB, curva agressiva |

---

## 📊 Tópicos MQTT

```
totem/{id}/audioConfig      → Configuração completa
totem/{id}/audioTest        → Teste de áudio
totem/{id}/status           → Status do totem
```

---

## 🎯 Checklist de Instalação

- [ ] Firmware compilado e enviado
- [ ] Servidor reiniciado
- [ ] Link adicionado no dashboard
- [ ] Teste de salvamento OK
- [ ] Teste de áudio OK
- [ ] Firestore atualizado
- [ ] MQTT funcionando

---

**Documentação completa:** `docs/EQUALIZADOR-AUDIO-INSTALACAO.md`
