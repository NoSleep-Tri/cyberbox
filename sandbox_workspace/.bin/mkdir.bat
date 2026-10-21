@echo off
setlocal enabledelayedexpansion
set "D=%*"
if "%D%"=="" exit /b 0
set "D=!D:-p =!"
set "D=!D:/=\!"
if not exist "!D!" md "!D!" 2>nul
exit /b 0
