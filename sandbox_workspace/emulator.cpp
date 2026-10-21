#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <conio.h>
#include <windows.h>

// ============================================================================
// CYBERBOX VIRTUAL MACHINE & EMULATOR (CBVM-16)
// A 16-bit Bytecode Architecture for Retro Games in Pure C++17
// ============================================================================

enum OpCode {
    OP_NOP    = 0x00,
    OP_LOAD   = 0x01, // LOAD Rd, imm
    OP_MOV    = 0x02, // MOV Rd, Rs
    OP_ADD    = 0x03, // ADD Rd, Rs/imm
    OP_SUB    = 0x04, // SUB Rd, Rs/imm
    OP_CMP    = 0x05, // CMP Rd, Rs/imm
    OP_JMP    = 0x06, // JMP label
    OP_JE     = 0x07, // JE label
    OP_JNE    = 0x08, // JNE label
    OP_JL     = 0x09, // JL label
    OP_JG     = 0x0A, // JG label
    OP_RAND   = 0x0B, // RAND Rd, max
    OP_DRAW   = 0x0C, // DRAW Rx, Ry, char
    OP_CLEAR  = 0x0D, // CLEAR
    OP_RENDER = 0x0E, // RENDER
    OP_INPUT  = 0x0F, // INPUT Rd
    OP_SLEEP  = 0x10, // SLEEP ms
    OP_PRINT  = 0x11, // PRINT Rx, Ry, "text"
    OP_HALT   = 0xFF
};

const int SCREEN_W = 36;
const int SCREEN_H = 14;

struct Instruction {
    OpCode op;
    int r1 = 0;
    int r2 = 0;
    int imm = 0;
    std::string str_val = "";
};

class CyberEmulator {
private:
    int regs[8] = { 0 }; // R0 - R7
    int pc = 0;
    int flag_cmp = 0; // -1: less, 0: equal, 1: greater
    bool running = true;
    int score = 0;

    char screen[SCREEN_H][SCREEN_W];
    std::vector<Instruction> program;
    std::vector<std::string> labels;

    void hide_cursor() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(hConsole, &cursorInfo);
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(hConsole, &cursorInfo);
    }

    void set_cursor(int x, int y) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD coord;
        coord.X = static_cast<SHORT>(x);
        coord.Y = static_cast<SHORT>(y);
        SetConsoleCursorPosition(hConsole, coord);
    }

