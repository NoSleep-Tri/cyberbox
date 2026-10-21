@echo off
setlocal enabledelayedexpansion
set "PKG="
:arg_loop
if "%~1"=="" goto install_pkg
if /i "%~1"=="install" (
  shift
  goto get_pkg
)
shift
goto arg_loop
:get_pkg
if "%~1"=="" goto install_pkg
if not "%~1"=="-y" if not "%~1"=="--yes" if not "%~1"=="-q" (
  set "PKG=%~1"
  shift
  goto get_pkg
)
shift
goto get_pkg
:install_pkg
if "%PKG%"=="" exit /b 0
echo [CyberBox Package Bridge] Installing %PKG% on Windows...
call install.bat "%PKG%"
