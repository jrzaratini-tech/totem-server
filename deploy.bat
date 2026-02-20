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

git add .
git commit -m "%msg%"
git push

echo.
echo ===============================
echo   Deploy concluido!
echo ===============================
pause