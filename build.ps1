# PowerShell Build Script for CyberBox AI
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host "      CyberBox AI - Native C++ Build Script (PowerShell)      " -ForegroundColor Cyan
Write-Host "==============================================================" -ForegroundColor Cyan

$sources = @(
    "src/main.cpp",
    "src/native_gui.cpp",
    "src/graphics/dx11_device.cpp",
    "src/graphics/d2d_renderer.cpp",
    "src/graphics/icon_renderer.cpp",
    "src/graphics/syntax_highlighter.cpp",
    "src/views/arena_view.cpp",
    "src/views/topic_modal.cpp",
    "src/views/chat_view.cpp",
    "src/views/settings_view.cpp",
    "src/views/workspace_view.cpp",
    "src/views/session_modal.cpp",
    "src/ai_client.cpp",
    "src/sandbox_manager.cpp",
    "src/docker_sandbox.cpp",
    "src/win32_jail_sandbox.cpp",
    "src/anti_screenshot.cpp"
)

# 1. Check for MSVC (cl.exe)
if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
    Write-Host "[*] Phat hien trinh bien dich MSVC (cl.exe). Dang bien dich..." -ForegroundColor Yellow
    $msvcArgs = @(
        "/EHsc", "/std:c++17", "/O2", "/I", "include", "/I", "src",
        "src\main.cpp", "src\native_gui.cpp",
        "src\graphics\dx11_device.cpp", "src\graphics\d2d_renderer.cpp", "src\graphics\icon_renderer.cpp", "src\graphics\syntax_highlighter.cpp",
        "src\views\arena_view.cpp", "src\views\topic_modal.cpp", "src\views\chat_view.cpp", "src\views\settings_view.cpp", "src\views\workspace_view.cpp", "src\views\session_modal.cpp",
        "src\ai_client.cpp", "src\sandbox_manager.cpp", "src\docker_sandbox.cpp", "src\win32_jail_sandbox.cpp", "src\anti_screenshot.cpp",
        "/Fe:cyberbox.exe",
        "user32.lib", "gdi32.lib", "gdiplus.lib", "d3d11.lib", "dxgi.lib", "d2d1.lib", "dwrite.lib", "dwmapi.lib", "comctl32.lib", "ws2_32.lib", "winhttp.lib", "shell32.lib", "shlwapi.lib", "advapi32.lib", "winmm.lib"
    )
    & cl.exe $msvcArgs
    if ($LASTEXITCODE -eq 0) {
        Copy-Item .\cyberbox.exe .\cyberbox_ai.exe -Force -ErrorAction SilentlyContinue
        Write-Host "`n[OK] Bien dich thanh cong: cyberbox.exe (100% Pure C++ Native Desktop)!" -ForegroundColor Green
        Write-Host "[*] Chay .\cyberbox.exe de khoi dong ung dung!" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "[ERROR] Bien dich MSVC that bai." -ForegroundColor Red
        exit 1
    }
}

# 2. Check for MinGW (g++.exe)
if (Get-Command g++.exe -ErrorAction SilentlyContinue) {
    Write-Host "[*] Phat hien trinh bien dich MinGW (g++.exe). Dang bien dich..." -ForegroundColor Yellow
    $gccArgs = @(
        "-std=c++17", "-O3", "-Iinclude", "-Isrc"
    ) + $sources + @(
        "-o", "cyberbox.exe",
        "-luser32", "-lgdi32", "-lgdiplus", "-ld2d1", "-ldwrite", "-ldwmapi", "-lcomctl32", "-lws2_32", "-lwinhttp", "-lshell32", "-lshlwapi", "-ladvapi32", "-lwinmm"
    )
    & g++.exe $gccArgs
    if ($LASTEXITCODE -eq 0) {
        Copy-Item .\cyberbox.exe .\cyberbox_ai.exe -Force -ErrorAction SilentlyContinue
        Write-Host "`n[OK] Bien dich thanh cong: cyberbox.exe!" -ForegroundColor Green
        Write-Host "[*] Chay .\cyberbox.exe de khoi dong ung dung!" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "[ERROR] Bien dich MinGW that bai." -ForegroundColor Red
        exit 1
    }
}

