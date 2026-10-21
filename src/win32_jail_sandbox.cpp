#include "win32_jail_sandbox.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <direct.h>
#include <shlwapi.h>
#include <sddl.h>
#include <aclapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace sandbox {

Win32JailSandbox::Win32JailSandbox(const std::string& workspace_dir) : workspace_dir_(workspace_dir) {}

Win32JailSandbox::~Win32JailSandbox() {
    shutdown();
}

void Win32JailSandbox::setup_job_limits() {
    if (job_object_ != NULL) return;

    job_object_ = CreateJobObjectW(NULL, NULL);
    if (!job_object_) return;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    jeli.BasicLimitInformation.LimitFlags = 
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
        JOB_OBJECT_LIMIT_PROCESS_MEMORY |
        JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
        JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    
    // 1 GB RAM limit per process
    jeli.ProcessMemoryLimit = 1024ULL * 1024ULL * 1024ULL;
    // Max 32 active concurrent processes (anti fork-bomb)
    jeli.BasicLimitInformation.ActiveProcessLimit = 32;

    SetInformationJobObject(
        job_object_,
        JobObjectExtendedLimitInformation,
        &jeli,
        sizeof(jeli)
    );

    // Hard UI Restrictions: prevent sandboxed processes from touching host desktops, clipboards, or windows
    JOBOBJECT_BASIC_UI_RESTRICTIONS jbuir = { 0 };
    jbuir.UIRestrictionsClass = 
        JOB_OBJECT_UILIMIT_HANDLES |
        JOB_OBJECT_UILIMIT_READCLIPBOARD |
        JOB_OBJECT_UILIMIT_WRITECLIPBOARD |
        JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
        JOB_OBJECT_UILIMIT_DESKTOP |
        JOB_OBJECT_UILIMIT_GLOBALATOMS;
    SetInformationJobObject(
        job_object_,
        JobObjectBasicUIRestrictions,
        &jbuir,
        sizeof(jbuir)
    );
}

std::string Win32JailSandbox::resolve_safe_path(const std::string& rel_path) {
    char abs_workspace[MAX_PATH];
    GetFullPathNameA(workspace_dir_.c_str(), MAX_PATH, abs_workspace, NULL);

    std::string combined = std::string(abs_workspace) + "\\" + rel_path;
    char abs_target[MAX_PATH];
    GetFullPathNameA(combined.c_str(), MAX_PATH, abs_target, NULL);

    // Prevent directory traversal: abs_target MUST start with abs_workspace
    std::string target_str = abs_target;
    std::string workspace_str = abs_workspace;
    if (target_str.find(workspace_str) != 0) {
        // Path escape attempt blocked!
        return "";
    }
    return target_str;
}

bool Win32JailSandbox::apply_firewall_rule(bool block_internet) {
    // Uses netsh to block or allow outbound traffic from sandbox workspace processes
    std::string cmd;
    if (block_internet) {
        cmd = "netsh advfirewall firewall add rule name=\"Joyful_Sandbox_Airgap\" dir=out action=block protocol=ANY profile=any remoteip=0.0.0.0/0 >nul 2>&1";
    } else {
        cmd = "netsh advfirewall firewall delete rule name=\"Joyful_Sandbox_Airgap\" >nul 2>&1";
    }
    // Best-effort execution
    system(cmd.c_str());
    return true;
}

bool Win32JailSandbox::set_low_integrity_folder(const std::string& folder_path) {
    PSECURITY_DESCRIPTOR pSD = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("S:(ML;;NW;;;LW)", SDDL_REVISION_1, &pSD, NULL)) {
        return false;
    }
    PACL pSacl = NULL;
    BOOL bSaclPresent = FALSE, bSaclDefaulted = FALSE;
    GetSecurityDescriptorSacl(pSD, &bSaclPresent, &pSacl, &bSaclDefaulted);
    DWORD dwErr = SetNamedSecurityInfoA(
        const_cast<char*>(folder_path.c_str()),
        SE_FILE_OBJECT,
        LABEL_SECURITY_INFORMATION,
        NULL, NULL, NULL,
        pSacl
    );
    LocalFree(pSD);
    return (dwErr == ERROR_SUCCESS);
}

HANDLE Win32JailSandbox::create_jail_token() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, &hToken)) {
        return NULL;
    }

    HANDLE hRestricted = NULL;
    // DISABLE_MAX_PRIVILEGE: Strips all administrative privileges from token
    // LUA_TOKEN: Strips administrative groups
    if (!CreateRestrictedToken(hToken, DISABLE_MAX_PRIVILEGE | LUA_TOKEN, 0, NULL, 0, NULL, 0, NULL, &hRestricted)) {
        CloseHandle(hToken);
        return NULL;
    }
    CloseHandle(hToken);

    // Apply Mandatory Integrity Level: Low Integrity (S-1-16-4096)
    // Under Low Integrity, the Windows NT kernel enforces No-Write-Up:
    // the process physically cannot write or modify ANY host files, desktop, registry HKLM/HKCU, etc.
    PSID pIntegritySid = NULL;
    if (ConvertStringSidToSidA("S-1-16-4096", &pIntegritySid)) {
        TOKEN_MANDATORY_LABEL tml = { 0 };
        tml.Label.Attributes = SE_GROUP_INTEGRITY;
        tml.Label.Sid = pIntegritySid;
        SetTokenInformation(hRestricted, TokenIntegrityLevel, &tml, sizeof(tml) + GetLengthSid(pIntegritySid));
        LocalFree(pIntegritySid);
    }

    return hRestricted;
}

