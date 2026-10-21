@echo off
setlocal enabledelayedexpansion
set "TARGET=%~1"
if "%TARGET%"=="" (
  echo [CyberBox Local Installer] Usage: install ^<local_zip_or_file^>
  exit /b 1
)
if exist "%TARGET%" (
  set "EXT=%~x1"
  if /i "!EXT!"==".zip" (
    echo [*] Giai nen goi ZIP cuc bo: %TARGET%
    tar.exe -xf "%TARGET%"
    exit /b !errorlevel!
  )
  if /i "!EXT!"==".tar" if /i "!EXT!"==".tgz" if /i "!EXT!"==".gz" (
    echo [*] Giai nen goi TAR cuc bo: %TARGET%
    tar.exe -xf "%TARGET%"
    exit /b !errorlevel!
  )
)
echo [Sentinel Guard Blocked]: Cai dat goi '%TARGET%' ra ngoai may that bi CHAN.
echo Vui long su dung file ZIP cuc bo, pip hoac bien dich C++ trong workspace.
exit /b 126