# 3. Check for Clang (clang++.exe)
if (Get-Command clang++.exe -ErrorAction SilentlyContinue) {
    Write-Host "[*] Phat hien trinh bien dich Clang (clang++.exe). Dang bien dich..." -ForegroundColor Yellow
    $clangArgs = @(
        "-std=c++17", "-O3", "-Iinclude", "-Isrc"
    ) + $sources + @(
        "-o", "cyberbox.exe",
        "-luser32", "-lgdi32", "-lgdiplus", "-ld2d1", "-ldwrite", "-ldwmapi", "-lcomctl32", "-lws2_32", "-lwinhttp", "-lshell32", "-lshlwapi", "-ladvapi32", "-lwinmm"
    )
    & clang++.exe $clangArgs
    if ($LASTEXITCODE -eq 0) {
        Copy-Item .\cyberbox.exe .\cyberbox_ai.exe -Force -ErrorAction SilentlyContinue
        Write-Host "`n[OK] Bien dich thanh cong: cyberbox.exe!" -ForegroundColor Green
        Write-Host "[*] Chay .\cyberbox.exe de khoi dong ung dung!" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "[ERROR] Bien dich Clang that bai." -ForegroundColor Red
        exit 1
    }
}

# 4. Check for CMake
if (Get-Command cmake.exe -ErrorAction SilentlyContinue) {
    Write-Host "[*] Phat hien CMake. Dang bien dich bang CMake..." -ForegroundColor Yellow
    if (!(Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }
    Push-Location build
    cmake ..
    cmake --build . --config Release
    Pop-Location
    if (Test-Path "build\Release\cyberbox.exe") {
        Copy-Item "build\Release\cyberbox.exe" "cyberbox.exe"
    }
    Write-Host "`n[OK] Bien dich thanh cong: cyberbox.exe!" -ForegroundColor Green
    exit 0
}

# 5. Check for installed Visual Studio / Build Tools if cl is not in current PATH
$vcvarsCandidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)

foreach ($vcvars in $vcvarsCandidates) {
    if (Test-Path $vcvars) {
        Write-Host "[*] Phat hien Visual Studio tai: $vcvars" -ForegroundColor Yellow
        $compileCmd = "call `"$vcvars`" && cl /EHsc /std:c++17 /O2 /I include /I src src\main.cpp src\native_gui.cpp src\graphics\dx11_device.cpp src\graphics\d2d_renderer.cpp src\graphics\icon_renderer.cpp src\graphics\syntax_highlighter.cpp src\views\arena_view.cpp src\views\topic_modal.cpp src\views\chat_view.cpp src\views\settings_view.cpp src\views\workspace_view.cpp src\views\session_modal.cpp src\ai_client.cpp src\sandbox_manager.cpp src\docker_sandbox.cpp src\win32_jail_sandbox.cpp src\anti_screenshot.cpp /Fe:cyberbox.exe user32.lib gdi32.lib gdiplus.lib d3d11.lib dxgi.lib d2d1.lib dwrite.lib dwmapi.lib comctl32.lib ws2_32.lib winhttp.lib shell32.lib shlwapi.lib advapi32.lib winmm.lib && del *.obj"
        cmd.exe /c $compileCmd
        if ($LASTEXITCODE -eq 0 -and (Test-Path .\cyberbox.exe)) {
            Copy-Item .\cyberbox.exe .\cyberbox_ai.exe -Force -ErrorAction SilentlyContinue
            Write-Host "`n[OK] Bien dich thanh cong: cyberbox.exe (100% Pure C++ Native Desktop)!" -ForegroundColor Green
            Write-Host "[*] Chay .\cyberbox.exe de khoi dong ung dung!" -ForegroundColor Green
            exit 0
        }
    }
}

Write-Host "[ERROR] Khong tim thay trinh bien dich C++ nao (cl, g++, clang++, hoac cmake) tren he thong!" -ForegroundColor Red
Write-Host "Huong dan cai dat nhanh:" -ForegroundColor Yellow
Write-Host "1. Neu dung MinGW: Cai dat w64devkit hoac msys2 va them vao PATH."
Write-Host "2. Neu dung Visual Studio: Mo 'Developer PowerShell for VS' hoac 'Developer Command Prompt' roi chay lai script."
exit 1