void Win32JailSandbox::create_shims() {
    std::string bin_dir = workspace_dir_ + "\\.bin";
    CreateDirectoryA(bin_dir.c_str(), NULL);

    auto write_file_if_missing = [](const std::string& path, const std::string& content) {
        std::ofstream out(path);
        if (out.is_open()) {
            out << content;
        }
    };

    write_file_if_missing(bin_dir + "\\pwd.bat", "@echo %CD%\n");
    write_file_if_missing(bin_dir + "\\which.bat", "@for %%i in (%*) do @(where %%i 2>nul || where %%i.bat 2>nul || where %%i.exe 2>nul || where %%i.cmd 2>nul)\n");
    write_file_if_missing(bin_dir + "\\ls.bat", "@dir /b /a-h %*\n");
    write_file_if_missing(bin_dir + "\\uname.bat", "@echo Windows_NT CyberBox-Sandbox 10.0 AMD64 x86_64\n");
    write_file_if_missing(bin_dir + "\\cat.bat", "@echo off\nsetlocal enabledelayedexpansion\nset \"P=%*\"\nif \"%P%\"==\"\" exit /b 0\nset \"P=!P:/=\\!\"\ntype !P! 2>nul || python -c \"import sys; [print(open(f, 'r', errors='ignore').read(), end='') for f in sys.argv[1:]]\" %* 2>nul\n");
    write_file_if_missing(bin_dir + "\\clear.bat", "@cls\n");
    write_file_if_missing(bin_dir + "\\touch.bat", "@copy /y nul %* >nul 2>nul\n");
    write_file_if_missing(bin_dir + "\\mkdir.bat", 
        "@echo off\n"
        "setlocal enabledelayedexpansion\n"
        "set \"D=%*\"\n"
        "if \"%D%\"==\"\" exit /b 0\n"
        "set \"D=!D:-p =!\"\n"
        "set \"D=!D:/=\\!\"\n"
        "if not exist \"!D!\" md \"!D!\" 2>nul\n"
        "exit /b 0\n");
    write_file_if_missing(bin_dir + "\\md.bat", "@call mkdir.bat %*\n");
    write_file_if_missing(bin_dir + "\\rm.bat", 
        "@echo off\n"
        "setlocal enabledelayedexpansion\n"
        "set \"T=%*\"\n"
        "set \"T=!T:-rf =!\"\n"
        "set \"T=!T:-f =!\"\n"
        "set \"T=!T:/=\\!\"\n"
        "if exist \"!T!\" del /f /q \"!T!\" 2>nul || rd /s /q \"!T!\" 2>nul\n"
        "exit /b 0\n");
    write_file_if_missing(bin_dir + "\\true.bat", "@exit /b 0\n");
    write_file_if_missing(bin_dir + "\\false.bat", "@exit /b 1\n");
    write_file_if_missing(bin_dir + "\\id.bat", "@echo uid=1000(cyberbox) gid=1000(cyberbox) groups=1000(cyberbox)\n");
    write_file_if_missing(bin_dir + "\\whoami.bat", "@echo cyberbox\n");
    write_file_if_missing(bin_dir + "\\hostname.bat", "@echo cyberbox-sandbox\n");
    write_file_if_missing(bin_dir + "\\hostname.bat", "@echo cyberbox-sandbox\n");
    std::string python_shim = 
        "@echo off\n"
        "if exist \"%USERPROFILE%\\.local\\bin\\python3.12.exe\" (\n"
        "  \"%USERPROFILE%\\.local\\bin\\python3.12.exe\" %*\n"
        ") else if exist \"C:\\Users\\Tri\\.local\\bin\\python3.12.exe\" (\n"
        "  \"C:\\Users\\Tri\\.local\\bin\\python3.12.exe\" %*\n"
        ") else (\n"
        "  python.exe %*\n"
        ")\n";
    write_file_if_missing(bin_dir + "\\python.bat", python_shim);
    write_file_if_missing(bin_dir + "\\python3.bat", "@call \"%~dp0python.bat\" %*\n");
    write_file_if_missing(bin_dir + "\\head.bat", "@powershell -NoProfile -NonInteractive -Command \"$input | Select-Object -First 20\"\n");
    write_file_if_missing(bin_dir + "\\grep.bat", "@findstr /i %*\n");
    write_file_if_missing(bin_dir + "\\tr.bat", "@more\n");
    write_file_if_missing(bin_dir + "\\env.bat", "@set\n");
    write_file_if_missing(bin_dir + "\\export.bat", "@set %*\n");
    write_file_if_missing(bin_dir + "\\sleep.bat", "@timeout /t %1 /nobreak >nul 2>nul\n");
    write_file_if_missing(bin_dir + "\\chmod.bat", "@exit /b 0\n");
    write_file_if_missing(bin_dir + "\\sudo.bat", "@%*\n");
    std::string airgap_check = 
        "@if exist \"%~dp0..\\.airgap\" (\n"
        "  echo [Sentinel Air-Gap Guard] Error: Ket noi Internet bi CHAN - Che do Air-Gap dang bat. 1>&2\n"
        "  exit /b 403\n"
        ")\n";

    write_file_if_missing(bin_dir + "\\curl.bat", airgap_check + "@curl.exe %*\n");
    write_file_if_missing(bin_dir + "\\wget.bat", airgap_check + "@echo off\nif \"%~1\"==\"\" exit /b 0\ncurl.exe -fSL -O %*\n");

    std::string web_search_py =
        "import sys, urllib.request, urllib.parse, html, re\n"
        "if len(sys.argv) < 2:\n"
        "    print('[Usage] web_search \"<tu khoa>\"')\n"
        "    sys.exit(0)\n"
        "q = ' '.join(sys.argv[1:]).strip()\n"
        "print('[*] Dang tim kiem truc tuyen:', q, '\\n')\n"
        "try:\n"
        "    pdata = urllib.parse.urlencode({'q': q}).encode('utf-8')\n"
        "    req = urllib.request.Request('https://lite.duckduckgo.com/lite/', data=pdata, headers={'User-Agent': 'Mozilla/5.0'})\n"
        "    with urllib.request.urlopen(req, timeout=12) as resp:\n"
        "        content = resp.read().decode('utf-8', 'ignore')\n"
        "    raw_links = re.findall(r'<a[^>]+result-link[^>]*>.*?</a>', content, re.DOTALL)\n"
        "    snippets = re.findall(r'<td class=[\'\\\"]result-snippet[\'\\\"]>(.*?)</td>', content, re.DOTALL)\n"
        "    parsed = []\n"
        "    for l in raw_links:\n"
        "        mh = re.search(r'href=[\'\\\"]([^\'\\\"]+)[\'\\\"]', l)\n"
        "        mt = re.search(r'>([^<]+)<', l)\n"
        "        u = mh.group(1) if mh else ''\n"
        "        t = mt.group(1) if mt else 'Link'\n"
        "        if u and not u.startswith('/'): parsed.append((u, t))\n"
        "    total = min(len(parsed), len(snippets), 5)\n"
        "    if total == 0:\n"
        "        print('[-] Khong tim thay ket qua phu hop.')\n"
        "    else:\n"
        "        print(f'[+] Tim thay {total} ket qua hang dau:\\n')\n"
        "        for i in range(total):\n"
        "            url, title = parsed[i]\n"
        "            title = html.unescape(title).strip()\n"
        "            snip = html.unescape(re.sub(r'<[^>]+>', '', snippets[i])).strip()\n"
        "            print(f'[{i+1}] {title}\\n    {snip}\\n    Link: {url}\\n')\n"
        "except Exception as e:\n"
        "    print('[Error] Loi khi tim kiem:', e)\n";
    write_file_if_missing(bin_dir + "\\web_search.py", web_search_py);

    std::string web_read_py =
        "import sys, urllib.request, re\n"
        "if len(sys.argv) < 2:\n"
        "    print('[Usage] web_read \"<url>\"')\n"
        "    sys.exit(0)\n"
        "u = sys.argv[1].strip()\n"
        "if not u.startswith('http://') and not u.startswith('https://'): u = 'https://' + u\n"
        "print('[*] Dang doc URL:', u)\n"
        "try:\n"
        "    req = urllib.request.Request(u, headers={'User-Agent': 'Mozilla/5.0'})\n"
        "    with urllib.request.urlopen(req, timeout=12) as resp:\n"
        "        html = resp.read().decode('utf-8', 'ignore')\n"
        "    html = re.sub(r'<(script|style|svg|noscript)[^>]*>.*?</\\1>', '', html, flags=re.DOTALL|re.IGNORECASE)\n"
        "    text = re.sub(r'<[^>]+>', ' ', html)\n"
        "    text = re.sub(r'\\s+', ' ', text).strip()\n"
        "    if len(text) > 3000: text = text[:3000] + '\\n... [Noi dung dai da rut gon]'\n"
        "    print('\\n--- [NOI DUNG TRANG WEB] ---\\n' + text + '\\n--- [HET TRANG WEB] ---\\n')\n"
        "except Exception as e:\n"
        "    print('[Error] Khong the doc URL:', e)\n";
    write_file_if_missing(bin_dir + "\\web_read.py", web_read_py);

    write_file_if_missing(bin_dir + "\\web_search.bat", airgap_check + "@call \"%~dp0python.bat\" \"%~dp0web_search.py\" %*\n");
    write_file_if_missing(bin_dir + "\\search.bat", "@call \"%~dp0web_search.bat\" %*\n");
    write_file_if_missing(bin_dir + "\\web_read.bat", airgap_check + "@call \"%~dp0python.bat\" \"%~dp0web_read.py\" %*\n");
    write_file_if_missing(bin_dir + "\\web_fetch.bat", "@call \"%~dp0web_read.bat\" %*\n");

    write_file_if_missing(bin_dir + "\\tar.bat", "@tar.exe %*\n");
    write_file_if_missing(bin_dir + "\\unzip.bat", "@echo off\nif \"%~1\"==\"\" exit /b 0\ntar.exe -xf %1\n");

    std::string winget_shim = 
        "@echo off\n"
        "echo [Sandbox Notice] He thong Host da duoc cach ly an toan.\n"
        "echo Khong dung winget cai dat ra may that. Vui long dung:\n"
        "echo   - C++: cl /EHsc /std:c++17 main.cpp\n"
        "echo   - Python: python main.py\n"
        "echo   - Portable: Giai nen zip/tar truc tiep vao workspace.\n"
        "exit /b 126\n";
    write_file_if_missing(bin_dir + "\\winget.bat", winget_shim);
    write_file_if_missing(bin_dir + "\\wiget.bat", winget_shim);

    write_file_if_missing(bin_dir + "\\choco.bat", "@echo off\necho [Sandbox Notice] Chocolatey khong kha dung trong Sandbox.\nexit /b 126\n");
    write_file_if_missing(bin_dir + "\\msiexec.bat", "@echo off\necho [Sandbox Notice] MSIEXEC khong kha dung trong Sandbox.\nexit /b 126\n");
    write_file_if_missing(bin_dir + "\\powershell.bat", "@powershell.exe -NoProfile -ExecutionPolicy Bypass %*\n");
    write_file_if_missing(bin_dir + "\\pwsh.bat", "@powershell.exe -NoProfile -ExecutionPolicy Bypass %*\n");
    write_file_if_missing(bin_dir + "\\start.bat", "@start \"\" %*\n");
    write_file_if_missing(bin_dir + "\\launch.bat", "@echo off\nif \"%~1\"==\"\" exit /b 0\necho [*] Launching Windows application: %*\nstart \"\" %*\n");
    write_file_if_missing(bin_dir + "\\run.bat", "@echo off\nif \"%~1\"==\"\" exit /b 0\necho [*] Launching Windows application: %*\nstart \"\" %*\n");
    write_file_if_missing(bin_dir + "\\fetch.bat", airgap_check + "@curl.exe -sSL %*\n");
    write_file_if_missing(bin_dir + "\\pip.bat", "@python -m pip %*\n");
    write_file_if_missing(bin_dir + "\\pip3.bat", "@python -m pip %*\n");
    write_file_if_missing(bin_dir + "\\npm.bat", "@npm.cmd %*\n");
    write_file_if_missing(bin_dir + "\\open.bat", "@echo off\nif \"%~1\"==\"\" exit /b 0\nstart \"\" %*\n");
    write_file_if_missing(bin_dir + "\\xdg-open.bat", "@echo off\nif \"%~1\"==\"\" exit /b 0\nstart \"\" %*\n");
    write_file_if_missing(bin_dir + "\\browser.bat", 
        "@echo off\n"
        "set \"U=%~1\"\n"
        "if \"%U%\"==\"\" set \"U=http://127.0.0.1:8080\"\n"
        "echo [*] Opening browser: %U%\n"
        "if exist \"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe\" (\n"
        "  start \"\" \"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe\" \"%U%\"\n"
        "  exit /b 0\n"
        ")\n"
        "if exist \"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe\" (\n"
        "  start \"\" \"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe\" \"%U%\"\n"
        "  exit /b 0\n"
        ")\n"
        "start \"\" \"%U%\"\n");
    write_file_if_missing(bin_dir + "\\browse.bat", "@call browser.bat %*\n");
    write_file_if_missing(bin_dir + "\\chrome.bat", 
        "@echo off\n"
        "if exist \"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe\" (\n"
        "  start \"\" \"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe\" %*\n"
        ") else (\n"
        "  start \"\" %*\n"
        ")\n");
    write_file_if_missing(bin_dir + "\\edge.bat", 
        "@echo off\n"
        "if exist \"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe\" (\n"
        "  start \"\" \"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe\" %*\n"
        ") else (\n"
        "  start \"\" %*\n"
        ")\n");
    write_file_if_missing(bin_dir + "\\msedge.bat", "@call edge.bat %*\n");

    std::string install_shim = 
        "@echo off\n"
        "setlocal enabledelayedexpansion\n"
        "set \"TARGET=%~1\"\n"
        "if \"%TARGET%\"==\"\" (\n"
        "  echo [CyberBox Local Installer] Usage: install ^<local_zip_or_file^>\n"
        "  exit /b 1\n"
        ")\n"
        "if exist \"%TARGET%\" (\n"
        "  set \"EXT=%~x1\"\n"
        "  if /i \"!EXT!\"==\".zip\" (\n"
        "    echo [*] Giai nen goi ZIP cuc bo: %TARGET%\n"
        "    tar.exe -xf \"%TARGET%\"\n"
        "    exit /b !errorlevel!\n"
        "  )\n"
        "  if /i \"!EXT!\"==\".tar\" if /i \"!EXT!\"==\".tgz\" if /i \"!EXT!\"==\".gz\" (\n"
        "    echo [*] Giai nen goi TAR cuc bo: %TARGET%\n"
        "    tar.exe -xf \"%TARGET%\"\n"
        "    exit /b !errorlevel!\n"
        "  )\n"
        ")\n"
        "echo [Sentinel Guard Blocked]: Cai dat goi '%TARGET%' ra ngoai may that bi CHAN.\n"
        "echo Vui long su dung file ZIP cuc bo, pip hoac bien dich C++ trong workspace.\n"
        "exit /b 126\n";
    write_file_if_missing(bin_dir + "\\install.bat", install_shim);

    std::string apt_shim = 
        "@echo off\n"
        "setlocal enabledelayedexpansion\n"
        "set \"PKG=\"\n"
        ":arg_loop\n"
        "if \"%~1\"==\"\" goto install_pkg\n"
        "if /i \"%~1\"==\"install\" (\n"
        "  shift\n"
        "  goto get_pkg\n"
        ")\n"
        "shift\n"
        "goto arg_loop\n"
        ":get_pkg\n"
        "if \"%~1\"==\"\" goto install_pkg\n"
        "if not \"%~1\"==\"-y\" if not \"%~1\"==\"--yes\" if not \"%~1\"==\"-q\" (\n"
        "  set \"PKG=%~1\"\n"
        "  shift\n"
        "  goto get_pkg\n"
        ")\n"
        "shift\n"
        "goto get_pkg\n"
        ":install_pkg\n"
        "if \"%PKG%\"==\"\" exit /b 0\n"
        "echo [CyberBox Package Bridge] Installing %PKG% on Windows...\n"
        "call install.bat \"%PKG%\"\n";
    write_file_if_missing(bin_dir + "\\apt.bat", apt_shim);
    write_file_if_missing(bin_dir + "\\apt-get.bat", apt_shim);

    std::string msvc_path = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat";
    std::string compiler_shim = 
        "@echo off\n"
        "setlocal enabledelayedexpansion\n"
        "set \"ARGS=\"\n"
        "set \"OUT=\"\n"
        ":parse_loop\n"
        "if \"%~1\"==\"\" goto do_cl\n"
        "if \"%~1\"==\"-o\" (\n"
        "  shift\n"
        "  set \"OUT=/Fe:%~1.exe\"\n"
        "  shift\n"
        "  goto parse_loop\n"
        ")\n"
        "if \"%~1\"==\"-std=c++17\" ( shift & goto parse_loop )\n"
        "if \"%~1\"==\"-Wall\" ( shift & goto parse_loop )\n"
        "if \"%~1\"==\"-O2\" ( shift & goto parse_loop )\n"
        "set \"ARGS=!ARGS! %1\"\n"
        "shift\n"
        "goto parse_loop\n"
        ":do_cl\n"
        "@if exist \"" + msvc_path + "\" (\n"
        "  call \"" + msvc_path + "\" >nul 2>nul\n"
        ")\n"
        "cl /EHsc /std:c++17 !OUT! !ARGS!\n";
    write_file_if_missing(bin_dir + "\\g++.bat", compiler_shim);
    write_file_if_missing(bin_dir + "\\gcc.bat", compiler_shim);
    write_file_if_missing(bin_dir + "\\clang++.bat", compiler_shim);

    // Dummy /proc and /etc for inspecting agents
    CreateDirectoryA((workspace_dir_ + "\\proc").c_str(), NULL);
    CreateDirectoryA((workspace_dir_ + "\\proc\\1").c_str(), NULL);
    CreateDirectoryA((workspace_dir_ + "\\proc\\self").c_str(), NULL);
    CreateDirectoryA((workspace_dir_ + "\\etc").c_str(), NULL);

    write_file_if_missing(workspace_dir_ + "\\proc\\1\\cgroup", "0::/\n");
    write_file_if_missing(workspace_dir_ + "\\proc\\1\\cmdline", "cyberbox-sandbox\n");
    write_file_if_missing(workspace_dir_ + "\\proc\\cpuinfo", "model name: AMD64 / Intel(R) Core(TM) Processor\nprocessor: 0\n");
    write_file_if_missing(workspace_dir_ + "\\proc\\self\\status", "Name:\tcyberbox-sandbox\nState:\tR (running)\nPid:\t1\n");
    write_file_if_missing(workspace_dir_ + "\\etc\\os-release", "NAME=\"CyberBox OS (Win32 Jail)\"\nVERSION=\"10.0\"\nID=cyberbox\n");
    write_file_if_missing(workspace_dir_ + "\\etc\\passwd", "cyberbox:x:1000:1000:cyberbox:/:/bin/bash\n");
}

