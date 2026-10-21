@echo off
setlocal enabledelayedexpansion
set "P=%*"
if "%P%"=="" exit /b 0
set "P=!P:/=\!"
type !P! 2>nul || python -c "import sys; [print(open(f, 'r', errors='ignore').read(), end='') for f in sys.argv[1:]]" %* 2>nul
