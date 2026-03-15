@echo off
echo ===============================
echo   Deploy Firmware OTA
echo ===============================
echo.

REM Compilar firmware
echo [1/4] Compilando firmware...
cd firmware
call pio run
if %errorlevel% neq 0 (
    echo.
    echo ERRO: Falha ao compilar firmware
    pause
    exit /b 1
)
cd ..

REM Copiar .bin para pasta pública
echo.
echo [2/4] Copiando firmware para servidor...
copy /Y "firmware\.pio\build\esp32-s3-devkitc-1\firmware.bin" "server\public\firmware\firmware-v4.1.0-led205.bin"

REM Habilitar auto-publish no config
echo.
echo [3/4] Habilitando auto-publish...
powershell -Command "(Get-Content server\firmware-config.json) -replace '\"autoPublish\": false', '\"autoPublish\": true' | Set-Content server\firmware-config.json"

REM Fazer commit e push
echo.
echo [4/4] Enviando para GitHub/Render...
set /p msg=Digite a mensagem do commit (ou Enter para usar padrao): 

if "%msg%"=="" (
    set msg=Atualizar firmware OTA
)

git add firmware/
git add server/public/firmware/firmware-v4.1.0-led205.bin
git add server/firmware-config.json
git commit -m "%msg%"
git push

echo.
echo ===============================
echo   Firmware enviado!
echo ===============================
echo.
echo O ESP32 vai atualizar automaticamente quando o Render terminar o deploy (~3 min)
echo.
pause
