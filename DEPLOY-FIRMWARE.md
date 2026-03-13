# 🚀 Como Atualizar o Firmware Remotamente

## 📋 Scripts Disponíveis

### `deploy.bat` - Deploy Normal
Atualiza **apenas o servidor/páginas web**, sem mexer no firmware do ESP32.

```bash
deploy.bat
```

### `deploy-firmware.bat` - Deploy de Firmware OTA
Atualiza **apenas o firmware do ESP32** remotamente via MQTT.

```bash
deploy-firmware.bat
```

---

## 🔧 Como Usar o Deploy de Firmware

### Passo a Passo

1. **Execute o script:**
   ```bash
   deploy-firmware.bat
   ```

2. **O script vai:**
   - ✅ Compilar o firmware automaticamente
   - ✅ Copiar o `.bin` para a pasta pública do servidor
   - ✅ Habilitar o auto-publish no `firmware-config.json`
   - ✅ Fazer commit e push para o GitHub
   - ✅ Aguardar o Render fazer deploy (~3 min)
   - ✅ Servidor publica automaticamente no MQTT
   - ✅ ESP32 recebe e atualiza sozinho

3. **Pronto!** O ESP32 vai atualizar automaticamente.

---

## 📝 Quando Usar Cada Script

| Situação | Script a Usar |
|----------|---------------|
| Atualizar página web, servidor, rotas | `deploy.bat` |
| Atualizar configuração de LEDs (200+5) | `deploy-firmware.bat` |
| Corrigir bugs no firmware | `deploy-firmware.bat` |
| Adicionar novos efeitos de LED | `deploy-firmware.bat` |
| Mudar configurações de áudio | `deploy-firmware.bat` |

---

## ⚙️ Configuração Atual

**Arquivo:** `server/firmware-config.json`

```json
{
  "autoPublish": false,
  "firmwareVersion": "v4.1.0-led205",
  "firmwareUrl": "https://totem-server.onrender.com/firmware/firmware-v4.1.0-led205.bin",
  "totemId": "printpixel"
}
```

- **autoPublish**: Controlado automaticamente pelo script
- **firmwareVersion**: Versão atual do firmware
- **firmwareUrl**: URL pública do arquivo .bin
- **totemId**: ID do totem que vai receber a atualização

---

## 🔍 Monitorar Atualização

Após executar `deploy-firmware.bat`:

1. Aguarde ~3 minutos para o Render fazer deploy
2. O ESP32 vai mostrar no Serial Monitor:
   ```
   [OTA] Starting firmware download...
   [OTA] Progress: 50% (775314/1550629 bytes)
   [OTA] SUCCESS! Firmware updated, restarting...
   ```
3. ESP32 reinicia automaticamente com a nova versão

---

## 🛠️ Troubleshooting

### Firmware não atualiza?

1. Verifique se o Render terminou o deploy
2. Verifique os logs do servidor no Render
3. Verifique se o ESP32 está conectado ao MQTT
4. Verifique o Serial Monitor do ESP32

### Erro ao compilar?

```bash
cd firmware
pio run
```

Se der erro, corrija o código e tente novamente.

---

## 📦 Estrutura de Arquivos

```
totem-server/
├── deploy.bat                    # Deploy normal (servidor/páginas)
├── deploy-firmware.bat           # Deploy de firmware OTA
├── server/
│   ├── firmware-config.json      # Configuração de auto-publish
│   └── public/
│       └── firmware/
│           └── firmware-v4.1.0-led205.bin  # Firmware compilado
└── firmware/
    └── .pio/build/esp32-s3-devkitc-1/
        └── firmware.bin          # Firmware compilado (fonte)
```

---

## ✅ Checklist de Atualização

- [ ] Fazer alterações no código do firmware
- [ ] Executar `deploy-firmware.bat`
- [ ] Aguardar compilação
- [ ] Digitar mensagem de commit
- [ ] Aguardar deploy no Render (~3 min)
- [ ] ESP32 atualiza automaticamente
- [ ] Verificar no Serial Monitor se atualizou

---

**Versão Atual:** v4.1.0-led205 (200 LEDs principais + 5 LEDs batimento cardíaco)
