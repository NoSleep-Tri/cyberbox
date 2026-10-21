@echo off
set "U=%~1"
if "%U%"=="" set "U=http://127.0.0.1:8080"
echo [*] Opening browser: %U%
if exist "C:\Program Files\Google\Chrome\Application\chrome.exe" (
  start "" "C:\Program Files\Google\Chrome\Application\chrome.exe" "%U%"
  exit /b 0
)
if exist "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" (
  start "" "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" "%U%"
  exit /b 0
)
start "" "%U%"
