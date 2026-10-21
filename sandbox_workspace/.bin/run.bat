@echo off
if "%~1"=="" exit /b 0
echo [*] Launching Windows application: %*
start "" %*