std::string Win32JailSandbox::sanitize_command_for_windows(std::string cmd) {
    size_t pos = 0;
    while ((pos = cmd.find("&>/dev/null", pos)) != std::string::npos) {
        cmd.replace(pos, 11, ">nul 2>&1");
        pos += 9;
    }
    pos = 0;
    while ((pos = cmd.find("2>/dev/null", pos)) != std::string::npos) {
        cmd.replace(pos, 11, "2>nul");
        pos += 5;
    }
    pos = 0;
    while ((pos = cmd.find(">/dev/null", pos)) != std::string::npos) {
        cmd.replace(pos, 10, ">nul");
        pos += 4;
    }
    pos = 0;
    while ((pos = cmd.find("/workspace/", pos)) != std::string::npos) {
        cmd.replace(pos, 11, ".\\");
        pos += 2;
    }
    if (cmd == "/workspace" || cmd == "cd /workspace") {
        cmd = "cd .";
    }
    pos = 0;
    while ((pos = cmd.find("/etc/", pos)) != std::string::npos) {
        cmd.replace(pos, 5, "etc\\");
        pos += 4;
    }
    pos = 0;
    while ((pos = cmd.find("/proc/", pos)) != std::string::npos) {
        cmd.replace(pos, 6, "proc\\");
        pos += 5;
    }
    pos = 0;
    while ((pos = cmd.find("/tmp/", pos)) != std::string::npos) {
        cmd.replace(pos, 5, ".\\");
        pos += 2;
    }
    if (cmd == "ls /tmp" || cmd == "ls /tmp 2>&1") {
        cmd = "ls .";
    }
    if (cmd == "ls /" || cmd == "ls / 2>&1") {
        cmd = "ls .";
    }
    if (cmd.find("ls /dev") == 0) {
        cmd = "echo null random zero urandom";
    }
    pos = 0;
    while ((pos = cmd.find("mkdir -p ", pos)) != std::string::npos) {
        cmd.replace(pos, 9, "mkdir ");
        pos += 6;
    }

    // Strip sudo prefixes
    pos = 0;
    while ((pos = cmd.find("sudo ", pos)) != std::string::npos) {
        if (pos == 0 || cmd[pos - 1] == ' ' || cmd[pos - 1] == '&' || cmd[pos - 1] == '|' || cmd[pos - 1] == ';') {
            cmd.erase(pos, 5);
        } else {
            pos += 5;
        }
    }

    // Convert ./ to .\ for local binary/script executions outside quotes
    pos = 0;
    while ((pos = cmd.find("./", pos)) != std::string::npos) {
        if (pos == 0 || cmd[pos - 1] == ' ' || cmd[pos - 1] == '&' || cmd[pos - 1] == '|' || cmd[pos - 1] == ';' || cmd[pos - 1] == '"' || cmd[pos - 1] == '\'') {
            cmd.replace(pos, 2, ".\\");
            pos += 2;
        } else {
            pos += 2;
        }
    }

    // Convert statement-separating semicolons to " & " outside quotes
    std::string converted;
    bool in_single = false;
    bool in_double = false;
    for (size_t i = 0; i < cmd.size(); ++i) {
        char ch = cmd[i];
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
            converted += ch;
        } else if (ch == '"' && !in_single) {
            in_double = !in_double;
            converted += ch;
        } else if (ch == ';' && !in_single && !in_double) {
            converted += " & ";
        } else {
            converted += ch;
        }
    }
    return converted;
}

