@echo off
echo ===============================
echo  Atualizando Totem Server...
echo ===============================

git add .
git commit -m "Atualizacao automatica %date% %time%"
git push

echo.
echo ===============================
echo  Deploy enviado para o GitHub!
echo ===============================
pause