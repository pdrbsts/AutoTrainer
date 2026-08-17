@echo off
echo ===================================================
echo               AutoTrainer Launcher
echo ===================================================
echo.
echo 1. Iniciar AutoTrainer
echo 2. Iniciar MockGame (Jogo de Teste)
echo 3. Iniciar Ambos (AutoTrainer + MockGame)
echo 4. Recompilar com build.bat
echo 5. Sair
echo.
set /p opt="Escolha uma opcao [1-5]: "

if "%opt%"=="1" (
    start "" "%~dp0AutoTrainer.exe"
) else if "%opt%"=="2" (
    start "" "%~dp0MockGame.exe"
) else if "%opt%"=="3" (
    start "" "%~dp0MockGame.exe"
    timeout /t 1 /nobreak >nul
    start "" "%~dp0AutoTrainer.exe"
) else if "%opt%"=="4" (
    call "%~dp0build.bat"
    pause
)
