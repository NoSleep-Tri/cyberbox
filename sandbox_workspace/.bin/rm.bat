@echo off
setlocal enabledelayedexpansion
set "T=%*"
set "T=!T:-rf =!"
set "T=!T:-f =!"
set "T=!T:/=\!"
if exist "!T!" del /f /q "!T!" 2>nul || rd /s /q "!T!" 2>nul
exit /b 0
