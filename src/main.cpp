#include "native_gui.hpp"
#include <iostream>
#include <windows.h>

int main(int argc, char* argv[]) {
    // Enable UTF-8 encoding in Windows Console
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Set console title
    SetConsoleTitleW(L"CyberBox AI - Companion Execution & Sandbox Console (Pure C++)");

    std::cout << R"(
====================================================================
   ______      __               ____             ___    ____
  / ____/_  __/ /_  ___  _____ / __ )____  _  __/   |  /  _/
 / /   / / / / __ \/ _ \/ ___// __  / __ \| |/_/ /| |  / /  
/ /___/ /_/ / /_/ /  __/ /   / /_/ / /_/ />  </ ___ |_/ /   
\____/\__, /_.___/\___/_/   /_____/\____/_/|_/_/  |_/___/   
     /____/   100% Pure C++ Native Desktop & Sandbox Engine
====================================================================
)" << std::endl;

    std::cout << "[*] Khoi dong CyberBox AI - 100% Thuan C++17 Native (NO HTML, NO CSS)..." << std::endl;
    std::cout << "[+] Che do: Desktop GUI Win32 GDI+ (Giao dien Muse & Task Panel 1:1) + Console song song" << std::endl;
    std::cout << "[+] Da loai bo hoan toan: HTML, CSS, JavaScript, Trinh duyet Web, Node/Electron" << std::endl;
    std::cout << std::flush;

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Su dung:\n"
                      << "  cyberbox.exe              : Khoi chay che do Hybrid (GUI Win32 + Console)\n"
                      << "  cyberbox.exe --help       : Xem huong dan su dung\n"
                      << "  cyberbox.exe --version    : Xem thong tin phien ban\n"
                      << "  cyberbox.exe --test       : Kiem tra Sandbox Manager va cac module C++\n"
                      << std::endl;
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "CyberBox AI v2.5.0 (Pure C++17 Native Desktop Edition - NO HTML / NO CSS)" << std::endl;
            return 0;
        } else if (arg == "--test") {
            std::cout << "[*] Dang kiem tra khoi tao Sandbox Manager..." << std::endl;
            sandbox::SandboxManager smgr;
            bool ok = smgr.initialize("auto");
            std::cout << "[*] Sandbox Engine Init: " << (ok ? "THANH CONG" : "THAT BAI") << std::endl;
            auto status = smgr.get_status();
            std::cout << "[*] Engine Type: " << status.engine_type << std::endl;
            std::cout << "[*] Workspace: " << status.workspace_path << std::endl;
            auto res = smgr.get_engine()->execute_command("echo Hello from Pure C++ Native Sandbox!");
            std::cout << "[*] Test Sandbox Execute Exit Code: " << res.exit_code << std::endl;
            std::cout << "[*] Test Sandbox Output: " << res.stdout_output << std::endl;
            std::cout << "[OK] Tat ca kiem tra he thong hoan tat!" << std::endl;
            return 0;
        }
    }

    std::cout << "[+] Dang khoi tao Giao dien Do hoa Win32 GDI+ Immersive Dark Mode..." << std::endl;
    std::cout << std::flush;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    gui::NativeGui app;

    if (!app.create(hInstance, SW_SHOWNORMAL)) {
        std::cerr << "[-] Loi: Khong the khoi tao cua so Win32 GUI! Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "[*] Cua so Desktop GUI da hien thi. San sang nhan lenh!" << std::endl;
    std::cout << "[*] Moi thao tac Chat, Sandbox va Tranh luan se ghi nhat ky truc tiep tai day." << std::endl;
    std::cout << "--------------------------------------------------------------------" << std::endl;
    std::cout << std::flush;

    // Run Win32 Message Loop
    int exit_code = app.run_message_loop();

    std::cout << "\n[*] Dong ung dung CyberBox AI. Tam biet!" << std::endl;
    return exit_code;
}