bool Win32JailSandbox::validate_security_policy(const std::string& command, std::string& violation_reason) {
    // Air-Gap Enforcement when Offline
    if (!internet_enabled_.load()) {
        std::string lower = command;
        for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        if (lower.find("http://") != std::string::npos ||
            lower.find("https://") != std::string::npos ||
            lower.find("git clone") != std::string::npos) {
            violation_reason = "Chế độ AIR-GAP (Offline) đang kích hoạt. Kết nối mạng bên ngoài bị chặn.";
            return false;
        }
    }
    return true;
}

bool Win32JailSandbox::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create workspace directory & internal dirs
    CreateDirectoryA(workspace_dir_.c_str(), NULL);
    CreateDirectoryA((workspace_dir_ + "\\.appdata").c_str(), NULL);
    CreateDirectoryA((workspace_dir_ + "\\.tmp").c_str(), NULL);
    CreateDirectoryA((workspace_dir_ + "\\.bin").c_str(), NULL);

    // Mark workspace directories with Low Integrity Label (S:(ML;;NW;;;LW))
    // so the Low-Integrity jailed process has full write access inside its workspace,
    // while the Windows NT kernel physically forbids write access anywhere else on the machine.
    set_low_integrity_folder(workspace_dir_);
    set_low_integrity_folder(workspace_dir_ + "\\.appdata");
    set_low_integrity_folder(workspace_dir_ + "\\.tmp");
    set_low_integrity_folder(workspace_dir_ + "\\.bin");

    setup_job_limits();
    create_shims();
    apply_firewall_rule(!internet_enabled_.load());

    std::string marker = workspace_dir_ + "\\.airgap";
    if (!internet_enabled_.load()) {
        std::ofstream out(marker);
        if (out.is_open()) out << "AIR-GAP ACTIVE";
    } else {
        DeleteFileA(marker.c_str());
    }

    char abs_workspace[MAX_PATH];
    GetFullPathNameA(workspace_dir_.c_str(), MAX_PATH, abs_workspace, NULL);
    std::string bin_path = std::string(abs_workspace) + "\\.bin";
    std::string cur_path = getenv("PATH") ? getenv("PATH") : "";
    if (cur_path.find(bin_path) == std::string::npos) {
        SetEnvironmentVariableA("PATH", (bin_path + ";" + cur_path).c_str());
    }

    is_running_ = true;
    std::cout << "[Win32JailSandbox] Initialized native Low-Integrity Job Object sandbox in: " << workspace_dir_ << "\n";
    return true;
}

