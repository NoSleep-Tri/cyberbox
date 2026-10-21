@echo off
setlocal enabledelayedexpansion
set "ARGS="
set "OUT="
:parse_loop
if "%~1"=="" goto do_cl
if "%~1"=="-o" (
  shift
  set "OUT=/Fe:%~1.exe"
  shift
  goto parse_loop
)
if "%~1"=="-std=c++17" ( shift & goto parse_loop )
if "%~1"=="-Wall" ( shift & goto parse_loop )
if "%~1"=="-O2" ( shift & goto parse_loop )
set "ARGS=!ARGS! %1"
shift
goto parse_loop
:do_cl
@if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
)
cl /EHsc /std:c++17 !OUT! !ARGS!