public:
    CyberEmulator() {
        srand(static_cast<unsigned int>(time(nullptr)));
        clear_screen_buf();
    }

    void clear_screen_buf() {
        for (int y = 0; y < SCREEN_H; ++y) {
            for (int x = 0; x < SCREEN_W; ++x) {
                screen[y][x] = ' ';
            }
        }
    }

    void render_screen() {
        set_cursor(0, 0);
        std::cout << "+------------------------------------+\n";
        std::cout << "|   [CBVM-16] RETRO METEOR DODGER    |\n";
        std::cout << "+------------------------------------+\n";
        for (int y = 0; y < SCREEN_H; ++y) {
            std::cout << "|";
            for (int x = 0; x < SCREEN_W; ++x) {
                std::cout << screen[y][x];
            }
            std::cout << "|\n";
        }
        std::cout << "+------------------------------------+\n";
        std::cout << "| [A/<-] Trai  | [D/->] Phai | [Q] Thoat |\n";
        std::cout << "+------------------------------------+\n";
    }

    bool load_assembly(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[-] Error: Khong mo duoc tep ma nguon: " << filepath << "\n";
            return false;
        }

        std::string line;
        std::vector<std::pair<std::string, int>> label_table;
        std::vector<std::string> raw_lines;

        while (std::getline(file, line)) {
            // Strip comments
            size_t comment_pos = line.find(';');
            if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
            
            // Trim
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty()) continue;

            // Check label
            if (line.back() == ':') {
                std::string lbl = line.substr(0, line.length() - 1);
                label_table.push_back({ lbl, static_cast<int>(raw_lines.size()) });
            } else {
                raw_lines.push_back(line);
            }
        }

        auto parse_reg = [](const std::string& s) -> int {
            if (s.length() >= 2 && (s[0] == 'R' || s[0] == 'r')) {
                return s[1] - '0';
            }
            return 0;
        };

        auto resolve_target = [&](const std::string& t) -> int {
            for (const auto& entry : label_table) {
                if (entry.first == t) return entry.second;
            }
            try { return std::stoi(t); } catch (...) { return 0; }
        };

        program.clear();
        for (const auto& raw : raw_lines) {
            std::istringstream iss(raw);
            std::string mnemonic;
            iss >> mnemonic;

            Instruction inst;
            inst.op = OP_NOP;

            if (mnemonic == "LOAD") {
                std::string r, imm_str;
                iss >> r >> imm_str;
                inst.op = OP_LOAD;
                inst.r1 = parse_reg(r);
                inst.imm = std::stoi(imm_str);
            } else if (mnemonic == "MOV") {
                std::string rd, rs;
                iss >> rd >> rs;
                inst.op = OP_MOV;
                inst.r1 = parse_reg(rd);
                inst.r2 = parse_reg(rs);
            } else if (mnemonic == "ADD") {
                std::string rd, val;
                iss >> rd >> val;
                inst.op = OP_ADD;
                inst.r1 = parse_reg(rd);
                if (val[0] == 'R' || val[0] == 'r') {
                    inst.r2 = parse_reg(val);
                } else {
                    inst.imm = std::stoi(val);
                }
            } else if (mnemonic == "SUB") {
                std::string rd, val;
                iss >> rd >> val;
                inst.op = OP_SUB;
                inst.r1 = parse_reg(rd);
                if (val[0] == 'R' || val[0] == 'r') {
                    inst.r2 = parse_reg(val);
                } else {
                    inst.imm = std::stoi(val);
                }
            } else if (mnemonic == "CMP") {
                std::string rd, val;
                iss >> rd >> val;
                inst.op = OP_CMP;
                inst.r1 = parse_reg(rd);
                if (val[0] == 'R' || val[0] == 'r') {
                    inst.r2 = parse_reg(val);
                } else {
                    inst.imm = std::stoi(val);
                }
            } else if (mnemonic == "JMP") {
                std::string target; iss >> target;
                inst.op = OP_JMP;
                inst.imm = resolve_target(target);
            } else if (mnemonic == "JE") {
                std::string target; iss >> target;
                inst.op = OP_JE;
                inst.imm = resolve_target(target);
            } else if (mnemonic == "JNE") {
                std::string target; iss >> target;
                inst.op = OP_JNE;
                inst.imm = resolve_target(target);
            } else if (mnemonic == "JL") {
                std::string target; iss >> target;
                inst.op = OP_JL;
                inst.imm = resolve_target(target);
            } else if (mnemonic == "JG") {
                std::string target; iss >> target;
                inst.op = OP_JG;
                inst.imm = resolve_target(target);
            } else if (mnemonic == "RAND") {
                std::string rd, max_str;
                iss >> rd >> max_str;
                inst.op = OP_RAND;
                inst.r1 = parse_reg(rd);
                inst.imm = std::stoi(max_str);
            } else if (mnemonic == "DRAW") {
                std::string rx, ry, ch;
                iss >> rx >> ry >> ch;
                inst.op = OP_DRAW;
                inst.r1 = parse_reg(rx);
                inst.r2 = parse_reg(ry);
                inst.imm = ch.empty() ? ' ' : ch[0];
            } else if (mnemonic == "CLEAR") {
                inst.op = OP_CLEAR;
            } else if (mnemonic == "RENDER") {
                inst.op = OP_RENDER;
            } else if (mnemonic == "INPUT") {
                std::string rd; iss >> rd;
                inst.op = OP_INPUT;
                inst.r1 = parse_reg(rd);
            } else if (mnemonic == "SLEEP") {
                std::string ms; iss >> ms;
                inst.op = OP_SLEEP;
                inst.imm = std::stoi(ms);
            } else if (mnemonic == "HALT") {
                inst.op = OP_HALT;
            }

            program.push_back(inst);
        }

        std::cout << "[+] Emulator: Da nap va bien dich " << program.size() << " lenh bytecode thanh cong.\n";
        return true;
    }

    void run() {
        hide_cursor();
        system("cls");
        pc = 0;
        running = true;

        while (running && pc >= 0 && pc < static_cast<int>(program.size())) {
            const auto& inst = program[pc++];

            switch (inst.op) {
                case OP_NOP: break;

                case OP_LOAD:
                    regs[inst.r1] = inst.imm;
                    break;

                case OP_MOV:
                    regs[inst.r1] = regs[inst.r2];
                    break;

                case OP_ADD:
                    if (inst.r2 != 0) regs[inst.r1] += regs[inst.r2];
                    else regs[inst.r1] += inst.imm;
                    break;

                case OP_SUB:
                    if (inst.r2 != 0) regs[inst.r1] -= regs[inst.r2];
                    else regs[inst.r1] -= inst.imm;
                    break;

                case OP_CMP: {
                    int v2 = (inst.r2 != 0) ? regs[inst.r2] : inst.imm;
                    if (regs[inst.r1] < v2) flag_cmp = -1;
                    else if (regs[inst.r1] == v2) flag_cmp = 0;
                    else flag_cmp = 1;
                    break;
                }

                case OP_JMP: pc = inst.imm; break;
                case OP_JE:  if (flag_cmp == 0) pc = inst.imm; break;
                case OP_JNE: if (flag_cmp != 0) pc = inst.imm; break;
                case OP_JL:  if (flag_cmp < 0)  pc = inst.imm; break;
                case OP_JG:  if (flag_cmp > 0)  pc = inst.imm; break;

                case OP_RAND:
                    regs[inst.r1] = (inst.imm > 0) ? (rand() % inst.imm) : 0;
                    break;

                case OP_DRAW: {
                    int x = regs[inst.r1];
                    int y = regs[inst.r2];
                    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
                        screen[y][x] = static_cast<char>(inst.imm);
                    }
                    break;
                }

                case OP_CLEAR:
                    clear_screen_buf();
                    break;

                case OP_RENDER:
                    render_screen();
                    break;

                case OP_INPUT: {
                    int key = 0;
                    if (_kbhit()) {
                        key = _getch();
                        if (key == 224) { // Arrow keys prefix
                            int arrow = _getch();
                            if (arrow == 75) key = 'a'; // Left
                            if (arrow == 77) key = 'd'; // Right
                        }
                    }
                    regs[inst.r1] = key;
                    break;
                }

                case OP_SLEEP:
                    std::this_thread::sleep_for(std::chrono::milliseconds(inst.imm));
                    break;

                case OP_HALT:
                    running = false;
                    break;
            }
        }

        set_cursor(0, SCREEN_H + 8);
        std::cout << "\n========================================\n";
        std::cout << "  GAME OVER! Tro choi ket thuc.\n";
        std::cout << "  Emulator CBVM-16 dung tai PC: " << pc << "\n";
        std::cout << "========================================\n";
    }
};

int main(int argc, char* argv[]) {
    std::cout << "===============================================\n";
    std::cout << "   CyberBox AI - Pure C++17 Emulator (CBVM-16)  \n";
    std::cout << "===============================================\n";

    std::string game_file = "game.asm";
    if (argc > 1) {
        game_file = argv[1];
    }

    CyberEmulator emu;
    if (!emu.load_assembly(game_file)) {
        std::cerr << "[-] Usage: emulator.exe [duong_dan_game.asm]\n";
        return 1;
    }

    std::cout << "[*] Khoi dong Emulator va bat dau tro choi trong 2 giay...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    emu.run();
    return 0;
}