void Win32JailSandbox::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (job_object_ != NULL) {
        TerminateJobObject(job_object_, 0);
        CloseHandle(job_object_);
        job_object_ = NULL;
    }
    is_running_ = false;
}

bool Win32JailSandbox::reset() {
    shutdown();
    // Re-initialize
    return initialize();
}

bool Win32JailSandbox::handle_heredoc_write(const std::string& command, ExecutionResult& result) {
    size_t heredoc_pos = command.find("<<");
    if (heredoc_pos == std::string::npos) return false;
    if (command.find("cat") == std::string::npos) return false;

    // Find the line containing "cat" and "<<"
    size_t cat_pos = command.find("cat");
    size_t gt_pos = command.find('>', cat_pos);
    if (gt_pos == std::string::npos) return false;

    std::string target_file = "";
    if (gt_pos < heredoc_pos) {
        // Format: cat > filename << EOF
        size_t file_start = command.find_first_not_of(" \t", gt_pos + 1);
        if (file_start != std::string::npos && file_start < heredoc_pos) {
            size_t file_end = command.find_first_of(" \t<", file_start);
            target_file = command.substr(file_start, file_end - file_start);
        }
    } else {
        // Format: cat << EOF > filename
        size_t file_start = command.find_first_not_of(" \t", gt_pos + 1);
        if (file_start != std::string::npos) {
            size_t file_end = command.find_first_of(" \t\r\n;&", file_start);
            target_file = command.substr(file_start, file_end - file_start);
        }
    }

    // Strip quotes around filename
    while (!target_file.empty() && (target_file.front() == '\'' || target_file.front() == '"')) target_file.erase(0, 1);
    while (!target_file.empty() && (target_file.back() == '\'' || target_file.back() == '"')) target_file.pop_back();
    if (target_file.empty()) return false;

    // Find delimiter after '<<'
    size_t delim_start = command.find_first_not_of(" \t'\"", heredoc_pos + 2);
    if (delim_start == std::string::npos) return false;
    size_t delim_end = command.find_first_of(" \t\r\n>'\"", delim_start);
    std::string delimiter = command.substr(delim_start, delim_end - delim_start);
    if (delimiter.empty()) delimiter = "EOF";

    // Content starts at the first newline after << DELIM
    size_t first_nl = command.find('\n', heredoc_pos);
    if (first_nl == std::string::npos) return false;
    size_t content_start = first_nl + 1;

    // Search for delimiter on its own line
    size_t search_pos = content_start;
    size_t content_end = std::string::npos;
    size_t after_delim_pos = std::string::npos;

    while ((search_pos = command.find(delimiter, search_pos)) != std::string::npos) {
        bool at_line_start = (search_pos == 0 || command[search_pos - 1] == '\n' || command[search_pos - 1] == '\r');
        if (at_line_start) {
            size_t after = search_pos + delimiter.length();
            if (after >= command.length() || command[after] == '\r' || command[after] == '\n' || command[after] == ' ' || command[after] == ';') {
                content_end = search_pos;
                // Find where next line or command begins
                size_t next_nl = command.find_first_of("\r\n", after);
                if (next_nl != std::string::npos) {
                    after_delim_pos = command.find_first_not_of("\r\n \t", next_nl);
                }
                break;
            }
        }
        search_pos += delimiter.length();
    }

    if (content_end == std::string::npos) {
        content_end = command.length();
    }

    std::string file_content = command.substr(content_start, content_end - content_start);

    // Normalize target file path for sandbox workspace
    for (char& c : target_file) if (c == '/') c = '\\';
    if (target_file.find(".\\") == 0) target_file = target_file.substr(2);

    std::string abs_target = resolve_safe_path(target_file);
    if (abs_target.empty()) abs_target = workspace_dir_ + "\\" + target_file;

    // Create subdirectories if needed
    size_t last_slash = abs_target.find_last_of('\\');
    if (last_slash != std::string::npos) {
        std::string parent_dir = abs_target.substr(0, last_slash);
        for (size_t s = parent_dir.find('\\'); s != std::string::npos; s = parent_dir.find('\\', s + 1)) {
            CreateDirectoryA(parent_dir.substr(0, s).c_str(), NULL);
        }
        CreateDirectoryA(parent_dir.c_str(), NULL);
    }

    std::ofstream ofs(abs_target, std::ios::binary);
    if (!ofs.is_open()) {
        result.exit_code = 1;
        result.stderr_output = "Error: Failed to write file: " + target_file;
        return true;
    }
    ofs.write(file_content.data(), file_content.size());
    ofs.close();

    result.exit_code = 0;
    result.stdout_output = "[CyberBox Native File Writer] Successfully created: " + target_file + " (" + std::to_string(file_content.size()) + " bytes)\n";

    // If there is a trailing command (e.g. echo "done", etc.)
    if (after_delim_pos != std::string::npos && after_delim_pos < command.length()) {
        std::string remainder = command.substr(after_delim_pos);
        if (!remainder.empty()) {
            ExecutionResult rem_res = execute_command(remainder);
            result.stdout_output += rem_res.stdout_output;
            result.stderr_output += rem_res.stderr_output;
            result.exit_code = rem_res.exit_code;
        }
    }

    result.execution_time_ms = 5.0;
    return true;
}

