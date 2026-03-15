@echo off
echo ===============================
echo   Deploy Totem Server
echo ===============================
echo.

set /p msg=Digite a mensagem do commit: 

if "%msg%"=="" (
    echo.
    echo ERRO: Voce precisa digitar uma mensagem.
    pause
    exit
)

echo.
echo Enviando atualizacao...
echo.

git add server/
git add docs/
git add .env.example
git add .gitignore
git add package.json
git add package-lock.json
git add server.js
git add deploy.bat
git add deploy-firmware.bat
git add DEPLOY-FIRMWARE.md
git commit -m "%msg%"
git push

echo.
echo ===============================
echo   Deploy concluido!
echo ===============================
pause