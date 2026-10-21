@echo off
title CyberBox AI Desktop
echo [*] Kiem tra may chu CyberBox AI...
tasklist /FI "IMAGENAME eq cyberbox.exe" 2>NUL | find /I /N "cyberbox.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo [OK] May chu CyberBox AI dang chay.
) else (
    echo [*] Dang khoi dong may chu CyberBox AI trong nen...
    start "" /B cyberbox.exe
    timeout /t 2 /nobreak >nul
)
echo [*] Dang mo Cua so Desktop App CyberBox AI...
start "" "msedge.exe" --app=http://127.0.0.1:8080 --window-size=1360,860
if errorlevel 1 (
    start "" "chrome.exe" --app=http://127.0.0.1:8080 --window-size=1360,860
)
