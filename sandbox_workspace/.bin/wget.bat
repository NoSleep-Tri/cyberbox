@if exist "%~dp0..\.airgap" (
  echo [Sentinel Air-Gap Guard] Error: Ket noi Internet bi CHAN - Che do Air-Gap dang bat. 1>&2
  exit /b 403
)
@echo off
if "%~1"=="" exit /b 0
curl.exe -fSL -O %*