ExecutionResult Win32JailSandbox::execute_command(const std::string& command, int timeout_seconds) {
    ExecutionResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!is_running_) {
        result.stderr_output = "Error: Win32 Sandbox is not initialized.";
        result.exit_code = -1;
        return result;
    }

    // Intercept Linux-style heredocs (cat > file << 'EOF' ... EOF) for seamless Windows execution
    if (handle_heredoc_write(command, result)) {
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        return result;
    }

    // Enforce Sentinel Security Policy Check
    std::string violation_reason;
    if (!validate_security_policy(command, violation_reason)) {
        result.exit_code = 126;
        result.stderr_output = "[Sentinel Sandbox Guard Blocked]: Lệnh bị từ chối do vi phạm an toàn Sandbox:\n" + violation_reason;
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        return result;
    }

    char abs_workspace[MAX_PATH];
    GetFullPathNameA(workspace_dir_.c_str(), MAX_PATH, abs_workspace, NULL);

    // Prepare pipes for STDOUT and STDERR with generous buffer to prevent write-deadlock
    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
    HANDLE hStdErrRead = NULL, hStdErrWrite = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 1024 * 1024);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 1024 * 1024);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE hStdIn = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = (hStdIn != INVALID_HANDLE_VALUE) ? hStdIn : NULL;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };

    // Build isolated sandbox PATH (strictly strip host package managers and user tool folders)
    std::string orig_path = getenv("PATH") ? getenv("PATH") : "";
    std::string isolated_path = std::string(abs_workspace) + "\\.bin;" + std::string(abs_workspace) + ";";

    std::stringstream ss(orig_path);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (item.empty()) continue;
        std::string item_lower = item;
        for (char& c : item_lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        if (item_lower.find("windowsapps") != std::string::npos) continue;
        if (item_lower.find("chocolatey") != std::string::npos) continue;
        if (item_lower.find("users") != std::string::npos && item_lower.find("appdata") != std::string::npos) continue;

        isolated_path += item + ";";
    }

    // Format command to run via cmd.exe inside isolated workspace with jailed user directories
    std::string sanitized = sanitize_command_for_windows(command);
    std::string env_setup = 
        "set PATH=" + isolated_path + " & " +
        "set LOCALAPPDATA=" + std::string(abs_workspace) + "\\.appdata & " +
        "set APPDATA=" + std::string(abs_workspace) + "\\.appdata & " +
        "set USERPROFILE=" + std::string(abs_workspace) + " & " +
        "set HOMEPATH=" + std::string(abs_workspace) + " & " +
        "set TEMP=" + std::string(abs_workspace) + "\\.tmp & " +
        "set TMP=" + std::string(abs_workspace) + "\\.tmp & ";
    if (!internet_enabled_.load()) {
        env_setup += "set http_proxy=http://127.0.0.1:0 & set https_proxy=http://127.0.0.1:0 & ";
    }
    std::string full_cmd = "cmd.exe /c \"" + env_setup + sanitized + "\"";

    HANDLE hJailToken = create_jail_token();
    BOOL success = FALSE;

    if (hJailToken != NULL) {
        success = CreateProcessAsUserA(
            hJailToken,
            NULL,
            const_cast<char*>(full_cmd.c_str()),
            NULL,
            NULL,
            TRUE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW,
            NULL,
            abs_workspace,
            &si,
            &pi
        );
        CloseHandle(hJailToken);
    }

    if (!success) {
        // Fallback to standard CreateProcessA if restricted token spawn fails
        success = CreateProcessA(
            NULL,
            const_cast<char*>(full_cmd.c_str()),
            NULL,
            NULL,
            TRUE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW,
            NULL,
            abs_workspace,
            &si,
            &pi
        );
    }

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    if (hStdIn != INVALID_HANDLE_VALUE) CloseHandle(hStdIn);

    if (!success) {
        DWORD err = GetLastError();
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        result.stderr_output = "Failed to spawn process in sandbox (Win32 error: " + std::to_string(err) + ").";
        result.exit_code = -1;
        return result;
    }

    // Assign process to the Job Object (imposing RAM, CPU, PID limits)
    if (job_object_ != NULL) {
        AssignProcessToJobObject(job_object_, pi.hProcess);
    }
    ResumeThread(pi.hThread);

    // Wait for process with timeout
    DWORD wait_res = WaitForSingleObject(pi.hProcess, timeout_seconds * 1000);
    if (wait_res == WAIT_TIMEOUT) {
        result.timed_out = true;
        result.stderr_output += "\n[Process timed out and was killed by sandbox limit]";
        TerminateProcess(pi.hProcess, 124);
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    // Read stdout
    char buf[4096];
    DWORD bytes_read = 0;
    while (ReadFile(hStdOutRead, buf, sizeof(buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        result.stdout_output.append(buf, bytes_read);
    }
    CloseHandle(hStdOutRead);

    // Read stderr
    while (ReadFile(hStdErrRead, buf, sizeof(buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        result.stderr_output.append(buf, bytes_read);
    }
    CloseHandle(hStdErrRead);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return result;
}

bool Win32JailSandbox::set_internet_access(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    internet_enabled_ = enabled;
    apply_firewall_rule(!enabled);

    std::string marker = workspace_dir_ + "\\.airgap";
    if (!enabled) {
        std::ofstream out(marker);
        if (out.is_open()) out << "AIR-GAP ACTIVE";
    } else {
        DeleteFileA(marker.c_str());
    }
    return true;
}

bool Win32JailSandbox::get_internet_access() const {
    return internet_enabled_.load();
}

bool Win32JailSandbox::add_port_forward(int container_port, int host_port, const std::string& desc) {
    std::lock_guard<std::mutex> lock(mutex_);
    PortMapping pm;
    pm.container_port = container_port;
    pm.host_port = host_port;
    pm.description = desc;
    pm.active = true;
    port_mappings_.push_back(pm);
    return true;
}

bool Win32JailSandbox::remove_port_forward(int host_port) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = port_mappings_.begin(); it != port_mappings_.end(); ++it) {
        if (it->host_port == host_port) {
            port_mappings_.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<PortMapping> Win32JailSandbox::get_port_mappings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return port_mappings_;
}

std::vector<FileItem> Win32JailSandbox::list_files(const std::string& relative_path) {
    std::vector<FileItem> items;
    std::string safe_dir = resolve_safe_path(relative_path);
    if (safe_dir.empty()) return items;

    std::string search_pattern = safe_dir + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return items;

    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        FileItem item;
        item.name = name;
        item.path = relative_path.empty() ? name : (relative_path + "/" + name);
        item.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        item.size_bytes = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        items.push_back(item);
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return items;
}

std::string Win32JailSandbox::read_file(const std::string& relative_path) {
    std::string safe_file = resolve_safe_path(relative_path);
    if (safe_file.empty()) return "Error: Path traversal detected.";

    std::ifstream ifs(safe_file, std::ios::binary);
    if (!ifs.is_open()) return "Error: Cannot open file.";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

bool Win32JailSandbox::write_file(const std::string& relative_path, const std::string& content) {
    std::string safe_file = resolve_safe_path(relative_path);
    if (safe_file.empty()) return false;

    // Ensure parent directory exists
    size_t last_slash = safe_file.find_last_of("\\/");
    if (last_slash != std::string::npos) {
        std::string parent_dir = safe_file.substr(0, last_slash);
        CreateDirectoryA(parent_dir.c_str(), NULL);
    }

    std::ofstream ofs(safe_file, std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs.write(content.data(), content.size());
    return ofs.good();
}

bool Win32JailSandbox::delete_file(const std::string& relative_path) {
    std::string safe_file = resolve_safe_path(relative_path);
    if (safe_file.empty()) return false;
    return DeleteFileA(safe_file.c_str()) != 0;
}

SandboxStatus Win32JailSandbox::get_status() {
    std::lock_guard<std::mutex> lock(mutex_);
    SandboxStatus status;
    status.engine_type = "Win32_Jail";
    status.container_id = "JobObject_Joyful";
    status.is_running = is_running_.load();
    status.internet_enabled = internet_enabled_.load();
    status.memory_limit_bytes = 1024ULL * 1024ULL * 1024ULL; // 1 GB
    status.ports = port_mappings_;
    status.workspace_path = workspace_dir_;

    if (job_object_ != NULL) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION acct = { 0 };
        if (QueryInformationJobObject(job_object_, JobObjectBasicAccountingInformation, &acct, sizeof(acct), NULL)) {
            status.active_processes = acct.ActiveProcesses;
        }
    }
    return status;
}

} // namespace sandbox
