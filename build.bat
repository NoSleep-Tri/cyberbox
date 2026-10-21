@echo off
setlocal

echo ==============================================================
echo       CyberBox AI - Native C++ Build Script
echo ==============================================================

where cl >nul 2>nul
if %ERRORLEVEL% equ 0 goto use_msvc

where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 goto use_mingw

where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 goto use_clang

where cmake >nul 2>nul
if %ERRORLEVEL% equ 0 goto use_cmake

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    echo [*] Detected Visual Studio BuildTools. Initializing environment...
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    goto use_msvc
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo [*] Detected Visual Studio Community. Initializing environment...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    goto use_msvc
)

echo [ERROR] No C++ compiler detected: cl, g++, clang++, or cmake.
echo Please run from "Developer Command Prompt for Visual Studio" or install MinGW-w64 / GCC.
exit /b 1

:use_msvc
echo [*] Detected MSVC Compiler [cl.exe]. Compiling with MSVC...
cl /EHsc /std:c++17 /O2 /I include /I src src\main.cpp src\app_server.cpp src\ai_client.cpp src\sandbox_manager.cpp src\docker_sandbox.cpp src\win32_jail_sandbox.cpp /Fe:cyberbox_ai.exe ws2_32.lib winhttp.lib shell32.lib shlwapi.lib advapi32.lib
if %ERRORLEVEL% equ 0 (
    echo.
    echo [OK] Build Successful: cyberbox_ai.exe
    echo [*] Run "cyberbox_ai.exe" to start the application!
    exit /b 0
) else (
    echo [ERROR] MSVC compilation failed.
    exit /b 1
)

:use_mingw
echo [*] Detected MinGW Compiler [g++.exe]. Compiling with MinGW...
g++ -std=c++17 -O3 -Iinclude -Isrc src/main.cpp src/app_server.cpp src/ai_client.cpp src/sandbox_manager.cpp src/docker_sandbox.cpp src/win32_jail_sandbox.cpp -o cyberbox_ai.exe -lws2_32 -lwinhttp -lshell32 -lshlwapi
if %ERRORLEVEL% equ 0 (
    echo.
    echo [OK] Build Successful: cyberbox_ai.exe
    echo [*] Run "cyberbox_ai.exe" to start the application!
    exit /b 0
) else (
    echo [ERROR] MinGW compilation failed.
    exit /b 1
)

:use_clang
echo [*] Detected Clang Compiler [clang++.exe]. Compiling with Clang...
clang++ -std=c++17 -O3 -Iinclude -Isrc src/main.cpp src/app_server.cpp src/ai_client.cpp src/sandbox_manager.cpp src/docker_sandbox.cpp src/win32_jail_sandbox.cpp -o cyberbox_ai.exe -lws2_32 -lwinhttp -lshell32 -lshlwapi
if %ERRORLEVEL% equ 0 (
    echo.
    echo [OK] Build Successful: cyberbox_ai.exe
    echo [*] Run "cyberbox_ai.exe" to start the application!
    exit /b 0
) else (
    echo [ERROR] Clang compilation failed.
    exit /b 1
)

:use_cmake
echo [*] Detected CMake. Generating and building with CMake...
if not exist build mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ..
if exist build\Release\cyberbox_ai.exe copy build\Release\cyberbox_ai.exe cyberbox_ai.exe
echo [OK] Build Complete: cyberbox_ai.exe
exit /b 0
