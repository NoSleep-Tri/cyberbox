#include "native_gui.hpp"
#include "simple_json.hpp"
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>

#define IDC_CHAT_INPUT       1101
#define ID_TIMER_ANIM        2001

#define IDC_BTN_ARENA_TOGGLE 1201
#define IDC_BTN_ARENA_TOPIC  1202
#define IDC_BTN_ARENA_EXPORT 1203
#define IDC_ARENA_INPUT      1204
#define IDC_BTN_ARENA_INTERJECT 1205
#define IDC_ARENA_TOPIC_INPUT 1206

#define IDC_CFG_PROVIDER     1301
#define IDC_CFG_BASE_URL     1302
#define IDC_CFG_API_KEY      1303
#define IDC_CFG_MODEL        1304
#define IDC_BTN_CFG_TEST     1305
#define IDC_BTN_CFG_SAVE     1306
#define IDC_CFG_ENGINE       1307
#define IDC_BTN_CFG_RESET_WS 1308

#define IDC_WS_LISTBOX       1401
#define IDC_BTN_WS_REFRESH   1402
#define IDC_WS_PREVIEW       1403
#define IDC_WS_CMD_INPUT     1404
#define IDC_BTN_WS_CMD_RUN   1405
#define IDC_BTN_WS_OPEN_FOLDER 1406
#define IDC_BTN_WS_SAVE_FILE   1407

#define WM_APP_CHAT_CHUNK    (WM_APP + 1)
#define WM_APP_CHAT_DONE     (WM_APP + 2)
#define WM_APP_ARENA_UPDATE  (WM_APP + 3)

namespace gui {

void NativeGui::log_console(const std::string& prefix, const std::string& msg, int color_attr) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole && hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(hConsole, static_cast<WORD>(color_attr));
        std::cout << prefix << " ";
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << msg << std::endl;
    } else {
        std::cout << prefix << " " << msg << std::endl;
    }
    std::cout << std::flush;
}

static std::string normalize_utf8(const std::string& str) {
    // Repair any 6-byte CESU-8 surrogate pair (ED A0..AF b3 ED B0..BF b6) into valid 4-byte UTF-8
    std::string out;
    out.reserve(str.size());
    size_t i = 0;
    while (i < str.size()) {
        if (i + 5 < str.size() &&
            static_cast<unsigned char>(str[i]) == 0xED &&
            (static_cast<unsigned char>(str[i + 1]) & 0xF0) == 0xA0 &&
            (static_cast<unsigned char>(str[i + 2]) & 0xC0) == 0x80 &&
            static_cast<unsigned char>(str[i + 3]) == 0xED &&
            (static_cast<unsigned char>(str[i + 4]) & 0xF0) == 0xB0 &&
            (static_cast<unsigned char>(str[i + 5]) & 0xC0) == 0x80) {
            unsigned int high = 0xD800 | ((static_cast<unsigned char>(str[i + 1]) & 0x0F) << 6) |
                                (static_cast<unsigned char>(str[i + 2]) & 0x3F);
            unsigned int low  = 0xDC00 | ((static_cast<unsigned char>(str[i + 4]) & 0x0F) << 6) |
                                (static_cast<unsigned char>(str[i + 5]) & 0x3F);
            unsigned int cp = 0x10000 + (((high - 0xD800) << 10) | (low - 0xDC00));
            out += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
            i += 6;
            continue;
        }
        out += str[i++];
    }
    return out;
}

static std::string utf8_safe_substr(const std::string& str, size_t max_bytes) {
    if (str.size() <= max_bytes) return str;
    size_t pos = 0;
    while (pos < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[pos]);
        size_t char_len = 1;
        if ((c & 0x80) == 0) char_len = 1;
        else if ((c & 0xE0) == 0xC0) char_len = 2;
        else if ((c & 0xF0) == 0xE0) char_len = 3;
        else if ((c & 0xF8) == 0xF0) char_len = 4;
        if (pos + char_len > max_bytes) break;
        pos += char_len;
    }
    return str.substr(0, pos);
}

static std::wstring s2ws(const std::string& raw_str) {
    if (raw_str.empty()) return L"";
    std::string str = normalize_utf8(raw_str);
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (size <= 0) return L"";
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}

static std::string ws2s(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

NativeGui::NativeGui() {
    // Start GDI+
    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&gdiplus_token_, &gsi, NULL);

    // Initialize Direct2D + DirectWrite for 100% Unicode, multi-script fallback & Color Emoji rendering
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory_);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&dwrite_factory_));
    if (d2d_factory_) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f, 96.0f,
            D2D1_RENDER_TARGET_USAGE_NONE,
            D2D1_FEATURE_LEVEL_DEFAULT
        );
        d2d_factory_->CreateDCRenderTarget(&props, &d2d_dc_target_);
        if (d2d_dc_target_) {
            d2d_dc_target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        }
    }

    // Load muse_avatar.png from exe directory first, then current directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dirPath = exePath;
    size_t lastSlash = dirPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        dirPath = dirPath.substr(0, lastSlash + 1);
    }
    std::wstring avatarPath = dirPath + L"muse_avatar.png";
    muse_avatar_img_ = Gdiplus::Image::FromFile(avatarPath.c_str());
    if (!muse_avatar_img_ || muse_avatar_img_->GetLastStatus() != Gdiplus::Ok) {
        delete muse_avatar_img_;
        muse_avatar_img_ = Gdiplus::Image::FromFile(L"muse_avatar.png");
    }

    sandbox_mgr_ = std::make_unique<sandbox::SandboxManager>();
    sandbox_mgr_->initialize("win32");
    ai_client_ = std::make_unique<ai::AiClient>();

    init_providers();
    init_arena_agents();
    load_config_from_disk();
    auto_load_active_arena();
    init_today_tasks();

    // Create GDI fonts
    font_ui_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_ui_bold_ = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_title_ = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_mono_ = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    font_small_ = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Brushes matching screenshots (#121316 canvas, #25262b capsule)
    bg_brush_ = CreateSolidBrush(RGB(18, 19, 22));        // #121316
    card_brush_ = CreateSolidBrush(RGB(28, 29, 34));     // #1c1d22
    input_brush_ = CreateSolidBrush(RGB(37, 38, 43));    // #25262b
    header_brush_ = CreateSolidBrush(RGB(18, 19, 22));   // #121316

    auto st = sandbox_mgr_->get_status();
    auto cfg = get_active_config();

    // Clean initial greeting from Muse reflecting only real active Sandbox capabilities
    GuiMessage welcome;
    welcome.sender = "muse";
    welcome.sender_name = "Muse";
    welcome.role = "";
    welcome.timestamp = get_current_time_str();
    welcome.text = "Xin chào! Tôi là **Muse** — đang chạy trên động cơ thực tế **`" + st.engine_type + "`** tại thư mục cô lập `" + st.workspace_path + "` "
                   "(Giới hạn RAM: **" + std::to_string(st.memory_limit_bytes / (1024 * 1024)) + " MB**, Token `Low-Integrity S-1-16-4096`, tối đa `32 tiến trình`, khóa UI/Clipboard Host).";
    chat_messages_.push_back(welcome);

    timeline_logs_.push_back("[" + get_current_time_str() + "] Engine: " + st.engine_type + " (" + st.workspace_path + ")");
    timeline_logs_.push_back("[" + get_current_time_str() + "] JobObject: RAM " + std::to_string(st.memory_limit_bytes / (1024 * 1024)) + "MB, Max 32 PIDs");
    timeline_logs_.push_back("[" + get_current_time_str() + "] Token: Low-Integrity (S-1-16-4096)");
    timeline_logs_.push_back("[" + get_current_time_str() + "] AI: " + cfg.id + " (" + cfg.model + ")");
}

NativeGui::~NativeGui() {
    arena_running_ = false;
    if (arena_thread_.joinable()) {
        arena_thread_.join();
    }

    if (muse_avatar_img_) {
        delete muse_avatar_img_;
        muse_avatar_img_ = NULL;
    }

    for (auto& kv : text_format_cache_) {
        if (kv.second) kv.second->Release();
    }
    text_format_cache_.clear();

    if (d2d_avatar_bmp_) {
        d2d_avatar_bmp_->Release();
        d2d_avatar_bmp_ = NULL;
    }
    if (d2d_solid_brush_) {
        d2d_solid_brush_->Release();
        d2d_solid_brush_ = NULL;
    }
    if (d2d_stroke_round_) {
        d2d_stroke_round_->Release();
        d2d_stroke_round_ = NULL;
    }
    if (d2d_dc_target_) {
        d2d_dc_target_->Release();
        d2d_dc_target_ = NULL;
    }
    if (dwrite_factory_) {
        dwrite_factory_->Release();
        dwrite_factory_ = NULL;
    }
    if (d2d_factory_) {
        d2d_factory_->Release();
        d2d_factory_ = NULL;
    }

    if (backbuffer_dc_) {
        if (backbuffer_old_bmp_) SelectObject(backbuffer_dc_, backbuffer_old_bmp_);
        if (backbuffer_bmp_) DeleteObject(backbuffer_bmp_);
        DeleteDC(backbuffer_dc_);
        backbuffer_dc_ = NULL;
        backbuffer_bmp_ = NULL;
    }

    if (font_ui_) DeleteObject(font_ui_);
    if (font_ui_bold_) DeleteObject(font_ui_bold_);
    if (font_title_) DeleteObject(font_title_);
    if (font_mono_) DeleteObject(font_mono_);
    if (font_small_) DeleteObject(font_small_);

    if (bg_brush_) DeleteObject(bg_brush_);
    if (card_brush_) DeleteObject(card_brush_);
    if (input_brush_) DeleteObject(input_brush_);
    if (header_brush_) DeleteObject(header_brush_);

    timeEndPeriod(1);

    if (gdiplus_token_) {
        Gdiplus::GdiplusShutdown(gdiplus_token_);
    }
}

void NativeGui::init_providers() {
    std::lock_guard<std::mutex> lock(providers_mutex_);
    providers_.clear();

    providers_.push_back({ "ollama", "Ollama (Local / Tự lưu trữ)", "http://127.0.0.1:11434/v1", "", "maternion/minicpm5-heretic:2b-q4_K_M" });
    providers_.push_back({ "openai", "OpenAI (GPT-4o, GPT-4o-mini)", "https://api.openai.com/v1", "", "gpt-4o-mini" });
    providers_.push_back({ "deepseek", "DeepSeek (V3, R1 Reasoning)", "https://api.deepseek.com/v1", "", "deepseek-chat" });
    providers_.push_back({ "groq", "Groq Cloud (Llama 3.3, Mixtral)", "https://api.groq.com/openai/v1", "", "llama-3.3-70b-versatile" });
    providers_.push_back({ "openrouter", "OpenRouter (Mọi mô hình)", "https://openrouter.ai/api/v1", "", "meta-llama/llama-3.3-70b-instruct" });
    providers_.push_back({ "gemini", "Google Gemini (Tương thích OpenAI)", "https://generativelanguage.googleapis.com/v1beta/openai", "", "gemini-1.5-flash" });
    providers_.push_back({ "custom", "Tùy chỉnh (Endpoint riêng / vLLM)", "http://localhost:8000/v1", "", "default" });
}

struct SwarmAgentTemplate {
    std::string id;
    std::string name;
    COLORREF color;
};

static const SwarmAgentTemplate SWARM_POOL[12] = {
    { "nexus", "Nexus", RGB(168, 85, 247) },       // Purple Neon
    { "codecraft", "CodeCraft", RGB(6, 182, 212) }, // Cyan
    { "nova", "Nova", RGB(16, 185, 129) },         // Emerald
    { "specter", "Specter", RGB(249, 115, 22) },   // Orange
    { "vortex", "Vortex", RGB(236, 72, 153) },     // Pink
    { "aegis", "Aegis", RGB(245, 158, 11) },       // Amber
    { "titan", "Titan", RGB(239, 68, 68) },        // Red
    { "oracle", "Oracle", RGB(99, 102, 241) },     // Indigo
    { "cipher", "Cipher", RGB(20, 184, 166) },     // Teal
    { "eclipse", "Eclipse", RGB(147, 51, 234) },   // Violet
    { "blaze", "Blaze", RGB(244, 63, 94) },        // Rose
    { "zero", "Zero", RGB(56, 189, 248) }          // Sky Cyber
};

void NativeGui::init_arena_agents() {
    set_swarm_size(swarm_size_ > 0 ? swarm_size_ : 3);
}

void NativeGui::set_swarm_size(int size) {
    if (size < 2) size = 2;
    if (size > 12) size = 12;
    swarm_size_ = size;
    {
        std::lock_guard<std::mutex> lock(arena_mutex_);
        arena_agents_.clear();
        for (int i = 0; i < swarm_size_; ++i) {
            const auto& t = SWARM_POOL[i % 12];
            AgentInfo a;
            a.id = t.id;
            a.name = t.name;
            a.role = ""; // Absolutely ZERO fixed or assigned roles!
            a.color = t.color;
            arena_agents_.push_back(a);
        }
    }
    log_console("[SWARM]", "Quy mô bầy đàn đã cập nhật: " + std::to_string(swarm_size_) + " Agent (Tự chủ hoàn toàn).", 11);
    if (hwnd_main_) InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::add_swarm_agent() {
    set_swarm_size(swarm_size_ + 1);
}

void NativeGui::remove_swarm_agent() {
    set_swarm_size(swarm_size_ - 1);
}

void NativeGui::reset_swarm_roles() {
    std::lock_guard<std::mutex> lock(arena_mutex_);
    for (auto& a : arena_agents_) {
        a.role = "";
    }
    log_console("[SWARM]", "Đã làm mới bầy đàn: Tất cả Agent sẵn sàng thảo luận tự do!", 14);
    if (hwnd_main_) InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::cycle_arena_speed() {
    int cur = arena_delay_ms_.load();
    if (cur >= 3000) arena_delay_ms_ = 800;       // Siêu tốc 800ms
    else if (cur <= 1000) arena_delay_ms_ = 1800; // Tiêu chuẩn 1.8s
    else arena_delay_ms_ = 3000;                  // Chậm 3.0s
    log_console("[ARENA]", "Đã đổi nhịp độ tranh luận bầy đàn: " + std::to_string(arena_delay_ms_.load()) + " ms", 14);
    if (hwnd_main_) InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::cycle_arena_context_window() {
    if (arena_context_window_ == 3) arena_context_window_ = 5;
    else if (arena_context_window_ == 5) arena_context_window_ = 10;
    else if (arena_context_window_ == 10) arena_context_window_ = 0; // Vô hạn
    else if (arena_context_window_ == 0) arena_context_window_ = 1;
    else arena_context_window_ = 3;

    std::string wStr = (arena_context_window_ == 0) ? "Vô hạn (Toàn bộ)" : (std::to_string(arena_context_window_) + " lượt");
    log_console("[ARENA]", "Đã tùy chỉnh Context Window tranh luận: " + wStr, 14);
    if (hwnd_main_) InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::apply_new_topic(const std::string& new_topic) {
    if (new_topic.empty()) return;
    arena_topic_ = new_topic;
    reset_swarm_roles();

    GuiMessage sysMsg;
    sysMsg.sender = "sys";
    sysMsg.sender_name = "Hệ thống";
    sysMsg.role = "";
    sysMsg.timestamp = get_current_time_str();
    sysMsg.color = RGB(168, 85, 247);
    sysMsg.text = "🎯 [CHỦ ĐỀ TRANH LUẬN MỚI]: " + arena_topic_ + "\n\nBầy agent (" + std::to_string(arena_agents_.size()) + " agent) đã kích hoạt chu trình tranh luận tự do theo chủ đề mới!";

    {
        std::lock_guard<std::mutex> lock(arena_mutex_);
        arena_messages_.push_back(sysMsg);
    }

    show_topic_modal_ = false;
    ShowWindow(h_arena_topic_input_, SW_HIDE);
    auto_scroll_arena_ = true;
    auto_save_active_arena();
    InvalidateRect(hwnd_main_, NULL, FALSE);
    log_console("[ARENA]", "Chủ đề tranh luận đã đổi thành: " + arena_topic_, 10);
}

void NativeGui::init_today_tasks() {
    std::string now_str = get_current_time_str();
    auto st = sandbox_mgr_->get_status();
    auto cfg = get_active_config();
    today_tasks_ = {
        {
            "Engine: " + st.engine_type + " (" + st.workspace_path + ")",
            st.engine_type == "Win32_Jail"
                ? "Win32 Job Object (1024 MB RAM, tối đa 32 tiến trình, khóa Clipboard/Desktop Host)"
                : ("Docker Container ID: " + st.container_id + " (2048 MB RAM, 2 CPU, 128 PIDs)"),
            now_str,
            true
        },
        {
            st.engine_type == "Win32_Jail" ? "Low-Integrity Token (S-1-16-4096)" : "Docker Named Pipe Isolation",
            st.engine_type == "Win32_Jail"
                ? "Tước quyền Admin (LUA_TOKEN), gắn ACL S:(ML;;NW;;;LW), chặn vượt đường dẫn .."
                : "Giao tiếp qua \\\\.\\pipe\\docker_engine, cô lập /workspace",
            now_str,
            true
        },
        {
            "Mạng: " + std::string(st.internet_enabled ? "ONLINE" : "AIR-GAP") + " | AI: " + cfg.id,
            "Model đang chọn: " + cfg.model + " (" + cfg.base_url + ")",
            now_str,
            true
        }
    };
}

void NativeGui::copy_to_clipboard(const std::string& text) {
    if (!OpenClipboard(hwnd_main_)) return;
    EmptyClipboard();
    std::wstring wstr = s2ws(text);
    size_t bytes = (wstr.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* ptr = GlobalLock(hMem);
        if (ptr) {
            memcpy(ptr, wstr.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
}

ai::AiConfig NativeGui::get_active_config() {
    std::lock_guard<std::mutex> lock(providers_mutex_);
    for (const auto& p : providers_) {
        if (p.id == active_provider_id_) return p;
    }
    return ai_client_->get_config();
}

ai::AiConfig NativeGui::get_config_for_agent(const AgentInfo& agent) {
    std::lock_guard<std::mutex> lock(providers_mutex_);
    ai::AiConfig cfg = ai_client_->get_config();
    std::string pid = agent.provider_id.empty() ? active_provider_id_ : agent.provider_id;
    for (const auto& p : providers_) {
        if (p.id == pid) {
            cfg = p;
            break;
        }
    }
    if (!agent.custom_model.empty()) {
        cfg.model = agent.custom_model;
    }
    return cfg;
}

std::string NativeGui::read_file_content(const std::string& path, const std::string& fallback) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return fallback;
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string s = buffer.str();
    return s.empty() ? fallback : s;
}

void NativeGui::write_file_content(const std::string& path, const std::string& content) {
    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << content;
    }
}

std::string NativeGui::get_current_time_str() {
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%I:%M %p", localtime(&now));
    return buf;
}

void NativeGui::load_config_from_disk() {
    std::string content = read_file_content("config.json", "");
    if (content.empty()) return;
    try {
        json::Value root = json::parse(content);
        if (root.contains("active_provider")) {
            active_provider_id_ = root["active_provider"].as_string();
        }
        if (root.contains("providers") && root["providers"].is_array()) {
            std::lock_guard<std::mutex> lock(providers_mutex_);
            for (size_t i = 0; i < root["providers"].size(); ++i) {
                const auto& item = root["providers"][i];
                std::string pid = item["id"].as_string();
                for (auto& p : providers_) {
                    if (p.id == pid) {
                        if (item.contains("base_url")) p.base_url = item["base_url"].as_string();
                        if (item.contains("api_key")) p.api_key = item["api_key"].as_string();
                        if (item.contains("model")) p.model = item["model"].as_string();
                        break;
                    }
                }
            }
        }
        ai_client_->set_config(get_active_config());
    } catch (...) {}
}

void NativeGui::save_config_to_disk() {
    std::lock_guard<std::mutex> lock(providers_mutex_);
    json::Value root = json::Value::object();
    root["active_provider"] = active_provider_id_;

    json::Value arr = json::Value::array();
    for (const auto& p : providers_) {
        json::Value item = json::Value::object();
        item["id"] = p.id;
        item["name"] = p.name;
        item["base_url"] = p.base_url;
        item["api_key"] = p.api_key;
        item["model"] = p.model;
        arr.push_back(item);
    }
    root["providers"] = arr;
    write_file_content("config.json", root.dump());
}

static std::string truncate_sandbox_out(const std::string& s, size_t max_len = 3200) {
    if (s.size() <= max_len) return s;
    size_t half = max_len / 2;
    return utf8_safe_substr(s, half) +
           "\n...[Đã thu gọn " + std::to_string(s.size() - max_len) + " bytes output trung gian]...\n" +
           s.substr(s.size() - half);
}

std::string NativeGui::build_system_prompt() {
    return build_system_prompt("");
}

std::string NativeGui::build_system_prompt(const std::string& agent_name) {
    auto st = sandbox_mgr_->get_status();
    std::string prompt;
    if (agent_name.empty()) {
        prompt = "Bạn là Muse, trợ lý AI hoạt động bên trong Sandbox thực thi của ứng dụng CyberBox C++ Native.\n";
    } else {
        prompt = "Bạn là " + agent_name + ", thành viên tham gia thảo luận độc lập bên trong Sandbox thực thi của ứng dụng CyberBox C++ Native.\n"
                 "Danh xưng duy nhất của bạn là " + agent_name + ". Tuyệt đối không xưng hô là Muse hay mượn tên của các thành viên khác.\n";
    }
    prompt += "Đặc điểm thật và đang hoạt động của Sandbox hiện tại:\n";
    if (st.engine_type == "Win32_Jail") {
        prompt += "- Engine đang chạy: Win32_Jail (Win32 Job Object + Restricted Low-Integrity Token).\n";
        prompt += "- Thư mục làm việc cô lập: " + st.workspace_path + " (kiểm tra chống Path Traversal '..' qua GetFullPathNameA).\n";
        prompt += "- Token tiến trình: CreateRestrictedToken (DISABLE_MAX_PRIVILEGE | LUA_TOKEN) với nhãn Low Mandatory Integrity (S-1-16-4096) và ACL S:(ML;;NW;;;LW).\n";
        prompt += "- Giới hạn Win32 Job Object: RAM tối đa 1024 MB/tiến trình, tối đa 32 tiến trình đồng thời, KILL_ON_JOB_CLOSE, chặn UI Host (Clipboard, Desktop, Global Atoms, Handles).\n";
        prompt += "- Môi trường & Shim lệnh: USERPROFILE/APPDATA/TEMP trỏ vào .appdata và .tmp trong workspace; hỗ trợ shim lệnh (.bin: ls, cat, grep, pwd, mkdir, rm, python, g++/cl, web_search, web_read); chặn winget, choco, msiexec cài đặt ra máy thật.\n";
    } else {
        prompt += "- Engine đang chạy: Docker Container (ubuntu:22.04, ID: " + st.container_id + ") qua Named Pipe \\\\.\\pipe\\docker_engine.\n";
        prompt += "- Thư mục làm việc: /workspace (Giới hạn 2048 MB RAM, 2 CPU cores, 128 PIDs).\n";
    }
    prompt += std::string("- Trạng thái mạng hiện tại: ") + (st.internet_enabled ? "ONLINE (Cho phép kết nối)" : "AIR-GAP (Ngắt mạng, chặn http/https/git clone, proxy 127.0.0.1:0 & Firewall)") + ".\n";
    if (st.internet_enabled) {
        prompt += "- Duyệt & tìm kiếm web (Chế độ ONLINE): Bạn có thể chạy công cụ duyệt web an toàn bên trong sandbox bằng lệnh:\n"
                  "  `web_search <từ khóa>` (tìm kiếm thông tin trực tuyến)\n"
                  "  `web_read <url>` (đọc nội dung bài viết/trang web thành văn bản markdown)\n";
    } else {
        prompt += "- Chế độ AIR-GAP đang bật: Mọi nỗ lực truy cập Internet (kể cả web_search, web_read, curl, pip) đều bị ngắt và bảo vệ tuyệt đối.\n";
    }
    prompt += "- Không gian Canvas & Tệp đa nhiệm: Bạn có thể tạo, chỉnh sửa tệp trong workspace bằng bash (ví dụ `cat << 'EOF' > ten_tep.py ... EOF`). Người dùng có thể xem và chỉnh sửa trực tiếp mã nguồn trên Canvas Editor (Tab Workspace) hoặc mở thư mục ngoài máy thật.\n";
    prompt += "Chỉ nêu đúng các đặc điểm thật đang có ở trên khi được hỏi về Sandbox. Để chạy lệnh trong sandbox, đặt lệnh trong khối:\n"
              "```bash\n<lệnh cần chạy>\n```\n"
              "Trả lời rõ ràng, ngắn gọn bằng tiếng Việt.";
    return prompt;
}

std::vector<TextPart> NativeGui::parse_markdown_blocks(const std::string& text) {
    std::vector<TextPart> parts;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t code_start = text.find("```", pos);
        if (code_start == std::string::npos) {
            std::string rem = text.substr(pos);
            if (!rem.empty()) parts.push_back({ false, "", rem });
            break;
        }
        if (code_start > pos) {
            parts.push_back({ false, "", text.substr(pos, code_start - pos) });
        }
        size_t lang_start = code_start + 3;
        size_t lang_end = text.find('\n', lang_start);
        std::string lang = "code";
        size_t content_start = lang_start;
        if (lang_end != std::string::npos && lang_end - lang_start < 20) {
            lang = text.substr(lang_start, lang_end - lang_start);
            lang.erase(0, lang.find_first_not_of(" \r\n\t"));
            lang.erase(lang.find_last_not_of(" \r\n\t") + 1);
            if (lang.empty()) lang = "code";
            content_start = lang_end + 1;
        }
        size_t code_end = text.find("```", content_start);
        if (code_end == std::string::npos) {
            parts.push_back({ true, lang, text.substr(content_start) });
            break;
        } else {
            std::string c = text.substr(content_start, code_end - content_start);
            while (!c.empty() && (c.back() == '\n' || c.back() == '\r')) c.pop_back();
            parts.push_back({ true, lang, c });
            pos = code_end + 3;
            while (pos < text.size() && (text[pos] == '\n' || text[pos] == '\r')) pos++;
        }
    }
    return parts;
}

LRESULT CALLBACK NativeGui::WndProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    NativeGui* pThis = NULL;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (NativeGui*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        if (pThis) pThis->hwnd_main_ = hwnd;
    } else {
        pThis = (NativeGui*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (pThis) {
        return pThis->handle_message(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

bool NativeGui::create(HINSTANCE hInstance, int nCmdShow) {
    hinstance_ = hInstance;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = NativeGui::WndProcThunk;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CyberBoxAiMainWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = bg_brush_;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1320;
    int winH = 840;
    int startX = (screenW - winW) / 2;
    int startY = (screenH - winH) / 2;

    hwnd_main_ = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Muse - CyberBox AI Native C++",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        startX, startY, winW, winH,
        NULL, NULL, hInstance, this
    );

    if (!hwnd_main_) return false;

    // Enable Immersive Dark Mode for Windows 10/11
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd_main_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDarkMode, sizeof(useDarkMode));

    // Enable Modern Windows 11 Rounded Corners
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd_main_, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));

    // Extend Frame into client area so DWM renders its native drop shadow around borderless window
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd_main_, &margins);

    // Trigger WM_NCCALCSIZE to strip standard title bar
    SetWindowPos(hwnd_main_, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    create_child_controls();
    init_ui_engine();

    // Initialize DirectX 11 Hardware GPU Engine (Flip Model)
    RECT initClientRc;
    GetClientRect(hwnd_main_, &initClientRc);
    int initW = max(1, (int)(initClientRc.right - initClientRc.left));
    int initH = max(1, (int)(initClientRc.bottom - initClientRc.top));
    if (dx11_.initialize(hwnd_main_, initW, initH)) {
        if (dx11_renderer_.initialize(dx11_)) {
            use_dx11_ = true;
            if (d2d_solid_brush_) { d2d_solid_brush_->Release(); d2d_solid_brush_ = NULL; }
            if (d2d_avatar_bmp_) { d2d_avatar_bmp_->Release(); d2d_avatar_bmp_ = NULL; }
            if (d2d_stroke_round_) { d2d_stroke_round_->Release(); d2d_stroke_round_ = NULL; }
            log_console("[DIRECTX 11]", "Hardware GPU Renderer Active (Direct3D 11 + Flip-Model DXGI)!", 10);
            log_console("[GPU]", "Adapter: " + dx11_.get_gpu_name(), 11);
        }
    }

    security::AntiScreenshot::instance().initialize(hwnd_main_, security::AntiCaptureLevel::DISABLED);
    security::AntiScreenshot::instance().set_on_status_changed([this](const security::AntiCaptureStatus&) {
        InvalidateRect(hwnd_main_, NULL, FALSE);
    });

    switch_tab(ActiveTab::CHAT);
    RECT initialRc;
    GetClientRect(hwnd_main_, &initialRc);
    dock_indicator_target_y_ = get_dock_icon_y(active_dock_icon_, (float)initialRc.bottom);
    dock_indicator_y_ = dock_indicator_target_y_;
    tab_transition_progress_ = 1.0f;
    right_panel_cur_w_ = 310.0f;
    right_panel_target_w_ = 310.0f;
    // 120 FPS High-Resolution Multimedia Timer & QPC Initialization
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&perf_freq_);
    QueryPerformanceCounter(&last_anim_time_);

    // Start 120 FPS ultra-smooth animation timer (8ms ≈ 120-125 FPS)
    SetTimer(hwnd_main_, ID_TIMER_ANIM, 8, NULL);

    ShowWindow(hwnd_main_, nCmdShow);
    UpdateWindow(hwnd_main_);

    log_console("[GUI]", "Cua so Desktop Native Win32 GDI+ khoi tao thanh cong (1:1 Muse Design + Stream & Animation).", 10);
    log_console("[SYS]", "Hoat dong song song: Giao dien do hoa Win32 + Man hinh Console Sandbox.", 11);

    return true;
}

void NativeGui::create_child_controls() {
    // Chat Input: Borderless EDIT control inside the bottom capsule
    h_chat_input_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
        160, 752, 560, 24, hwnd_main_, (HMENU)IDC_CHAT_INPUT, hinstance_, NULL);

    SendMessageW(h_chat_input_, 0x1501 /* EM_SETCUEBANNER */, TRUE, (LPARAM)L"Nhắn tin");

    // Arena Controls
    h_arena_interject_input_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL | ES_LEFT,
        64, 750, 700, 32, hwnd_main_, (HMENU)IDC_ARENA_INPUT, hinstance_, NULL);
    SendMessageW(h_arena_interject_input_, 0x1501, TRUE, (LPARAM)L"Nhập ý kiến can thiệp vào phòng Arena...");

    h_arena_topic_input_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        100, 100, 500, 56, hwnd_main_, (HMENU)IDC_ARENA_TOPIC_INPUT, hinstance_, NULL);
    SendMessageW(h_arena_topic_input_, 0x1501, TRUE, (LPARAM)L"Nhập chủ đề tranh luận mới...");

    // Settings Controls
    h_cfg_provider_combo_ = CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        106, 150, 520, 200, hwnd_main_, (HMENU)IDC_CFG_PROVIDER, hinstance_, NULL);
    h_cfg_base_url_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL,
        106, 225, 520, 32, hwnd_main_, (HMENU)IDC_CFG_BASE_URL, hinstance_, NULL);
    h_cfg_api_key_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL | ES_PASSWORD,
        106, 300, 520, 32, hwnd_main_, (HMENU)IDC_CFG_API_KEY, hinstance_, NULL);
    h_cfg_model_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL,
        106, 375, 520, 32, hwnd_main_, (HMENU)IDC_CFG_MODEL, hinstance_, NULL);
    h_cfg_engine_combo_ = CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        106, 450, 520, 120, hwnd_main_, (HMENU)IDC_CFG_ENGINE, hinstance_, NULL);

    for (const auto& p : providers_) {
        SendMessageW(h_cfg_provider_combo_, CB_ADDSTRING, 0, (LPARAM)s2ws(p.name).c_str());
    }
    SendMessageW(h_cfg_provider_combo_, CB_SETCURSEL, 0, 0);
    SendMessageW(h_cfg_provider_combo_, 0x1701 /* CB_SETMINVISIBLE */, 6, 0);

    SendMessageW(h_cfg_engine_combo_, CB_ADDSTRING, 0, (LPARAM)L"Win32 Job Object Jail (Native Windows)");
    SendMessageW(h_cfg_engine_combo_, CB_ADDSTRING, 0, (LPARAM)L"Docker Container (Linux Root)");
    SendMessageW(h_cfg_engine_combo_, CB_SETCURSEL, 0, 0);
    SendMessageW(h_cfg_engine_combo_, 0x1701 /* CB_SETMINVISIBLE */, 4, 0);

    // Workspace Controls
    h_ws_listbox_ = CreateWindowExW(0, L"LISTBOX", L"",
        WS_CHILD | LBS_NOTIFY | WS_VSCROLL,
        68, 96, 300, 560, hwnd_main_, (HMENU)IDC_WS_LISTBOX, hinstance_, NULL);
    h_ws_refresh_btn_ = CreateWindowW(L"BUTTON", L"Làm mới tệp", WS_CHILD | BS_OWNERDRAW,
        68, 670, 130, 34, hwnd_main_, (HMENU)IDC_BTN_WS_REFRESH, hinstance_, NULL);
    h_ws_open_folder_btn_ = CreateWindowW(L"BUTTON", L"Mở thư mục", WS_CHILD | BS_OWNERDRAW,
        208, 670, 150, 34, hwnd_main_, (HMENU)IDC_BTN_WS_OPEN_FOLDER, hinstance_, NULL);

    h_ws_preview_edit_ = CreateWindowExW(0, L"EDIT", L"Chọn một tệp từ danh sách bên trái để xem hoặc chỉnh sửa trực tiếp (Canvas mode).",
        WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL,
        384, 96, 520, 560, hwnd_main_, (HMENU)IDC_WS_PREVIEW, hinstance_, NULL);

    h_ws_save_btn_ = CreateWindowW(L"BUTTON", L"Lưu tệp", WS_CHILD | BS_OWNERDRAW,
        384, 670, 110, 34, hwnd_main_, (HMENU)IDC_BTN_WS_SAVE_FILE, hinstance_, NULL);
    h_ws_cmd_input_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL,
        504, 670, 300, 34, hwnd_main_, (HMENU)IDC_WS_CMD_INPUT, hinstance_, NULL);
    SendMessageW(h_ws_cmd_input_, 0x1501, TRUE, (LPARAM)L"Nhập lệnh shell để chạy trong Sandbox...");
    h_ws_cmd_run_btn_ = CreateWindowW(L"BUTTON", L"Chạy lệnh", WS_CHILD | BS_OWNERDRAW,
        814, 670, 100, 34, hwnd_main_, (HMENU)IDC_BTN_WS_CMD_RUN, hinstance_, NULL);

    // Apply Fonts
    SendMessageW(h_chat_input_, WM_SETFONT, (WPARAM)font_ui_, TRUE);
    SendMessageW(h_arena_interject_input_, WM_SETFONT, (WPARAM)font_ui_, TRUE);
    SendMessageW(h_arena_topic_input_, WM_SETFONT, (WPARAM)font_ui_, TRUE);
    SendMessageW(h_cfg_base_url_, WM_SETFONT, (WPARAM)font_mono_, TRUE);
    SendMessageW(h_cfg_api_key_, WM_SETFONT, (WPARAM)font_mono_, TRUE);
    SendMessageW(h_cfg_model_, WM_SETFONT, (WPARAM)font_mono_, TRUE);
    SendMessageW(h_cfg_provider_combo_, WM_SETFONT, (WPARAM)font_ui_, TRUE);
    SendMessageW(h_cfg_engine_combo_, WM_SETFONT, (WPARAM)font_ui_, TRUE);
    SendMessageW(h_ws_listbox_, WM_SETFONT, (WPARAM)font_ui_, TRUE);
    SendMessageW(h_ws_preview_edit_, WM_SETFONT, (WPARAM)font_mono_, TRUE);
    SendMessageW(h_ws_cmd_input_, WM_SETFONT, (WPARAM)font_mono_, TRUE);

    on_settings_provider_changed();
    on_ws_refresh();
}

void NativeGui::layout_controls(int width, int height) {
    int chat_left = 48;
    int right_panel_w = (int)right_panel_cur_w_;
    int chat_right = width - right_panel_w;
    int chat_w = chat_right - chat_left;

    // Chat bottom capsule input (matches Screenshot 3)
    int capsule_w = min(chat_w - 80, 620);
    int capsule_x = chat_left + (chat_w - capsule_w) / 2;
    int capsule_y = height - 62;

    SetWindowPos(h_chat_input_, NULL, capsule_x + 42, capsule_y + 11, capsule_w - 84, 22, SWP_NOZORDER);

    // Arena controls
    int arenaBarX = chat_left + 20;
    int arenaBarW = chat_w - 40;
    int arenaBarY = height - 56;
    int btnW = 110;
    int inputX = arenaBarX + 112;
    int inputW = arenaBarW - 112 - btnW - 16;
    SetWindowPos(h_arena_interject_input_, NULL, inputX, arenaBarY + 10, inputW, 26, SWP_NOZORDER);

    // Modal Topic Input position
    if (show_topic_modal_) {
        float rW = (float)width;
        float rH = (float)height;
        float cardW = min(660.0f, rW - 40.0f);
        float cardH = 430.0f;
        float cardX = (rW - cardW) * 0.5f;
        float cardY = max(36.0f, (rH - cardH) * 0.5f);
        SetWindowPos(h_arena_topic_input_, NULL, (int)cardX + 28, (int)cardY + 104, (int)cardW - 56, 56, SWP_SHOWWINDOW);
    } else if (h_arena_topic_input_) {
        ShowWindow(h_arena_topic_input_, SW_HIDE);
    }

    // Settings controls (centered horizontally with card)
    int cardW_i = min(chat_w - 64, 720);
    int cardX_i = chat_left + (chat_w - cardW_i) / 2;
    int cfgX = cardX_i + 26;
    int cfgW = cardW_i - 52;
    int cfgCardY = 64;
    SetWindowPos(h_cfg_provider_combo_, NULL, cfgX, cfgCardY + 84, cfgW, 200, SWP_NOZORDER);
    SetWindowPos(h_cfg_base_url_, NULL, cfgX, cfgCardY + 154, cfgW, 30, SWP_NOZORDER);
    SetWindowPos(h_cfg_api_key_, NULL, cfgX, cfgCardY + 224, cfgW, 30, SWP_NOZORDER);
    SetWindowPos(h_cfg_model_, NULL, cfgX, cfgCardY + 294, cfgW, 30, SWP_NOZORDER);
    SetWindowPos(h_cfg_engine_combo_, NULL, cfgX, cfgCardY + 364, cfgW, 120, SWP_NOZORDER);

    // Workspace controls
    int wsRightW = max(320, chat_w - 350);
    SetWindowPos(h_ws_listbox_, NULL, 68, 96, 290, height - 175, SWP_NOZORDER);
    SetWindowPos(h_ws_refresh_btn_, NULL, 68, height - 66, 130, 34, SWP_NOZORDER);
    SetWindowPos(h_ws_open_folder_btn_, NULL, 208, height - 66, 150, 34, SWP_NOZORDER);
    SetWindowPos(h_ws_preview_edit_, NULL, 376, 96, wsRightW - 20, height - 175, SWP_NOZORDER);
    SetWindowPos(h_ws_save_btn_, NULL, 376, height - 66, 110, 34, SWP_NOZORDER);
    SetWindowPos(h_ws_cmd_input_, NULL, 496, height - 66, wsRightW - 240, 34, SWP_NOZORDER);
    SetWindowPos(h_ws_cmd_run_btn_, NULL, 376 + wsRightW - 110, height - 66, 90, 34, SWP_NOZORDER);
}

void NativeGui::switch_tab(ActiveTab tab) {
    if (current_tab_ == tab && tab_transition_progress_ >= 1.0f) return;

    int fromIcon = active_dock_icon_;
    current_tab_ = tab;

    if (tab == ActiveTab::CHAT) active_dock_icon_ = 0;
    else if (tab == ActiveTab::WORKSPACE) active_dock_icon_ = 2;
    else if (tab == ActiveTab::ARENA) active_dock_icon_ = 5;
    else if (tab == ActiveTab::SETTINGS) active_dock_icon_ = 6;

    int toIcon = active_dock_icon_;

    // Set transition state
    tab_transition_from_icon_ = fromIcon;
    tab_transition_to_icon_ = toIcon;
    tab_transition_progress_ = 0.0f;
    tab_transition_offset_y_ = 0.0f;

    // Calculate dock indicator target position (centered vertically)
    RECT rc;
    GetClientRect(hwnd_main_, &rc);
    dock_indicator_target_y_ = get_dock_icon_y(active_dock_icon_, (float)rc.bottom);

    update_control_visibility();
    layout_controls(rc.right, rc.bottom);

    if (tab == ActiveTab::CHAT && h_chat_input_) {
        SetFocus(h_chat_input_);
    } else if (tab == ActiveTab::ARENA && h_arena_interject_input_) {
        SetFocus(h_arena_interject_input_);
    }

    InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::update_control_visibility() {
    BOOL isChat = (current_tab_ == ActiveTab::CHAT);
    BOOL isArena = (current_tab_ == ActiveTab::ARENA);
    BOOL isSettings = (current_tab_ == ActiveTab::SETTINGS);
    BOOL isWs = (current_tab_ == ActiveTab::WORKSPACE);

    ShowWindow(h_chat_input_, isChat ? SW_SHOW : SW_HIDE);
    ShowWindow(h_arena_interject_input_, isArena ? SW_SHOW : SW_HIDE);
    ShowWindow(h_arena_topic_input_, (isArena && show_topic_modal_) ? SW_SHOW : SW_HIDE);

    ShowWindow(h_cfg_provider_combo_, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(h_cfg_base_url_, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(h_cfg_api_key_, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(h_cfg_model_, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(h_cfg_engine_combo_, isSettings ? SW_SHOW : SW_HIDE);

    ShowWindow(h_ws_listbox_, isWs ? SW_SHOW : SW_HIDE);
    ShowWindow(h_ws_refresh_btn_, isWs ? SW_SHOW : SW_HIDE);
    ShowWindow(h_ws_open_folder_btn_, isWs ? SW_SHOW : SW_HIDE);
    ShowWindow(h_ws_preview_edit_, isWs ? SW_SHOW : SW_HIDE);
    ShowWindow(h_ws_save_btn_, isWs ? SW_SHOW : SW_HIDE);
    ShowWindow(h_ws_cmd_input_, isWs ? SW_SHOW : SW_HIDE);
    ShowWindow(h_ws_cmd_run_btn_, isWs ? SW_SHOW : SW_HIDE);
}

static inline D2D1_COLOR_F to_d2d_col(Gdiplus::Color c) {
    return D2D1::ColorF(c.GetR() / 255.0f, c.GetG() / 255.0f, c.GetB() / 255.0f, c.GetA() / 255.0f);
}

ID2D1RenderTarget* NativeGui::get_d2d_target() {
    if (use_dx11_ && dx11_renderer_.get_context()) {
        return dx11_renderer_.get_context();
    }
    return d2d_dc_target_;
}

void NativeGui::ensure_d2d_resources() {
    d2d_target_ = get_d2d_target();
    ID2D1Factory* fac = (use_dx11_ && dx11_renderer_.get_factory()) ?
                        (ID2D1Factory*)dx11_renderer_.get_factory() : d2d_factory_;
    if (!d2d_target_ || !fac) return;

    if (!d2d_solid_brush_) {
        d2d_target_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &d2d_solid_brush_);
    }
    if (!d2d_stroke_round_) {
        D2D1_STROKE_STYLE_PROPERTIES ssp = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND
        );
        fac->CreateStrokeStyle(&ssp, NULL, 0, &d2d_stroke_round_);
    }
    if (!d2d_avatar_bmp_ && muse_avatar_img_ && muse_avatar_img_->GetLastStatus() == Gdiplus::Ok) {
        UINT w = muse_avatar_img_->GetWidth();
        UINT h = muse_avatar_img_->GetHeight();
        if (w > 0 && h > 0) {
            Gdiplus::Bitmap bmp(w, h, PixelFormat32bppPARGB);
            {
                Gdiplus::Graphics gBmp(&bmp);
                gBmp.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                gBmp.DrawImage(muse_avatar_img_, 0, 0, (INT)w, (INT)h);
            }
            Gdiplus::BitmapData bd;
            Gdiplus::Rect lockRc(0, 0, (INT)w, (INT)h);
            if (bmp.LockBits(&lockRc, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bd) == Gdiplus::Ok) {
                D2D1_BITMAP_PROPERTIES bp = D2D1::BitmapProperties(
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
                );
                d2d_target_->CreateBitmap(D2D1::SizeU(w, h), bd.Scan0, bd.Stride, &bp, &d2d_avatar_bmp_);
                bmp.UnlockBits(&bd);
            }
        }
    }
}

IDWriteTextFormat* NativeGui::get_text_format(const wchar_t* fontFamily, float fontSizePt, bool bold, bool wordWrap, float lineSpacing) {
    if (!dwrite_factory_) return NULL;
    const wchar_t* fam = fontFamily ? fontFamily : L"Segoe UI";
    wchar_t keyBuf[160];
    swprintf_s(keyBuf, L"%s_%.1f_%d_%d_%.1f", fam, fontSizePt, bold ? 1 : 0, wordWrap ? 1 : 0, lineSpacing);
    std::wstring key(keyBuf);

    auto it = text_format_cache_.find(key);
    if (it != text_format_cache_.end()) {
        return it->second;
    }

    float dipSize = fontSizePt * (96.0f / 72.0f);
    IDWriteTextFormat* fmt = NULL;
    HRESULT hr = dwrite_factory_->CreateTextFormat(
        fam,
        NULL,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        dipSize,
        L"vi-VN",
        &fmt
    );
    if (FAILED(hr) || !fmt) return NULL;

    fmt->SetWordWrapping(wordWrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (lineSpacing > 0.0f) {
        fmt->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineSpacing, lineSpacing * 0.77f);
    }
    text_format_cache_[key] = fmt;
    return fmt;
}

void NativeGui::d2d_fill_rect(float x, float y, float w, float h, Gdiplus::Color col) {
    if (!d2d_batch_active_ || !d2d_target_ || !d2d_solid_brush_) return;
    d2d_solid_brush_->SetColor(to_d2d_col(col));
    d2d_target_->FillRectangle(D2D1::RectF(x, y, x + w, y + h), d2d_solid_brush_);
}

void NativeGui::d2d_fill_ellipse(float x, float y, float w, float h, Gdiplus::Color col) {
    if (!d2d_batch_active_ || !d2d_target_ || !d2d_solid_brush_) return;
    d2d_solid_brush_->SetColor(to_d2d_col(col));
    D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(x + w * 0.5f, y + h * 0.5f), w * 0.5f, h * 0.5f);
    d2d_target_->FillEllipse(el, d2d_solid_brush_);
}

void NativeGui::d2d_draw_ellipse(float x, float y, float w, float h, Gdiplus::Color col, float strokeWidth) {
    if (!d2d_batch_active_ || !d2d_target_ || !d2d_solid_brush_) return;
    d2d_solid_brush_->SetColor(to_d2d_col(col));
    D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(x + w * 0.5f, y + h * 0.5f), w * 0.5f, h * 0.5f);
    d2d_target_->DrawEllipse(el, d2d_solid_brush_, strokeWidth, d2d_stroke_round_);
}

void NativeGui::d2d_draw_line(float x1, float y1, float x2, float y2, Gdiplus::Color col, float strokeWidth) {
    if (!d2d_batch_active_ || !d2d_target_ || !d2d_solid_brush_) return;
    d2d_solid_brush_->SetColor(to_d2d_col(col));
    d2d_target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), d2d_solid_brush_, strokeWidth, d2d_stroke_round_);
}

void NativeGui::d2d_draw_arc(float x, float y, float w, float h, float startDeg, float sweepDeg, Gdiplus::Color col, float strokeWidth) {
    if (!d2d_batch_active_ || !d2d_target_ || !d2d_solid_brush_) return;
    float rx = w * 0.5f;
    float ry = h * 0.5f;
    float cx = x + rx;
    float cy = y + ry;
    int steps = 14;
    float degToRad = 3.14159265f / 180.0f;
    d2d_solid_brush_->SetColor(to_d2d_col(col));
    for (int i = 0; i < steps; ++i) {
        float a1 = (startDeg + (sweepDeg * i) / steps) * degToRad;
        float a2 = (startDeg + (sweepDeg * (i + 1)) / steps) * degToRad;
        d2d_target_->DrawLine(
            D2D1::Point2F(cx + std::cos(a1) * rx, cy + std::sin(a1) * ry),
            D2D1::Point2F(cx + std::cos(a2) * rx, cy + std::sin(a2) * ry),
            d2d_solid_brush_, strokeWidth, d2d_stroke_round_
        );
    }
}

void NativeGui::draw_rounded_box(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, float radius,
                                 Gdiplus::Color bgColor, Gdiplus::Color borderColor, float borderWidth) {
    if (rect.Width <= 0 || rect.Height <= 0) return;

    if (d2d_batch_active_ && d2d_target_ && d2d_solid_brush_) {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF(rect.X, rect.Y, rect.X + rect.Width, rect.Y + rect.Height),
            radius, radius
        );
        if (bgColor.GetAlpha() > 0) {
            d2d_solid_brush_->SetColor(to_d2d_col(bgColor));
            d2d_target_->FillRoundedRectangle(rr, d2d_solid_brush_);
        }
        if (borderColor.GetAlpha() > 0 && borderWidth > 0.0f) {
            d2d_solid_brush_->SetColor(to_d2d_col(borderColor));
            d2d_target_->DrawRoundedRectangle(rr, d2d_solid_brush_, borderWidth);
        }
        return;
    }

    // GDI+ Fallback (used when rendering owner-draw Win32 controls like custom buttons)
    Gdiplus::GraphicsPath path;
    float d = min(radius * 2.0f, min(rect.Width, rect.Height));
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();

    if (bgColor.GetAlpha() > 0) {
        Gdiplus::SolidBrush fillBr(bgColor);
        g.FillPath(&fillBr, &path);
    }
    if (borderColor.GetAlpha() > 0 && borderWidth > 0.0f) {
        Gdiplus::Pen pen(borderColor, borderWidth);
        g.DrawPath(&pen, &path);
    }
}

void NativeGui::draw_circle_avatar(Gdiplus::Graphics& g, Gdiplus::Image* img, float x, float y, float size, const std::wstring& fallbackLetter) {
    if (d2d_batch_active_ && d2d_target_ && d2d_solid_brush_) {
        d2d_fill_ellipse(x, y, size, size, Gdiplus::Color(255, 255, 255, 255));
        if (d2d_avatar_bmp_) {
            D2D1_SIZE_F bmpSz = d2d_avatar_bmp_->GetSize();
            if (bmpSz.width > 0 && bmpSz.height > 0) {
                ID2D1BitmapBrush* bmpBrush = NULL;
                D2D1_BITMAP_BRUSH_PROPERTIES bbp = D2D1::BitmapBrushProperties(
                    D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
                );
                if (SUCCEEDED(d2d_target_->CreateBitmapBrush(d2d_avatar_bmp_, &bbp, NULL, &bmpBrush)) && bmpBrush) {
                    D2D1_MATRIX_3X2_F mat = D2D1::Matrix3x2F::Scale(size / bmpSz.width, size / bmpSz.height) *
                                            D2D1::Matrix3x2F::Translation(x, y);
                    bmpBrush->SetTransform(mat);
                    D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(x + size * 0.5f, y + size * 0.5f), size * 0.5f, size * 0.5f);
                    d2d_target_->FillEllipse(el, bmpBrush);
                    bmpBrush->Release();
                    return;
                }
            }
        }
        d2d_fill_ellipse(x, y, size, size, Gdiplus::Color(255, 124, 58, 237));
        draw_text_dw(g, fallbackLetter, Gdiplus::RectF(x, y, size, size), L"Segoe UI", size * 0.38f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);
    }
}

float NativeGui::get_dock_start_y(float clientH) {
    // Vertically center the 6 navigation icons (each 36px with 10px spacing = 5 * 46 + 36 = 266px total)
    // in the vertical region between the top bar (50px) and bottom settings button (clientH - 96px).
    float startY = (clientH - 312.0f) * 0.5f;
    return max(60.0f, startY);
}

float NativeGui::get_dock_icon_y(int iconIdx, float clientH) {
    if (iconIdx <= 5) {
        return get_dock_start_y(clientH) + (iconIdx * 46.0f);
    } else if (iconIdx == 6) {
        return clientH - 88.0f; // Settings
    } else {
        return clientH - 44.0f; // Sidebar Toggle
    }
}

void NativeGui::draw_dock_vector_icon(Gdiplus::Graphics& g, int iconIdx, float cx, float cy, Gdiplus::Color col) {
    float sw = 1.6f;
    switch (iconIdx) {
        case 0: { // Speech Bubble (Chat)
            d2d_draw_arc(cx - 7.5f, cy - 7.5f, 15.0f, 14.0f, 135.0f, 300.0f, col, sw);
            d2d_draw_line(cx - 5.5f, cy + 3.5f, cx - 8.0f, cy + 7.5f, col, sw);
            d2d_draw_line(cx - 8.0f, cy + 7.5f, cx - 2.0f, cy + 6.0f, col, sw);
            break;
        }
        case 1: { // Search Magnifying Glass
            d2d_draw_ellipse(cx - 7.0f, cy - 7.0f, 11.0f, 11.0f, col, sw);
            d2d_draw_line(cx + 2.5f, cy + 2.5f, cx + 7.0f, cy + 7.0f, col, sw);
            break;
        }
        case 2: { // Notebook / Docs with spine
            draw_rounded_box(g, Gdiplus::RectF(cx - 6.5f, cy - 7.5f, 13.0f, 15.0f), 2.5f, Gdiplus::Color(0,0,0,0), col, 1.5f);
            d2d_draw_line(cx - 3.0f, cy - 7.5f, cx - 3.0f, cy + 7.5f, col, sw);
            d2d_draw_line(cx, cy - 3.0f, cx + 3.5f, cy - 3.0f, col, sw);
            d2d_draw_line(cx, cy + 1.0f, cx + 3.5f, cy + 1.0f, col, sw);
            break;
        }
        case 3: { // Lightbulb (Skills)
            d2d_draw_arc(cx - 5.5f, cy - 7.5f, 11.0f, 11.0f, 140.0f, 260.0f, col, sw);
            d2d_draw_line(cx - 3.5f, cy + 2.0f, cx - 2.5f, cy + 4.5f, col, sw);
            d2d_draw_line(cx + 3.5f, cy + 2.0f, cx + 2.5f, cy + 4.5f, col, sw);
            d2d_draw_line(cx - 2.5f, cy + 4.5f, cx + 2.5f, cy + 4.5f, col, sw);
            d2d_draw_line(cx - 1.5f, cy + 7.5f, cx + 1.5f, cy + 7.5f, col, sw);
            break;
        }
        case 4: { // Checkbox with checkmark (Tasks)
            draw_rounded_box(g, Gdiplus::RectF(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f), 3.0f, Gdiplus::Color(0,0,0,0), col, 1.5f);
            d2d_draw_line(cx - 3.5f, cy + 0.2f, cx - 0.8f, cy + 3.0f, col, sw);
            d2d_draw_line(cx - 0.8f, cy + 3.0f, cx + 4.0f, cy - 2.5f, col, sw);
            break;
        }
        case 5: { // 4-Square Grid with rotated diamond top-right
            draw_rounded_box(g, Gdiplus::RectF(cx - 7.5f, cy - 7.5f, 6.0f, 6.0f), 1.5f, Gdiplus::Color(0,0,0,0), col, 1.5f);
            draw_rounded_box(g, Gdiplus::RectF(cx - 7.5f, cy + 1.5f, 6.0f, 6.0f), 1.5f, Gdiplus::Color(0,0,0,0), col, 1.5f);
            draw_rounded_box(g, Gdiplus::RectF(cx + 1.5f, cy + 1.5f, 6.0f, 6.0f), 1.5f, Gdiplus::Color(0,0,0,0), col, 1.5f);
            d2d_draw_line(cx + 4.5f, cy - 8.2f, cx + 8.2f, cy - 4.5f, col, sw);
            d2d_draw_line(cx + 8.2f, cy - 4.5f, cx + 4.5f, cy - 0.8f, col, sw);
            d2d_draw_line(cx + 4.5f, cy - 0.8f, cx + 0.8f, cy - 4.5f, col, sw);
            d2d_draw_line(cx + 0.8f, cy - 4.5f, cx + 4.5f, cy - 8.2f, col, sw);
            break;
        }
        case 6: { // Tray / Download box
            draw_rounded_box(g, Gdiplus::RectF(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f), 3.0f, Gdiplus::Color(0,0,0,0), col, 1.5f);
            d2d_draw_line(cx, cy - 4.0f, cx, cy + 2.0f, col, sw);
            d2d_draw_line(cx - 2.8f, cy - 0.5f, cx, cy + 2.5f, col, sw);
            d2d_draw_line(cx + 2.8f, cy - 0.5f, cx, cy + 2.5f, col, sw);
            d2d_draw_line(cx - 4.0f, cy + 4.2f, cx + 4.0f, cy + 4.2f, col, sw);
            break;
        }
        case 7: { // '=' Two horizontal lines
            d2d_draw_line(cx - 5.5f, cy - 2.5f, cx + 5.5f, cy - 2.5f, col, sw);
            d2d_draw_line(cx - 5.5f, cy + 2.5f, cx + 5.5f, cy + 2.5f, col, sw);
            break;
        }
    }
}

void NativeGui::draw_right_tab_vector_icon(Gdiplus::Graphics& g, int tabIdx, float cx, float cy, Gdiplus::Color col) {
    float sw = 1.5f;
    switch (tabIdx) {
        case 0: { // Bullet list icon (≡:)
            for (int i = -1; i <= 1; ++i) {
                float ry = cy + (i * 4.0f);
                d2d_fill_ellipse(cx - 7.0f, ry - 1.0f, 2.2f, 2.2f, col);
                d2d_draw_line(cx - 2.5f, ry, cx + 6.5f, ry, col, sw);
            }
            break;
        }
        case 1: { // Shield with checkmark
            d2d_draw_line(cx, cy - 6.8f, cx + 5.8f, cy - 4.5f, col, sw);
            d2d_draw_line(cx + 5.8f, cy - 4.5f, cx + 5.2f, cy + 1.5f, col, sw);
            d2d_draw_line(cx + 5.2f, cy + 1.5f, cx, cy + 6.8f, col, sw);
            d2d_draw_line(cx, cy + 6.8f, cx - 5.2f, cy + 1.5f, col, sw);
            d2d_draw_line(cx - 5.2f, cy + 1.5f, cx - 5.8f, cy - 4.5f, col, sw);
            d2d_draw_line(cx - 5.8f, cy - 4.5f, cx, cy - 6.8f, col, sw);
            d2d_draw_line(cx - 2.3f, cy + 0.2f, cx - 0.5f, cy + 2.2f, col, sw);
            d2d_draw_line(cx - 0.5f, cy + 2.2f, cx + 2.8f, cy - 1.8f, col, sw);
            break;
        }
        case 2: { // Clock
            d2d_draw_ellipse(cx - 6.5f, cy - 6.5f, 13.0f, 13.0f, col, sw);
            d2d_draw_line(cx, cy - 3.5f, cx, cy + 0.5f, col, sw);
            d2d_draw_line(cx, cy + 0.5f, cx + 3.0f, cy + 2.0f, col, sw);
            break;
        }
        case 3: { // Fingerprint / AI concentric arcs
            d2d_draw_arc(cx - 6.5f, cy - 6.5f, 13.0f, 13.0f, 195.0f, 150.0f, col, sw);
            d2d_draw_arc(cx - 4.2f, cy - 4.2f, 8.4f, 9.5f, 180.0f, 180.0f, col, sw);
            d2d_draw_arc(cx - 2.0f, cy - 2.0f, 4.0f, 6.5f, 170.0f, 190.0f, col, sw);
            d2d_draw_line(cx, cy + 0.5f, cx, cy + 5.5f, col, sw);
            break;
        }
    }
}

void NativeGui::set_render_clip(float x, float y, float w, float h) {
    if (d2d_batch_active_ && d2d_target_) {
        if (has_active_clip_) {
            d2d_target_->PopAxisAlignedClip();
        }
        active_clip_rect_ = D2D1::RectF(x, y, x + w, y + h);
        d2d_target_->PushAxisAlignedClip(active_clip_rect_, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        has_active_clip_ = true;
    }
}

void NativeGui::clear_render_clip() {
    if (has_active_clip_ && d2d_batch_active_ && d2d_target_) {
        d2d_target_->PopAxisAlignedClip();
    }
    has_active_clip_ = false;
}

D2D1_SIZE_F NativeGui::measure_text_dw(const std::wstring& text, float maxW, const wchar_t* fontFamily,
                                       float fontSizePt, bool bold, bool wordWrap) {
    D2D1_SIZE_F res = { 0.0f, 0.0f };
    if (!dwrite_factory_ || text.empty()) return res;

    IDWriteTextFormat* fmt = get_text_format(fontFamily, fontSizePt, bold, wordWrap);
    if (!fmt) return res;

    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    IDWriteTextLayout* layout = NULL;
    HRESULT hr = dwrite_factory_->CreateTextLayout(
        text.c_str(),
        (UINT32)text.size(),
        fmt,
        maxW > 0.0f ? maxW : 4000.0f,
        4000.0f,
        &layout
    );
    if (SUCCEEDED(hr) && layout) {
        DWRITE_TEXT_METRICS tm;
        if (SUCCEEDED(layout->GetMetrics(&tm))) {
            res.width = tm.widthIncludingTrailingWhitespace;
            res.height = tm.height;
        }
        layout->Release();
    }
    return res;
}

void NativeGui::draw_text_dw(Gdiplus::Graphics& g, const std::wstring& text, const Gdiplus::RectF& rect,
                             const wchar_t* fontFamily, float fontSizePt, bool bold, Gdiplus::Color color,
                             int alignH, int alignV, bool wordWrap) {
    if (text.empty() || !d2d_target_ || !dwrite_factory_) return;

    IDWriteTextFormat* fmt = get_text_format(fontFamily, fontSizePt, bold, wordWrap);
    if (!fmt) return;

    if (alignH == 1) fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    else if (alignH == 2) fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    else fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    if (alignV == 1) fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    else fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // Fast path: when inside the single-pass frame batch, draw directly without BindDC/BeginDraw/EndDraw!
    if (d2d_batch_active_ && d2d_solid_brush_) {
        d2d_solid_brush_->SetColor(to_d2d_col(color));
        D2D1_RECT_F layoutRect = D2D1::RectF(rect.X, rect.Y, rect.X + rect.Width, rect.Y + rect.Height);
        d2d_target_->DrawText(
            text.c_str(),
            (UINT32)text.size(),
            fmt,
            layoutRect,
            d2d_solid_brush_,
            static_cast<D2D1_DRAW_TEXT_OPTIONS>(0x00000004)
        );
        return;
    }
}

static void parse_inline_markdown_spans(const std::string& line, std::wstring& outCleanWStr,
                                        std::vector<DWRITE_TEXT_RANGE>& outCodeRanges,
                                        std::vector<DWRITE_TEXT_RANGE>& outBoldRanges) {
    outCleanWStr.clear();
    outCodeRanges.clear();
    outBoldRanges.clear();

    size_t i = 0;
    while (i < line.size()) {
        if (line[i] == '`') {
            size_t endTick = line.find('`', i + 1);
            if (endTick != std::string::npos) {
                std::wstring wTok = L" " + s2ws(line.substr(i + 1, endTick - i - 1)) + L" ";
                DWRITE_TEXT_RANGE r = { (UINT32)outCleanWStr.size(), (UINT32)wTok.size() };
                outCodeRanges.push_back(r);
                outCleanWStr += wTok;
                i = endTick + 1;
                continue;
            }
        }
        if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
            size_t endBold = line.find("**", i + 2);
            if (endBold != std::string::npos) {
                std::wstring wTok = s2ws(line.substr(i + 2, endBold - i - 2));
                DWRITE_TEXT_RANGE r = { (UINT32)outCleanWStr.size(), (UINT32)wTok.size() };
                outBoldRanges.push_back(r);
                outCleanWStr += wTok;
                i = endBold + 2;
                continue;
            }
        }
        size_t nextSpec = line.find_first_of("`*", i);
        if (nextSpec == i) {
            outCleanWStr += s2ws(line.substr(i, 1));
            i++;
        } else if (nextSpec == std::string::npos) {
            outCleanWStr += s2ws(line.substr(i));
            break;
        } else {
            outCleanWStr += s2ws(line.substr(i, nextSpec - i));
            i = nextSpec;
        }
    }
}

float NativeGui::measure_rich_text_line(const std::string& line, float maxW) {
    if (line.empty()) return 10.0f;
    std::wstring cleanWStr;
    std::vector<DWRITE_TEXT_RANGE> codeRanges;
    std::vector<DWRITE_TEXT_RANGE> boldRanges;
    parse_inline_markdown_spans(line, cleanWStr, codeRanges, boldRanges);

    if (!dwrite_factory_ || cleanWStr.empty()) {
        int wrapped = max(1, (int)((cleanWStr.size() * 7.2f) / maxW) + 1);
        return (wrapped * 22.0f) + 4.0f;
    }

    IDWriteTextFormat* fmt = get_text_format(L"Segoe UI", 10.0f, false, true, 21.5f);
    if (!fmt) return 24.0f;

    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    IDWriteTextLayout* layout = NULL;
    float h = 24.0f;
    if (SUCCEEDED(dwrite_factory_->CreateTextLayout(cleanWStr.c_str(), (UINT32)cleanWStr.size(), fmt, maxW, 4000.0f, &layout)) && layout) {
        for (const auto& r : boldRanges) {
            layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, r);
        }
        for (const auto& r : codeRanges) {
            layout->SetFontFamilyName(L"Consolas", r);
            layout->SetFontSize(9.2f * (96.0f / 72.0f), r);
        }
        DWRITE_TEXT_METRICS tm;
        if (SUCCEEDED(layout->GetMetrics(&tm))) {
            h = max(21.5f, tm.height + 3.0f);
        }
        layout->Release();
    }
    return h;
}

void NativeGui::draw_code_highlighted(Gdiplus::Graphics& g, const std::string& lang, const std::string& code,
                                      const Gdiplus::RectF& rect, Gdiplus::Font& fontCode) {
    Gdiplus::Color defaultCol(255, 212, 212, 216); // #d4d4d8
    Gdiplus::Color commentCol(255, 113, 113, 122); // #71717a
    Gdiplus::Color kwCol(255, 248, 113, 113);      // Coral Red #f87171
    Gdiplus::Color strCol(255, 134, 239, 172);     // Mint Green #86efac
    Gdiplus::Color propCol(255, 125, 211, 252);    // Sky Cyan #7dd3fc
    Gdiplus::Color valCol(255, 196, 181, 253);     // Lavender #c4b5fd

    std::stringstream ss(code);
    std::string line;
    float curY = rect.Y;
    float viewBottom = (float)current_rc_.bottom - 55.0f;

    auto draw_code_token = [&](const std::wstring& wTok, float& xPos, Gdiplus::Color col) {
        if (wTok.empty()) return;
        D2D1_SIZE_F sz = measure_text_dw(wTok, 2000.0f, L"Consolas", 9.5f, false, false);
        float adv = (sz.width > 0.0f) ? sz.width : (wTok.size() * 7.4f);
        draw_text_dw(g, wTok, Gdiplus::RectF(xPos, curY, adv + 20.0f, 20.0f), L"Consolas", 9.5f, false, col, 0, 0, false);
        xPos += adv;
    };

    while (std::getline(ss, line)) {
        // Cull code lines outside the visible viewport on high-res screens
        if (curY + 20.0f < 40.0f || curY > viewBottom) {
            curY += 19.0f;
            continue;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        float curX = rect.X;

        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));

        if (trimmed.rfind("//", 0) == 0 || (trimmed.rfind("#", 0) == 0 && trimmed.rfind("#include", 0) != 0 && trimmed.rfind("#pragma", 0) != 0 && trimmed.rfind("#define", 0) != 0)) {
            draw_text_dw(g, s2ws(line), Gdiplus::RectF(curX, curY, rect.Width, 20.0f), L"Consolas", 9.5f, false, commentCol, 0, 0, false);
        } else if (trimmed.rfind("#", 0) == 0) {
            draw_text_dw(g, s2ws(line), Gdiplus::RectF(curX, curY, rect.Width, 20.0f), L"Consolas", 9.5f, false, kwCol, 0, 0, false);
        } else if (line.find('<') != std::string::npos && line.find('>') != std::string::npos) {
            size_t i = 0;
            while (i < line.size()) {
                if (line[i] == '<') {
                    size_t closePos = line.find('>', i);
                    if (closePos != std::string::npos) {
                        draw_code_token(s2ws(line.substr(i, closePos - i + 1)), curX, kwCol);
                        i = closePos + 1;
                        continue;
                    }
                }
                size_t nextTag = line.find('<', i);
                std::string plain = (nextTag == std::string::npos) ? line.substr(i) : line.substr(i, nextTag - i);
                draw_code_token(s2ws(plain), curX, defaultCol);
                if (nextTag == std::string::npos) break;
                i = nextTag;
            }
        } else if (line.find(':') != std::string::npos && line.find("::") == std::string::npos && line.find("http") == std::string::npos) {
            size_t colonPos = line.find(':');
            draw_code_token(s2ws(line.substr(0, colonPos + 1)), curX, propCol);
            draw_code_token(s2ws(line.substr(colonPos + 1)), curX, valCol);
        } else if (line.find('"') != std::string::npos) {
            size_t i = 0;
            while (i < line.size()) {
                size_t q1 = line.find('"', i);
                if (q1 == std::string::npos) {
                    draw_code_token(s2ws(line.substr(i)), curX, defaultCol);
                    break;
                }
                if (q1 > i) {
                    draw_code_token(s2ws(line.substr(i, q1 - i)), curX, defaultCol);
                }
                size_t q2 = line.find('"', q1 + 1);
                if (q2 == std::string::npos) {
                    draw_code_token(s2ws(line.substr(q1)), curX, strCol);
                    break;
                }
                draw_code_token(s2ws(line.substr(q1, q2 - q1 + 1)), curX, strCol);
                i = q2 + 1;
            }
        } else {
            draw_text_dw(g, s2ws(line), Gdiplus::RectF(curX, curY, rect.Width, 20.0f), L"Consolas", 9.5f, false,
                         (lang == "bash" || lang == "sh" || lang == "powershell" || lang == "cmd") ? propCol : defaultCol, 0, 0, false);
        }

        curY += 19.0f;
    }
}

void NativeGui::draw_rich_text_line(Gdiplus::Graphics& g, const std::string& line, float x, float y, float maxW,
                                    Gdiplus::Font& fontReg, Gdiplus::Font& fontBold, Gdiplus::Font& fontMono) {
    if (line.empty() || !d2d_batch_active_ || !d2d_target_ || !dwrite_factory_ || !d2d_solid_brush_) return;

    std::wstring cleanWStr;
    std::vector<DWRITE_TEXT_RANGE> codeRanges;
    std::vector<DWRITE_TEXT_RANGE> boldRanges;
    parse_inline_markdown_spans(line, cleanWStr, codeRanges, boldRanges);
    if (cleanWStr.empty()) return;

    IDWriteTextFormat* fmt = get_text_format(L"Segoe UI", 10.0f, false, true, 21.5f);
    if (!fmt) return;

    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    IDWriteTextLayout* layout = NULL;
    if (SUCCEEDED(dwrite_factory_->CreateTextLayout(cleanWStr.c_str(), (UINT32)cleanWStr.size(), fmt, maxW, 4000.0f, &layout)) && layout) {
        for (const auto& r : boldRanges) {
            layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, r);
        }
        for (const auto& r : codeRanges) {
            layout->SetFontFamilyName(L"Consolas", r);
            layout->SetFontSize(9.2f * (96.0f / 72.0f), r);
        }

        // Draw inline-code pill backgrounds directly into active Direct2D batch
        for (const auto& r : codeRanges) {
            UINT32 count = 0;
            layout->HitTestTextRange(r.startPosition, r.length, 0.0f, 0.0f, NULL, 0, &count);
            if (count > 0) {
                std::vector<DWRITE_HIT_TEST_METRICS> hits(count);
                if (SUCCEEDED(layout->HitTestTextRange(r.startPosition, r.length, 0.0f, 0.0f, hits.data(), count, &count))) {
                    for (UINT32 hIdx = 0; hIdx < count; ++hIdx) {
                        const auto& hm = hits[hIdx];
                        Gdiplus::RectF pillRect(x + hm.left, y + hm.top + 1.5f, hm.width, max(17.5f, hm.height - 3.0f));
                        draw_rounded_box(g, pillRect, 4.0f, Gdiplus::Color(255, 45, 47, 57), Gdiplus::Color(255, 62, 65, 78), 1.0f);
                    }
                }
            }
        }

        d2d_solid_brush_->SetColor(D2D1::ColorF(236.0f / 255.0f, 237.0f / 255.0f, 242.0f / 255.0f, 1.0f));
        d2d_target_->DrawTextLayout(
            D2D1::Point2F(x, y),
            layout,
            d2d_solid_brush_,
            static_cast<D2D1_DRAW_TEXT_OPTIONS>(0x00000004)
        );

        layout->Release();
    }
}

void NativeGui::draw_custom_button(LPDRAWITEMSTRUCT pDIS) {
    if (!pDIS || !pDIS->hDC) return;

    RECT rc = pDIS->rcItem;
    Gdiplus::Graphics g(pDIS->hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    Gdiplus::RectF r((float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
    bool isSelected = (pDIS->itemState & ODS_SELECTED);

    wchar_t btnText[128] = { 0 };
    GetWindowTextW(pDIS->hwndItem, btnText, 128);

    Gdiplus::Color bgCol = isSelected ? Gdiplus::Color(255, 24, 26, 34) : Gdiplus::Color(255, 34, 36, 45);
    Gdiplus::Color borderCol(255, 50, 53, 66);
    Gdiplus::Color textCol(255, 241, 245, 249);

    Gdiplus::Color parentBg = (pDIS->hwndItem == h_cfg_test_btn_ || pDIS->hwndItem == h_cfg_save_btn_ || pDIS->hwndItem == h_cfg_reset_ws_btn_)
        ? Gdiplus::Color(255, 28, 29, 34) : Gdiplus::Color(255, 18, 19, 22);
    Gdiplus::SolidBrush bgFill(parentBg);
    g.FillRectangle(&bgFill, r);

    draw_rounded_box(g, r, 8.0f, bgCol, borderCol, 1.0f);

    Gdiplus::FontFamily fontFam(L"Segoe UI");
    Gdiplus::Font font(&fontFam, 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush textBrush(textCol);
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(btnText, -1, &font, r, &sf, &textBrush);
}

void NativeGui::render_left_dock(Gdiplus::Graphics& g, const RECT& rc) {
    float dockW = 48.0f;
    d2d_fill_rect(0.0f, 0.0f, dockW, (float)rc.bottom, Gdiplus::Color(255, 16, 17, 20)); // #101114
    d2d_draw_line(dockW - 0.5f, 0.0f, dockW - 0.5f, (float)rc.bottom, Gdiplus::Color(255, 32, 33, 39), 1.0f);

    float startY = get_dock_start_y((float)rc.bottom);

    // 1. Smoothly gliding active indicator pill along the rail
    Gdiplus::RectF glideBox(6.0f, dock_indicator_y_, 36.0f, 36.0f);
    draw_rounded_box(g, glideBox, 18.0f, Gdiplus::Color(255, 38, 39, 46), Gdiplus::Color(255, 52, 54, 64), 1.0f);
    // Subtle neon purple accent indicator on the edge
    d2d_fill_rect(0.0f, dock_indicator_y_ + 10.0f, 2.5f, 16.0f, Gdiplus::Color(255, 168, 85, 247));

    float curY = startY;
    for (int i = 0; i < 6; ++i) {
        Gdiplus::RectF box(6.0f, curY, 36.0f, 36.0f);
        bool isAct = (active_dock_icon_ == i);
        bool isHov = (hover_dock_icon_ == i && !isAct);

        if (isHov) {
            draw_rounded_box(g, box, 18.0f, Gdiplus::Color(255, 28, 29, 35), Gdiplus::Color(0, 0, 0, 0), 0.0f);
        }

        Gdiplus::Color iconCol = isAct ? Gdiplus::Color(255, 240, 242, 248) :
                                 (isHov ? Gdiplus::Color(255, 200, 205, 215) : Gdiplus::Color(255, 142, 146, 158));
        draw_dock_vector_icon(g, i, 24.0f, curY + 18.0f, iconCol);
        curY += 46.0f;
    }

    // Bottom icon 1: Tray / Settings (iconIdx 6)
    float setY = get_dock_icon_y(6, (float)rc.bottom);
    Gdiplus::RectF setBox(6.0f, setY, 36.0f, 36.0f);
    bool setAct = (active_dock_icon_ == 6);
    bool setHov = (hover_dock_icon_ == 6 && !setAct);
    if (setHov) {
        draw_rounded_box(g, setBox, 18.0f, Gdiplus::Color(255, 34, 35, 42), Gdiplus::Color(0,0,0,0), 0.0f);
    }
    draw_dock_vector_icon(g, 6, 24.0f, setY + 18.0f,
                          setAct ? Gdiplus::Color(255, 240, 242, 248) : Gdiplus::Color(255, 142, 146, 158));

    // Bottom icon 2: '=' Sidebar Toggle (iconIdx 7)
    float togY = get_dock_icon_y(7, (float)rc.bottom);
    Gdiplus::RectF togBox(6.0f, togY, 36.0f, 36.0f);
    if (hover_dock_icon_ == 7) {
        draw_rounded_box(g, togBox, 18.0f, Gdiplus::Color(255, 34, 35, 42), Gdiplus::Color(0,0,0,0), 0.0f);
    }
    draw_dock_vector_icon(g, 7, 24.0f, togY + 18.0f,
                          (right_panel_cur_w_ > 50.0f) ? Gdiplus::Color(255, 210, 214, 224) : Gdiplus::Color(255, 142, 146, 158));
}

void NativeGui::render_top_bar(Gdiplus::Graphics& g, const RECT& rc) {
    float top_bar_right = (right_panel_cur_w_ > 146.0f) ? (float)(rc.right - (int)right_panel_cur_w_) : ((float)rc.right - 146.0f);
    float chat_right = top_bar_right;

    // 1. Top-Left Pill Button: Current Tab Indicator
    std::wstring tabTitle = L"Đoạn chat";
    if (current_tab_ == ActiveTab::ARENA) tabTitle = L"Đấu trường";
    else if (current_tab_ == ActiveTab::WORKSPACE) tabTitle = L"Workspace";
    else if (current_tab_ == ActiveTab::SETTINGS) tabTitle = L"Cài đặt";

    float sessionW = (current_tab_ == ActiveTab::ARENA || current_tab_ == ActiveTab::WORKSPACE) ? 112.0f : 96.0f;
    Gdiplus::RectF sessionRect(60.0f, 10.0f, sessionW, 30.0f);
    Gdiplus::Color sesBg = hover_btn_session_ ? Gdiplus::Color(255, 48, 50, 58) : Gdiplus::Color(255, 38, 39, 45);
    draw_rounded_box(g, sessionRect, 15.0f, sesBg, Gdiplus::Color(255, 52, 54, 62), 1.0f);

    Gdiplus::Color icCol(255, 210, 214, 222);
    d2d_draw_line(72.0f, 21.0f, 82.0f, 21.0f, icCol, 1.5f);
    d2d_draw_line(72.0f, 25.0f, 80.0f, 25.0f, icCol, 1.5f);
    d2d_draw_line(72.0f, 29.0f, 82.0f, 29.0f, icCol, 1.5f);

    draw_text_dw(g, tabTitle, Gdiplus::RectF(87.0f, 10.0f, sessionW - 30.0f, 30.0f),
                 L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 236, 238, 244), 0, 1, false);

    // Sleek Frameless Title Branding: ⚡ CYBERBOX ∞
    float brandX = sessionRect.X + sessionRect.Width + 14.0f;
    draw_text_dw(g, L"⚡ CYBERBOX ∞", Gdiplus::RectF(brandX, 10.0f, 130.0f, 30.0f),
                 L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 129, 140, 248), 0, 1, false);

    // 2. Security Status Badge (Air-Gap Zero-Trust / Online Web Bridge) - 1-Click Interactive Toggle!
    auto status = sandbox_mgr_->get_status();
    bool isOnline = status.internet_enabled;
    float badgeW = isOnline ? 135.0f : 150.0f;
    std::wstring badgeStr = isOnline ? L"🌐 ONLINE" : L"🛡️ AIR-GAP";

    if (current_tab_ == ActiveTab::CHAT) {
        // Top-Right Pill Button: + Mới (Only in Chat Tab!)
        float newX = chat_right - 74.0f;
        Gdiplus::RectF newRect(newX, 10.0f, 60.0f, 30.0f);
        Gdiplus::Color newBg = hover_btn_new_ ? Gdiplus::Color(255, 48, 50, 58) : Gdiplus::Color(255, 38, 39, 45);
        draw_rounded_box(g, newRect, 15.0f, newBg, Gdiplus::Color(255, 52, 54, 62), 1.0f);

        draw_rounded_box(g, Gdiplus::RectF(newX + 11.0f, 19.0f, 11.0f, 11.0f), 2.0f, Gdiplus::Color(0,0,0,0), icCol, 1.3f);
        d2d_draw_line(newX + 16.5f, 21.5f, newX + 16.5f, 27.5f, icCol, 1.5f);
        d2d_draw_line(newX + 13.5f, 24.5f, newX + 19.5f, 24.5f, icCol, 1.5f);
        draw_text_dw(g, L"Mới", Gdiplus::RectF(newX + 26.0f, 10.0f, 32.0f, 30.0f),
                     L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 236, 238, 244), 0, 1, false);

        float badgeX = newX - badgeW - 10.0f;
        if (badgeX > sessionRect.X + sessionRect.Width + 10.0f) {
            security_badge_rect_ = Gdiplus::RectF(badgeX, 10.0f, badgeW, 30.0f);
        } else {
            security_badge_rect_ = Gdiplus::RectF(0, 0, 0, 0);
        }
    } else if (current_tab_ == ActiveTab::ARENA) {
        // Direct2D Arena Action Buttons on Top Bar (100% GPU, zero HWND smearing):
        // 1. Export button on far right: [chat_right - 88 ... chat_right - 10]
        float expW = 80.0f;
        float expX = chat_right - expW - 10.0f;
        arena_export_btn_rect_ = Gdiplus::RectF(expX, 10.0f, expW, 30.0f);
        Gdiplus::Color expBg = hover_btn_arena_export_ ? Gdiplus::Color(255, 48, 50, 60) : Gdiplus::Color(255, 34, 36, 45);
        draw_rounded_box(g, arena_export_btn_rect_, 8.0f, expBg, Gdiplus::Color(255, 52, 55, 68), 1.0f);
        draw_text_dw(g, L"⬇ Xuất .MD", arena_export_btn_rect_, L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 236, 238, 244), 1, 1, false);

        // 2. History / Saved Sessions button
        float sessW = 78.0f;
        float sessX = expX - sessW - 8.0f;
        arena_sessions_btn_rect_ = Gdiplus::RectF(sessX, 10.0f, sessW, 30.0f);
        Gdiplus::Color sessBg = hover_btn_arena_sessions_ ? Gdiplus::Color(255, 48, 50, 60) : Gdiplus::Color(255, 34, 36, 45);
        draw_rounded_box(g, arena_sessions_btn_rect_, 8.0f, sessBg, Gdiplus::Color(255, 52, 55, 68), 1.0f);
        draw_text_dw(g, L"📁 Lịch sử", arena_sessions_btn_rect_, L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 236, 238, 244), 1, 1, false);

        // 3. Save Session button
        float savW = 74.0f;
        float savX = sessX - savW - 8.0f;
        arena_save_btn_rect_ = Gdiplus::RectF(savX, 10.0f, savW, 30.0f);
        Gdiplus::Color savBg = hover_btn_arena_save_ ? Gdiplus::Color(255, 76, 29, 149) : Gdiplus::Color(255, 50, 32, 85);
        draw_rounded_box(g, arena_save_btn_rect_, 8.0f, savBg, Gdiplus::Color(255, 126, 34, 206), 1.0f);
        draw_text_dw(g, L"💾 Lưu", arena_save_btn_rect_, L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 255, 255), 1, 1, false);

        // 4. Topic button: [savX - 100 ... savX - 8]
        float topW = 92.0f;
        float topX = savX - topW - 8.0f;
        arena_topic_btn_rect_ = Gdiplus::RectF(topX, 10.0f, topW, 30.0f);
        Gdiplus::Color topBg = hover_btn_arena_topic_ ? Gdiplus::Color(255, 48, 50, 60) : Gdiplus::Color(255, 34, 36, 45);
        draw_rounded_box(g, arena_topic_btn_rect_, 8.0f, topBg, Gdiplus::Color(255, 52, 55, 68), 1.0f);
        draw_text_dw(g, L"✎ Đổi chủ đề", arena_topic_btn_rect_, L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 236, 238, 244), 1, 1, false);

        // 5. Toggle button (Start / Pause debate): [topX - 143 ... topX - 8]
        float togW = 135.0f;
        float togX = topX - togW - 8.0f;
        arena_toggle_btn_rect_ = Gdiplus::RectF(togX, 10.0f, togW, 30.0f);

        Gdiplus::Color togBg;
        Gdiplus::Color togBorder;
        std::wstring togText;
        if (arena_running_) {
            togBg = hover_btn_arena_toggle_ ? Gdiplus::Color(255, 185, 28, 28) : Gdiplus::Color(255, 153, 27, 27);
            togBorder = Gdiplus::Color(255, 239, 68, 68);
            togText = L"⏸ Tạm dừng";
        } else {
            togBg = hover_btn_arena_toggle_ ? Gdiplus::Color(255, 109, 40, 217) : Gdiplus::Color(255, 88, 28, 135);
            togBorder = hover_btn_arena_toggle_ ? Gdiplus::Color(255, 192, 132, 252) : Gdiplus::Color(255, 147, 51, 234);
            togText = L"▶ Bắt đầu Thảo luận";
        }
        draw_rounded_box(g, arena_toggle_btn_rect_, 8.0f, togBg, togBorder, 1.0f);
        draw_text_dw(g, togText, arena_toggle_btn_rect_, L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);

        // Place Security Badge safely to the left of Arena buttons, never overlapping!
        float badgeX = togX - badgeW - 10.0f;
        if (badgeX > sessionRect.X + sessionRect.Width + 10.0f) {
            security_badge_rect_ = Gdiplus::RectF(badgeX, 10.0f, badgeW, 30.0f);
        } else {
            security_badge_rect_ = Gdiplus::RectF(0, 0, 0, 0); // Hide if window too narrow to avoid collision
        }
    } else {
        // Settings or Workspace tab: Security Badge on the right
        float badgeX = chat_right - badgeW - 14.0f;
        if (badgeX > sessionRect.X + sessionRect.Width + 10.0f) {
            security_badge_rect_ = Gdiplus::RectF(badgeX, 10.0f, badgeW, 30.0f);
        } else {
            security_badge_rect_ = Gdiplus::RectF(0, 0, 0, 0);
        }
    }

    if (security_badge_rect_.Width > 0.0f) {
        Gdiplus::Color badgeBg = isOnline ? Gdiplus::Color(255, 14, 38, 64) : Gdiplus::Color(255, 6, 48, 36);
        Gdiplus::Color badgeBorder = isOnline ? Gdiplus::Color(255, 14, 165, 233) : Gdiplus::Color(255, 16, 185, 129);
        Gdiplus::Color badgeText = isOnline ? Gdiplus::Color(255, 56, 189, 248) : Gdiplus::Color(255, 52, 211, 153);

        draw_rounded_box(g, security_badge_rect_, 15.0f, badgeBg, badgeBorder, 1.2f);
        draw_text_dw(g, badgeStr, Gdiplus::RectF(security_badge_rect_.X + 8.0f, 10.0f, security_badge_rect_.Width - 16.0f, 30.0f),
                     L"Segoe UI", 8.5f, true, badgeText, 1, 1, false);
    }

    // 3. Interactive SEB Anti-Screenshot Shield Badge (1-Click Toggle)
    auto antiSt = security::AntiScreenshot::instance().get_status();
    float antiBadgeW = 140.0f;
    float anchorRight = (security_badge_rect_.Width > 0.0f) ? security_badge_rect_.X :
                        (current_tab_ == ActiveTab::CHAT ? (chat_right - 74.0f) :
                         (current_tab_ == ActiveTab::ARENA ? arena_toggle_btn_rect_.X : chat_right - 14.0f));
    float antiBadgeX = anchorRight - antiBadgeW - 10.0f;

    if (antiBadgeX > sessionRect.X + sessionRect.Width + 10.0f) {
        anti_capture_badge_rect_ = Gdiplus::RectF(antiBadgeX, 10.0f, antiBadgeW, 30.0f);

        Gdiplus::Color antiBg = antiSt.enabled ?
            (antiSt.capture_tool_detected ? Gdiplus::Color(255, 69, 10, 10) : Gdiplus::Color(255, 6, 48, 36)) :
            Gdiplus::Color(255, 48, 35, 10);

        Gdiplus::Color antiBorder = antiSt.enabled ?
            (antiSt.capture_tool_detected ? Gdiplus::Color(255, 239, 68, 68) : Gdiplus::Color(255, 16, 185, 129)) :
            Gdiplus::Color(255, 245, 158, 11);

        Gdiplus::Color antiText = antiSt.enabled ?
            (antiSt.capture_tool_detected ? Gdiplus::Color(255, 248, 113, 113) : Gdiplus::Color(255, 52, 211, 153)) :
            Gdiplus::Color(255, 251, 191, 36);

        draw_rounded_box(g, anti_capture_badge_rect_, 15.0f, antiBg, antiBorder, 1.2f);

        std::wstring antiStr;
        if (antiSt.capture_tool_detected) {
            antiStr = L"🚨 PHÁT HIỆN TOOL";
        } else if (antiSt.enabled) {
            antiStr = L"🔒 CHỐNG CHỤP: BẬT";
        } else {
            antiStr = L"⚠️ CHỐNG CHỤP: TẮT";
        }

        draw_text_dw(g, antiStr, Gdiplus::RectF(anti_capture_badge_rect_.X + 6.0f, 10.0f, anti_capture_badge_rect_.Width - 12.0f, 30.0f),
                     L"Segoe UI", 8.5f, true, antiText, 1, 1, false);
    } else {
        anti_capture_badge_rect_ = Gdiplus::RectF(0, 0, 0, 0);
    }
}

void NativeGui::render_right_panel(Gdiplus::Graphics& g, const RECT& rc) {
    if (right_panel_cur_w_ <= 1.0f) return;

    float pW = 310.0f;
    float pX = (float)rc.right - right_panel_cur_w_;
    float pH = (float)rc.bottom;

    set_render_clip(pX, 0.0f, pW, pH);

    d2d_fill_rect(pX, 0.0f, pW, pH, Gdiplus::Color(255, 20, 21, 25)); // #141519
    d2d_draw_line(pX, 0.0f, pX, pH, Gdiplus::Color(255, 34, 35, 42), 1.0f);

    // Drawer collapse button on top-left of drawer header (clean separation from window controls)
    float drwCloseX = pX + 16.0f;
    float drwCloseY = 12.0f;
    btn_drawer_collapse_rect_ = Gdiplus::RectF(drwCloseX, drwCloseY, 26.0f, 26.0f);
    if (hover_btn_drawer_collapse_) {
        draw_rounded_box(g, btn_drawer_collapse_rect_, 6.0f, Gdiplus::Color(255, 38, 40, 50), Gdiplus::Color(255, 58, 60, 72), 1.0f);
    }
    Gdiplus::Color drwCol = hover_btn_drawer_collapse_ ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 142, 146, 158);
    // Draw right collapse arrow '→|'
    d2d_draw_line(drwCloseX + 6.0f, drwCloseY + 13.0f, drwCloseX + 16.0f, drwCloseY + 13.0f, drwCol, 1.4f);
    d2d_draw_line(drwCloseX + 12.0f, drwCloseY + 9.0f, drwCloseX + 16.0f, drwCloseY + 13.0f, drwCol, 1.4f);
    d2d_draw_line(drwCloseX + 12.0f, drwCloseY + 17.0f, drwCloseX + 16.0f, drwCloseY + 13.0f, drwCol, 1.4f);
    d2d_draw_line(drwCloseX + 19.0f, drwCloseY + 8.0f, drwCloseX + 19.0f, drwCloseY + 18.0f, drwCol, 1.4f);

    // Large Circular Muse Avatar (diameter 82px)
    float avSize = 82.0f;
    float avX = pX + (pW - avSize) / 2.0f;
    float avY = 38.0f;

    // Animated pulsing neon halo around Muse avatar when streaming or Arena is active
    if (is_chat_generating_ || arena_running_) {
        float pulse = (std::sin(pulse_phase_) + 1.0f) * 0.5f;
        float ringPad = 3.0f + pulse * 5.0f;
        int alphaOuter = (int)(160.0f - pulse * 110.0f);
        d2d_draw_ellipse(avX - ringPad, avY - ringPad, avSize + ringPad * 2.0f, avSize + ringPad * 2.0f, Gdiplus::Color(alphaOuter, 168, 85, 247), 2.5f);
        d2d_draw_ellipse(avX - 2.0f, avY - 2.0f, avSize + 4.0f, avSize + 4.0f, Gdiplus::Color(220, 139, 92, 246), 1.8f);
    }

    draw_circle_avatar(g, muse_avatar_img_, avX, avY, avSize, L"M");

    // Dark circular edit pencil badge at bottom-right of avatar
    float penBoxSize = 26.0f;
    float penX = avX + avSize - 25.0f;
    float penY = avY + avSize - 25.0f;
    Gdiplus::RectF pencilRect(penX, penY, penBoxSize, penBoxSize);
    draw_rounded_box(g, pencilRect, 13.0f, Gdiplus::Color(255, 38, 40, 48), Gdiplus::Color(255, 58, 60, 72), 1.0f);

    float pcx = penX + 13.0f, pcy = penY + 13.0f;
    d2d_draw_line(pcx - 3.5f, pcy + 3.5f, pcx + 3.5f, pcy - 3.5f, Gdiplus::Color(255, 210, 214, 224), 1.5f);
    d2d_draw_line(pcx - 4.5f, pcy + 4.5f, pcx - 2.0f, pcy + 4.0f, Gdiplus::Color(255, 210, 214, 224), 1.5f);

    // Name: Muse
    draw_text_dw(g, L"Muse", Gdiplus::RectF(pX, 128.0f, pW, 26.0f),
                 L"Segoe UI", 14.5f, true, Gdiplus::Color(255, 244, 245, 248), 1, 1, false);

    // Status row: Animated wave bars when streaming, or Green circle + "Đã kết nối" when idle
    if (is_chat_generating_ || arena_running_) {
        float stX = pX + (pW - 142.0f) / 2.0f;
        float stY = 158.0f;
        for (int b = 0; b < 3; ++b) {
            float barH = 5.0f + 7.0f * std::abs(std::sin(pulse_phase_ + b * 1.1f));
            float bx = stX + b * 4.5f;
            float by = stY + 8.5f - barH * 0.5f;
            d2d_fill_rect(bx, by, 2.8f, barH, Gdiplus::Color(255, 168, 85, 247));
        }
        draw_text_dw(g, L"Đang stream...", Gdiplus::RectF(stX + 18.0f, stY, 130.0f, 20.0f),
                     L"Segoe UI", 9.5f, true, Gdiplus::Color(255, 192, 132, 252), 0, 0, false);
    } else {
        float stX = pX + (pW - 92.0f) / 2.0f;
        float stY = 158.0f;
        d2d_fill_ellipse(stX, stY + 2.0f, 13.0f, 13.0f, Gdiplus::Color(255, 74, 222, 128));

        Gdiplus::Color boltCol(255, 20, 21, 25);
        d2d_draw_line(stX + 7.5f, stY + 4.5f, stX + 5.0f, stY + 8.5f, boltCol, 1.4f);
        d2d_draw_line(stX + 5.0f, stY + 8.5f, stX + 8.0f, stY + 8.5f, boltCol, 1.4f);
        d2d_draw_line(stX + 8.0f, stY + 8.5f, stX + 5.5f, stY + 12.5f, boltCol, 1.4f);

        draw_text_dw(g, L"Đã kết nối", Gdiplus::RectF(stX + 18.0f, stY, 90.0f, 20.0f),
                     L"Segoe UI", 9.5f, false, Gdiplus::Color(255, 148, 152, 166), 0, 0, false);
    }

    // Outer Pill Container for 4 Segmented Tabs
    Gdiplus::RectF segOuter(pX + 18.0f, 192.0f, pW - 36.0f, 34.0f);
    draw_rounded_box(g, segOuter, 17.0f, Gdiplus::Color(255, 25, 26, 31), Gdiplus::Color(255, 42, 44, 52), 1.0f);

    float segW = (segOuter.Width - 8.0f) / 4.0f;
    float targetTabX = segOuter.X + 4.0f + (right_panel_tab_ * segW);
    right_tab_target_x_ = targetTabX;
    if (right_tab_indicator_x_ <= 1.0f) {
        right_tab_indicator_x_ = targetTabX;
    }

    // Smooth gliding active tab indicator pill
    Gdiplus::RectF glideTab(right_tab_indicator_x_, segOuter.Y + 3.0f, segW, 28.0f);
    draw_rounded_box(g, glideTab, 14.0f, Gdiplus::Color(255, 68, 70, 80), Gdiplus::Color(0, 0, 0, 0), 0.0f);

    for (int t = 0; t < 4; ++t) {
        float sx = segOuter.X + 4.0f + (t * segW);
        Gdiplus::RectF tabRect(sx, segOuter.Y + 3.0f, segW, 28.0f);
        bool isAct = (right_panel_tab_ == t);
        bool isHov = (hover_right_tab_ == t && !isAct);

        if (isHov) {
            draw_rounded_box(g, tabRect, 14.0f, Gdiplus::Color(255, 38, 40, 48), Gdiplus::Color(0, 0, 0, 0), 0.0f);
        }

        if (t > 0 && right_panel_tab_ != t && right_panel_tab_ != (t - 1)) {
            d2d_draw_line(sx, segOuter.Y + 9.0f, sx, segOuter.Y + 25.0f, Gdiplus::Color(255, 45, 47, 56), 1.0f);
        }

        Gdiplus::Color icCol = isAct ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 148, 152, 166);
        draw_right_tab_vector_icon(g, t, sx + segW / 2.0f, segOuter.Y + 17.0f, icCol);
    }

    if (right_panel_tab_ == 0) {
        draw_text_dw(g, L"Hôm nay", Gdiplus::RectF(pX + 18.0f, 242.0f, 200.0f, 24.0f),
                     L"Segoe UI", 10.5f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);

        set_render_clip(pX, 270.0f, pW, pH - 275.0f);
        float itemY = 274.0f - right_panel_scroll_y_;

        for (size_t idx = 0; idx < today_tasks_.size(); ++idx) {
            const auto& task = today_tasks_[idx];
            std::wstring wDesc = s2ws(task.desc);
            D2D1_SIZE_F descSz = measure_text_dw(wDesc, 222.0f, L"Segoe UI", 9.0f, false, true);
            float descH = max(18.0f, descSz.height);
            float rowH = 22.0f + descH + 22.0f;

            if (itemY + rowH >= 265.0f && itemY <= pH) {
                if ((int)idx == hover_task_idx_) {
                    draw_rounded_box(g, Gdiplus::RectF(pX + 10.0f, itemY - 4.0f, pW - 24.0f, rowH + 4.0f),
                                     8.0f, Gdiplus::Color(255, 28, 30, 36), Gdiplus::Color(0,0,0,0), 0.0f);
                }

                float boxX = pX + 18.0f;
                float boxY = itemY + 2.0f;
                draw_rounded_box(g, Gdiplus::RectF(boxX, boxY, 36.0f, 36.0f), 10.0f,
                                 Gdiplus::Color(255, 34, 36, 43), Gdiplus::Color(255, 46, 48, 58), 1.0f);

                Gdiplus::Color icCol(255, 212, 216, 226);
                float icx = boxX + 18.0f, icy = boxY + 18.0f;
                if (task.is_done) {
                    d2d_draw_ellipse(icx - 7.5f, icy - 7.5f, 15.0f, 15.0f, icCol, 1.5f);
                    d2d_draw_line(icx - 3.2f, icy + 0.2f, icx - 0.6f, icy + 2.8f, icCol, 1.5f);
                    d2d_draw_line(icx - 0.6f, icy + 2.8f, icx + 3.8f, icy - 2.2f, icCol, 1.5f);
                } else {
                    draw_rounded_box(g, Gdiplus::RectF(icx - 6.0f, icy - 7.5f, 12.0f, 15.0f), 2.0f,
                                     Gdiplus::Color(0,0,0,0), Gdiplus::Color(255, 168, 172, 184), 1.4f);
                    d2d_draw_line(icx - 3.0f, icy - 2.5f, icx + 3.0f, icy - 2.5f, icCol, 1.5f);
                    d2d_draw_line(icx - 3.0f, icy + 1.5f, icx + 2.0f, icy + 1.5f, icCol, 1.5f);
                }

                float tx = pX + 66.0f;
                draw_text_dw(g, s2ws(task.title), Gdiplus::RectF(tx, itemY, 224.0f, 20.0f),
                             L"Segoe UI", 9.5f, true, Gdiplus::Color(255, 240, 242, 247), 0, 0, false);
                draw_text_dw(g, wDesc, Gdiplus::RectF(tx, itemY + 20.0f, 222.0f, descH + 6.0f),
                             L"Segoe UI", 9.0f, false, Gdiplus::Color(255, 148, 152, 166), 0, 0, true);
                draw_text_dw(g, s2ws(task.time), Gdiplus::RectF(tx, itemY + 21.0f + descH, 120.0f, 18.0f),
                             L"Segoe UI", 8.0f, false, Gdiplus::Color(255, 112, 116, 130), 0, 0, false);
            }

            itemY += rowH + 10.0f;
        }

        clear_render_clip();

        draw_rounded_box(g, Gdiplus::RectF(pX + pW - 6.0f, 270.0f + (right_panel_scroll_y_ * 0.3f), 3.5f, 160.0f),
                         1.7f, Gdiplus::Color(255, 48, 50, 58), Gdiplus::Color(0,0,0,0), 0.0f);
    }
    else if (right_panel_tab_ == 1) {
        draw_text_dw(g, L"Đặc điểm Sandbox Thực tế Đang chạy", Gdiplus::RectF(pX + 18.0f, 242.0f, 274.0f, 24.0f),
                     L"Segoe UI", 10.2f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);

        auto status = sandbox_mgr_->get_status();
        draw_rounded_box(g, Gdiplus::RectF(pX + 18.0f, 270.0f, pW - 36.0f, 208.0f), 10.0f,
                         Gdiplus::Color(255, 28, 30, 36), Gdiplus::Color(255, 45, 47, 56), 1.0f);

        float sy = 280.0f;
        float sx = pX + 28.0f;
        float sw = pW - 56.0f;

        if (status.engine_type == "Win32_Jail") {
            draw_text_dw(g, L"● Engine: Win32_Jail (Đang hoạt động)", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.8f, true, Gdiplus::Color(255, 74, 222, 128), 0, 0, false);
            sy += 24.0f;
            draw_text_dw(g, L"• Thư mục: " + s2ws(status.workspace_path) + L" (Chặn ..)", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 226, 232, 240), 0, 0, false);
            sy += 23.0f;
            draw_text_dw(g, L"• Token: Low-Integrity (S-1-16-4096)", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 192, 132, 252), 0, 0, false);
            sy += 23.0f;
            draw_text_dw(g, L"• ACL: S:(ML;;NW;;;LW) + LUA_TOKEN", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 148, 152, 166), 0, 0, false);
            sy += 23.0f;
            std::wstring jobSpec = L"• JobObject: RAM " + std::to_wstring(status.memory_limit_bytes / (1024 * 1024)) + L" MB | Max 32 PIDs";
            draw_text_dw(g, jobSpec, Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 226, 232, 240), 0, 0, false);
            sy += 23.0f;
            draw_text_dw(g, L"• UI Guard: Chặn Clipboard, Desktop, Handles", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 148, 152, 166), 0, 0, false);
            sy += 23.0f;
            draw_text_dw(g, L"• Cô lập: .appdata, .tmp, .bin (Chặn winget)", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 148, 152, 166), 0, 0, false);
            sy += 23.0f;
        } else {
            draw_text_dw(g, L"● Engine: Docker (" + s2ws(status.container_id) + L")", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.8f, true, Gdiplus::Color(255, 74, 222, 128), 0, 0, false);
            sy += 24.0f;
            draw_text_dw(g, L"• Pipe: \\\\.\\pipe\\docker_engine", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 192, 132, 252), 0, 0, false);
            sy += 23.0f;
            draw_text_dw(g, L"• Image: ubuntu:22.04 (" + s2ws(status.workspace_path) + L")", Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 226, 232, 240), 0, 0, false);
            sy += 23.0f;
            std::wstring dkSpec = L"• Giới hạn: RAM " + std::to_wstring(status.memory_limit_bytes / (1024 * 1024)) + L" MB | 2 CPU | 128 PIDs";
            draw_text_dw(g, dkSpec, Gdiplus::RectF(sx, sy, sw, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 226, 232, 240), 0, 0, false);
            sy += 23.0f;
        }

        std::wstring netMode = status.internet_enabled
            ? L"• Mạng: ONLINE (Cho phép kết nối)"
            : L"• Mạng: AIR-GAP (Chặn HTTP/Proxy 0/Firewall)";
        draw_text_dw(g, netMode, Gdiplus::RectF(sx, sy, sw, 20.0f),
                     L"Segoe UI", 8.5f, true,
                     status.internet_enabled ? Gdiplus::Color(255, 74, 222, 128) : Gdiplus::Color(255, 248, 113, 113), 0, 0, false);

        Gdiplus::RectF btnAir(pX + 18.0f, 490.0f, pW - 36.0f, 36.0f);
        draw_rounded_box(g, btnAir, 10.0f, Gdiplus::Color(255, 44, 46, 56), Gdiplus::Color(255, 68, 72, 88), 1.0f);
        draw_text_dw(g, status.internet_enabled ? L"Chuyển sang chế độ AIR-GAP" : L"Bật lại kết nối ONLINE",
                     btnAir, L"Segoe UI", 10.0f, true, Gdiplus::Color(255, 241, 245, 249), 1, 1, false);

        // SEB Anti-Screenshot Card
        auto antiStatus = security::AntiScreenshot::instance().get_status();
        float sebY = 538.0f;
        Gdiplus::RectF sebCard(pX + 18.0f, sebY, pW - 36.0f, 134.0f);
        Gdiplus::Color sebCardBg = antiStatus.enabled ? Gdiplus::Color(255, 12, 28, 22) : Gdiplus::Color(255, 30, 24, 16);
        Gdiplus::Color sebCardBorder = antiStatus.enabled ? Gdiplus::Color(255, 16, 185, 129) : Gdiplus::Color(255, 245, 158, 11);
        draw_rounded_box(g, sebCard, 10.0f, sebCardBg, sebCardBorder, 1.0f);

        draw_text_dw(g, L"🛡️ BẢO VỆ CHỐNG CHỤP SEB", Gdiplus::RectF(pX + 28.0f, sebY + 10.0f, pW - 56.0f, 20.0f),
                     L"Segoe UI", 9.5f, true, antiStatus.enabled ? Gdiplus::Color(255, 52, 211, 153) : Gdiplus::Color(255, 251, 191, 36), 0, 0, false);

        std::wstring wdaInfo = L"• DWM Affinity: " + std::wstring(antiStatus.wda_active ? L"WDA_EXCLUDEFROMCAPTURE" : L"None");
        draw_text_dw(g, wdaInfo, Gdiplus::RectF(pX + 28.0f, sebY + 32.0f, pW - 56.0f, 18.0f),
                     L"Segoe UI", 8.2f, false, Gdiplus::Color(255, 209, 213, 219), 0, 0, false);

        std::wstring blockInfo = L"• Chặn: PrtScn, Win+Shift+S, Win+G";
        draw_text_dw(g, blockInfo, Gdiplus::RectF(pX + 28.0f, sebY + 50.0f, pW - 56.0f, 18.0f),
                     L"Segoe UI", 8.2f, false, Gdiplus::Color(255, 156, 163, 175), 0, 0, false);

        std::wstring statInfo = L"• Đã chặn: " + std::to_wstring(antiStatus.blocked_snapshot_count) + L" lần | Wipe: " + std::to_wstring(antiStatus.clipboard_wipe_count);
        draw_text_dw(g, statInfo, Gdiplus::RectF(pX + 28.0f, sebY + 68.0f, pW - 56.0f, 18.0f),
                     L"Segoe UI", 8.2f, false, Gdiplus::Color(255, 156, 163, 175), 0, 0, false);

        Gdiplus::RectF btnSeb(pX + 26.0f, sebY + 92.0f, pW - 52.0f, 32.0f);
        Gdiplus::Color btnSebBg = antiStatus.enabled ? Gdiplus::Color(255, 6, 78, 59) : Gdiplus::Color(255, 120, 53, 15);
        draw_rounded_box(g, btnSeb, 8.0f, btnSebBg, sebCardBorder, 1.0f);
        draw_text_dw(g, antiStatus.enabled ? L"Tắt Chế Độ SEB Chống Chụp" : L"Bật Chế Độ SEB Chống Chụp",
                     btnSeb, L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 241, 245, 249), 1, 1, false);
    }
    else if (right_panel_tab_ == 2) {
        draw_text_dw(g, L"Nhật ký Hoạt động Thời gian thực", Gdiplus::RectF(pX + 18.0f, 242.0f, 260.0f, 24.0f),
                     L"Segoe UI", 10.5f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);
        float ly = 274.0f;
        for (const auto& entry : timeline_logs_) {
            draw_rounded_box(g, Gdiplus::RectF(pX + 18.0f, ly, pW - 36.0f, 38.0f), 8.0f,
                             Gdiplus::Color(255, 28, 30, 36), Gdiplus::Color(255, 42, 44, 52), 1.0f);
            draw_text_dw(g, s2ws(entry), Gdiplus::RectF(pX + 28.0f, ly, pW - 56.0f, 38.0f),
                         L"Segoe UI", 8.8f, false, Gdiplus::Color(255, 148, 152, 166), 0, 1, false);
            ly += 46.0f;
        }
    }
    else if (right_panel_tab_ == 3) {
        draw_text_dw(g, L"Hệ thống Đa Agent & AI", Gdiplus::RectF(pX + 18.0f, 242.0f, 250.0f, 24.0f),
                     L"Segoe UI", 10.5f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);
        auto cfg = get_active_config();
        draw_rounded_box(g, Gdiplus::RectF(pX + 18.0f, 272.0f, pW - 36.0f, 90.0f), 10.0f,
                         Gdiplus::Color(255, 28, 30, 36), Gdiplus::Color(255, 45, 47, 56), 1.0f);
        draw_text_dw(g, L"Provider: " + s2ws(cfg.name), Gdiplus::RectF(pX + 28.0f, 286.0f, pW - 56.0f, 22.0f),
                     L"Segoe UI", 9.0f, false, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);
        draw_text_dw(g, L"Model: " + s2ws(cfg.model), Gdiplus::RectF(pX + 28.0f, 314.0f, pW - 56.0f, 22.0f),
                     L"Segoe UI", 9.0f, false, Gdiplus::Color(255, 192, 132, 252), 0, 0, false);

        Gdiplus::RectF btnArena(pX + 18.0f, 378.0f, pW - 36.0f, 36.0f);
        draw_rounded_box(g, btnArena, 10.0f, Gdiplus::Color(255, 88, 28, 135), Gdiplus::Color(255, 147, 51, 234), 1.0f);
        draw_text_dw(g, L"Mở Phòng Tranh Luận Đa Agent", btnArena,
                     L"Segoe UI", 10.0f, true, Gdiplus::Color(255, 241, 245, 249), 1, 1, false);
    }
    clear_render_clip();
}

void NativeGui::render_chat_view(Gdiplus::Graphics& g, const RECT& rc) {
    int chat_left = 48;
    int right_panel_w = (int)right_panel_cur_w_;
    int chat_right = rc.right - right_panel_w;
    int chat_w = chat_right - chat_left;

    code_hitboxes_.clear();
    reasoning_hitboxes_.clear();
    int blockCounter = 0;

    set_render_clip((float)chat_left, 46.0f, (float)chat_w, (float)(rc.bottom - 46 - 72));

    // Center column layout matching Screenshot 1, 2 & 5 (max column width ~620px centered in chat canvas)
    float colW = (float)min(chat_w - 60, 620);
    float colX = (float)chat_left + ((float)chat_w - colW) / 2.0f;

    int startY = 54 - chat_scroll_y_;
    int curY = startY;

    Gdiplus::FontFamily fontSegoe(L"Segoe UI");
    Gdiplus::FontFamily fontConsolas(L"Consolas");
    Gdiplus::Font fontText(&fontSegoe, 10.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::Font fontBold(&fontSegoe, 10.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fontMonoInline(&fontConsolas, 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::Font fontCode(&fontConsolas, 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

    std::lock_guard<std::mutex> lock(chat_mutex_);

    for (size_t i = 0; i < chat_messages_.size(); ++i) {
        const auto& msg = chat_messages_[i];
        bool isUser = (msg.sender == "user");

        if (isUser) {
            std::wstring wText = s2ws(msg.text);
            if (std::abs(msg.cached_col_w - colW) > 0.5f || msg.cached_text_len != msg.text.size()) {
                D2D1_SIZE_F textSz = measure_text_dw(wText, colW * 0.72f, L"Segoe UI", 10.0f, false, true);
                msg.cached_user_w = min(colW * 0.75f, max(160.0f, textSz.width + 34.0f));
                msg.cached_user_h = max(36.0f, textSz.height + 18.0f);
                msg.cached_col_w = colW;
                msg.cached_text_len = msg.text.size();
            }
            float bubbleW = msg.cached_user_w;
            float bubbleH = msg.cached_user_h;
            float extraAttachH = msg.attachment_title.empty() ? 0.0f : 44.0f;
            float totalUserH = bubbleH + extraAttachH;
            float rightEdge = colX + colW;

            if (curY + totalUserH >= 40 && curY <= rc.bottom - 60) {
                float curBoxY = (float)curY;

                // Lavender Attachment Preview Thumbnail Pill (Matches Screenshot 1, 2 & 5 1:1!)
                if (!msg.attachment_title.empty()) {
                    float attW = 198.0f;
                    float attH = 38.0f;
                    Gdiplus::RectF attRect(rightEdge - attW, curBoxY, attW, attH);
                    draw_rounded_box(g, attRect, 18.0f,
                                     Gdiplus::Color(255, 224, 206, 250), // Soft lavender #e0cefa
                                     Gdiplus::Color(255, 255, 255, 255),
                                     1.5f);

                    draw_text_dw(g, s2ws("🍴 " + msg.attachment_title),
                                 Gdiplus::RectF(attRect.X, attRect.Y + 4.0f, attW, 16.0f),
                                 L"Segoe UI", 7.5f, true, Gdiplus::Color(255, 49, 46, 129), 1, 1, false);

                    draw_text_dw(g, s2ws(msg.attachment_sub),
                                 Gdiplus::RectF(attRect.X + 8.0f, attRect.Y + 20.0f, attW - 16.0f, 14.0f),
                                 L"Segoe UI", 5.5f, false, Gdiplus::Color(255, 107, 114, 128), 1, 1, false);

                    curBoxY += 44.0f;
                }

                // Deep Wine Red / Burgundy Pill Bubble (#5c1d24, border #782630)
                Gdiplus::RectF cardRect(rightEdge - bubbleW, curBoxY, bubbleW, bubbleH);
                draw_rounded_box(g, cardRect, 18.0f,
                                 Gdiplus::Color(255, 92, 29, 36),   // #5c1d24
                                 Gdiplus::Color(255, 120, 38, 48),  // #782630
                                 1.0f);

                draw_text_dw(g, wText,
                             Gdiplus::RectF(cardRect.X + 16.0f, curBoxY + 9.0f, bubbleW - 32.0f, bubbleH - 12.0f),
                             L"Segoe UI", 10.0f, false, Gdiplus::Color(255, 248, 249, 250), 0, 0, true);
            }

            curY += (int)totalUserH + 16;
        } else {
            // MUSE BUBBLE CARD: Dark Rounded Card (#1c1d22, radius 14px) matching Screenshot 1, 2 & 5!
            float bubbleW = colW - 36.0f;
            float innerW = bubbleW - 32.0f;

            bool showCursor = msg.is_streaming && msg.executed_cmds.empty() && ((anim_frame_ % 18) < 12);

            // Recompute cached markdown blocks & heights ONLY if width, text length, reasoning length, or streaming state changed
            bool cacheDirty = (std::abs(msg.cached_col_w - colW) > 0.5f) ||
                              (msg.cached_text_len != msg.text.size()) ||
                              (msg.cached_reasoning_len != msg.reasoning.size()) ||
                              (msg.cached_cmds_count != msg.executed_cmds.size()) ||
                              (msg.cached_was_streaming != msg.is_streaming) ||
                              (msg.cached_reasoning_collapsed != msg.reasoning_collapsed) ||
                              (msg.cached_card_h <= 0.0f);

            if (cacheDirty) {
                msg.cached_parts = parse_markdown_blocks(msg.text);

                float rBoxH = 0.0f;
                if (!msg.reasoning.empty()) {
                    if (msg.reasoning_collapsed) {
                        rBoxH = 32.0f;
                    } else {
                        std::wstring wR = s2ws(msg.reasoning + (msg.is_streaming && msg.text.empty() ? " ▋" : ""));
                        D2D1_SIZE_F rSz = measure_text_dw(wR, innerW - 24.0f, L"Segoe UI", 9.0f, false, true);
                        rBoxH = 30.0f + max(18.0f, rSz.height) + 16.0f;
                    }
                }
                msg.cached_reasoning_h = rBoxH;

                float cH = 28.0f;
                if (rBoxH > 0.0f) {
                    cH += rBoxH + 12.0f;
                }

                if (msg.is_streaming && msg.text.empty() && msg.reasoning.empty()) {
                    cH = 52.0f;
                } else {
                    for (const auto& p : msg.cached_parts) {
                        if (p.is_code) {
                            int codeLines = (int)std::count(p.text.begin(), p.text.end(), '\n') + 1;
                            cH += 34.0f + (codeLines * 19.0f) + 16.0f;
                        } else {
                            std::stringstream lss(p.text);
                            std::string ln;
                            while (std::getline(lss, ln)) {
                                if (!ln.empty() && ln.back() == '\r') ln.pop_back();
                                if (ln.empty()) {
                                    cH += 10.0f;
                                } else {
                                    cH += measure_rich_text_line(ln, innerW) + 2.0f;
                                }
                            }
                            cH += 6.0f;
                        }
                    }
                }

                for (const auto& cmd : msg.executed_cmds) {
                    std::wstring wCmd = s2ws("⚡ Sandbox Executed: " + cmd);
                    D2D1_SIZE_F cmdSz = measure_text_dw(wCmd, innerW - 20.0f, L"Consolas", 8.5f, false, true);
                    float badgeH = max(26.0f, cmdSz.height + 12.0f);
                    cH += badgeH + 8.0f;
                }

                msg.cached_card_h = cH;
                msg.cached_col_w = colW;
                msg.cached_text_len = msg.text.size();
                msg.cached_reasoning_len = msg.reasoning.size();
                msg.cached_cmds_count = msg.executed_cmds.size();
                msg.cached_was_streaming = msg.is_streaming;
                msg.cached_reasoning_collapsed = msg.reasoning_collapsed;
            }

            float cardH = msg.cached_card_h;
            float reasoningBoxH = msg.cached_reasoning_h;

            if (curY + cardH >= 40 && curY <= rc.bottom - 60) {
                Gdiplus::RectF museCard(colX, (float)curY, bubbleW, cardH);

                // Subtle glowing violet border when actively streaming
                Gdiplus::Color cardBorderCol(255, 38, 40, 47);
                float cardBorderW = 1.0f;
                if (msg.is_streaming) {
                    float pulse = (std::sin(pulse_phase_) + 1.0f) * 0.5f;
                    int rCol = (int)(70 + pulse * 69);
                    int gCol = (int)(45 + pulse * 47);
                    int bCol = (int)(110 + pulse * 136);
                    cardBorderCol = Gdiplus::Color(255, rCol, gCol, bCol);
                    cardBorderW = 1.4f;
                }

                draw_rounded_box(g, museCard, 14.0f,
                                 Gdiplus::Color(255, 28, 29, 34),  // #1c1d22 (Exact Muse bubble dark surface)
                                 cardBorderCol,
                                 cardBorderW);

                float partY = (float)curY + 14.0f;
                float partX = colX + 16.0f;

                // 1. Animated Thinking Wave (● ● ●) when waiting for first token
                if (msg.is_streaming && msg.text.empty() && msg.reasoning.empty()) {
                    for (int d = 0; d < 3; ++d) {
                        float wave = std::sin(pulse_phase_ * 1.4f - d * 0.85f);
                        float dy = partY + 7.0f - (wave * 3.5f);
                        int alpha = (int)(150 + 105 * ((wave + 1.0f) * 0.5f));
                        d2d_fill_ellipse(partX + (d * 13.0f), dy, 7.5f, 7.5f, Gdiplus::Color(alpha, 168, 85, 247));
                    }
                    draw_text_dw(g, L"Muse đang suy nghĩ...",
                                 Gdiplus::RectF(partX + 46.0f, partY + 1.0f, 220.0f, 22.0f),
                                 L"Segoe UI", 9.5f, false, Gdiplus::Color(255, 168, 172, 186), 0, 1, false);
                    partY += 28.0f;
                }

                // 2. Live Reasoning / Thought Stream Box (<think> or reasoning_content)
                if (reasoningBoxH > 0.0f) {
                    Gdiplus::RectF rBox(partX, partY, innerW, reasoningBoxH);
                    draw_rounded_box(g, rBox, 8.0f,
                                     Gdiplus::Color(255, 23, 24, 30),
                                     Gdiplus::Color(255, 68, 52, 108), 1.0f);

                    // Register header hitbox for collapsing / expanding
                    reasoning_hitboxes_.push_back({ Gdiplus::RectF(partX, partY, innerW, 30.0f), i });

                    std::wstring headerTitle = (msg.is_streaming && msg.text.empty()) ?
                        L"💭 Đang suy luận trực tiếp..." :
                        (msg.reasoning_collapsed ? L"💭 Đã hoàn thành suy luận" : L"💭 Luồng suy luận");

                    draw_text_dw(g, headerTitle,
                                 Gdiplus::RectF(partX + 12.0f, partY + 6.0f, innerW - 100.0f, 18.0f),
                                 L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 192, 132, 252), 0, 0, false);

                    draw_text_dw(g, msg.reasoning_collapsed ? L"Mở rộng ▼" : L"Thu gọn ▲",
                                 Gdiplus::RectF(partX + innerW - 80.0f, partY + 6.0f, 68.0f, 18.0f),
                                 L"Segoe UI", 8.0f, false, Gdiplus::Color(255, 168, 85, 247), 2, 0, false);

                    if (!msg.reasoning_collapsed) {
                        std::string rDisp = msg.reasoning;
                        if (msg.is_streaming && msg.text.empty() && showCursor) rDisp += " ▋";
                        std::wstring wReasoning = s2ws(rDisp);

                        draw_text_dw(g, wReasoning,
                                     Gdiplus::RectF(partX + 12.0f, partY + 28.0f, innerW - 24.0f, reasoningBoxH - 36.0f),
                                     L"Segoe UI", 9.0f, false, Gdiplus::Color(255, 156, 163, 175), 0, 0, true);
                    }
                    partY += reasoningBoxH + 12.0f;
                }

                // 3. Main Markdown & Code Blocks (Streaming token-by-token)
                for (size_t pIdx = 0; pIdx < msg.cached_parts.size(); ++pIdx) {
                    const auto& p = msg.cached_parts[pIdx];
                    if (p.is_code) {
                        int thisBlockId = blockCounter++;
                        int codeLines = (int)std::count(p.text.begin(), p.text.end(), '\n') + 1;
                        float blockH = 32.0f + (codeLines * 19.0f) + 8.0f;

                        if (partY + blockH >= 40.0f && partY <= rc.bottom - 55.0f) {
                            Gdiplus::RectF codeBoxRect(partX, partY, innerW, blockH);
                            draw_rounded_box(g, codeBoxRect, 8.0f,
                                             Gdiplus::Color(255, 21, 22, 26),  // #15161a
                                             Gdiplus::Color(255, 42, 44, 52),
                                             1.0f);

                            // Header bar inside code block (#25262c)
                            Gdiplus::RectF codeHeadRect(partX, partY, innerW, 28.0f);
                            draw_rounded_box(g, codeHeadRect, 8.0f,
                                             Gdiplus::Color(255, 36, 37, 44),
                                             Gdiplus::Color(0, 0, 0, 0),
                                             0.0f);

                            // Language label on left
                            draw_text_dw(g, s2ws(p.lang), Gdiplus::RectF(partX + 12.0f, partY, 120.0f, 28.0f),
                                         L"Consolas", 8.5f, false, Gdiplus::Color(255, 156, 163, 175), 0, 1, false);

                            // Vector Download Icon & Copy Icon on right
                            float dlX = partX + innerW - 52.0f;
                            float cpX = partX + innerW - 26.0f;
                            float icY = partY + 7.0f;
                            Gdiplus::Color icCol(255, 156, 163, 175);

                            // Download icon (down arrow + tray)
                            d2d_draw_line(dlX + 6.0f, icY + 1.0f, dlX + 6.0f, icY + 9.0f, icCol, 1.3f);
                            d2d_draw_line(dlX + 3.0f, icY + 6.0f, dlX + 6.0f, icY + 9.0f, icCol, 1.3f);
                            d2d_draw_line(dlX + 9.0f, icY + 6.0f, dlX + 6.0f, icY + 9.0f, icCol, 1.3f);
                            d2d_draw_line(dlX + 2.0f, icY + 12.0f, dlX + 10.0f, icY + 12.0f, icCol, 1.3f);

                            // Copy icon (two overlapping squares)
                            draw_rounded_box(g, Gdiplus::RectF(cpX + 3.0f, icY + 3.0f, 8.0f, 9.0f), 1.5f,
                                             Gdiplus::Color(0,0,0,0), icCol, 1.3f);
                            d2d_draw_line(cpX + 1.0f, icY + 1.0f, cpX + 8.0f, icY + 1.0f, icCol, 1.3f);
                            d2d_draw_line(cpX + 1.0f, icY + 1.0f, cpX + 1.0f, icY + 9.0f, icCol, 1.3f);

                            // Show feedback badge if just copied or downloaded
                            if (copied_block_id_ == thisBlockId) {
                                draw_text_dw(g, L"✓ Đã chép", Gdiplus::RectF(dlX - 72.0f, partY, 68.0f, 28.0f),
                                             L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 74, 222, 128), 0, 1, false);
                            } else if (downloaded_block_id_ == thisBlockId) {
                                draw_text_dw(g, L"✓ Đã lưu", Gdiplus::RectF(dlX - 66.0f, partY, 62.0f, 28.0f),
                                             L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 74, 222, 128), 0, 1, false);
                            }

                            // Register interactive hitboxes for Copy & Download
                            code_hitboxes_.push_back({
                                Gdiplus::RectF(cpX - 4.0f, partY + 2.0f, 22.0f, 24.0f),
                                Gdiplus::RectF(dlX - 4.0f, partY + 2.0f, 22.0f, 24.0f),
                                p.lang, p.text, thisBlockId
                            });

                            // Syntax-highlighted code body
                            Gdiplus::RectF codeTextRect(partX + 14.0f, partY + 36.0f, innerW - 28.0f, blockH - 40.0f);
                            draw_code_highlighted(g, p.lang, p.text, codeTextRect, fontCode);
                        }

                        partY += blockH + 12.0f;
                    } else {
                        std::vector<std::string> lines;
                        {
                            std::stringstream lss(p.text);
                            std::string ln;
                            while (std::getline(lss, ln)) {
                                if (!ln.empty() && ln.back() == '\r') ln.pop_back();
                                lines.push_back(ln);
                            }
                        }

                        int lastNonEmptyLine = -1;
                        for (int li = (int)lines.size() - 1; li >= 0; --li) {
                            if (!lines[li].empty()) {
                                lastNonEmptyLine = li;
                                break;
                            }
                        }

                        bool isLastPart = (pIdx + 1 == msg.cached_parts.size());
                        for (size_t li = 0; li < lines.size(); ++li) {
                            const auto& ln = lines[li];
                            if (ln.empty()) {
                                partY += 10.0f;
                                continue;
                            }
                            std::string drawLn = ln;
                            if (isLastPart && (int)li == lastNonEmptyLine && msg.is_streaming && showCursor) {
                                drawLn += " ▋";
                            }
                            float lineH = measure_rich_text_line(drawLn, innerW) + 2.0f;
                            if (partY + lineH >= 40.0f && partY <= rc.bottom - 55.0f) {
                                draw_rich_text_line(g, drawLn, partX, partY, innerW, fontText, fontBold, fontMonoInline);
                            }
                            partY += lineH;
                        }
                        partY += 4.0f;
                    }
                }

                // Executed sandbox commands pills with dynamic height and proper wrapping
                for (const auto& cmd : msg.executed_cmds) {
                    std::wstring wCmd = s2ws("⚡ Sandbox Executed: " + cmd);
                    D2D1_SIZE_F cmdSz = measure_text_dw(wCmd, innerW - 20.0f, L"Consolas", 8.5f, false, true);
                    float badgeH = max(26.0f, cmdSz.height + 12.0f);

                    if (partY + badgeH >= 40.0f && partY <= rc.bottom - 55.0f) {
                        Gdiplus::RectF cmdRect(partX, partY, innerW, badgeH);
                        draw_rounded_box(g, cmdRect, 6.0f,
                                         Gdiplus::Color(255, 6, 44, 34),
                                         Gdiplus::Color(255, 16, 185, 129), 1.0f);
                        draw_text_dw(g, wCmd,
                                     Gdiplus::RectF(partX + 10.0f, partY + 6.0f, innerW - 20.0f, badgeH - 12.0f),
                                     L"Consolas", 8.5f, false, Gdiplus::Color(255, 52, 211, 153), 0, 0, true);
                    }
                    partY += badgeH + 8.0f;
                }
            } else {
                // Even if card is scrolled off-screen, keep blockCounter consistent
                for (const auto& p : msg.cached_parts) {
                    if (p.is_code) blockCounter++;
                }
            }

            curY += (int)cardH + 16;
        }
    }

    chat_content_height_ = curY - startY;
    int visibleChatH = max(100, (int)(rc.bottom - 46 - 78));
    if (auto_scroll_chat_) {
        target_chat_scroll_y_ = max(0, chat_content_height_ - visibleChatH);
    }

    clear_render_clip();

    // Floating Muse Avatar at top-center when Right Panel is closed (Matches Screenshot 5 1:1!)
    if (right_panel_cur_w_ < 30.0f) {
        float flX = (float)chat_left + ((float)chat_w - 48.0f) / 2.0f;
        float flY = 8.0f;
        if (is_chat_generating_ || arena_running_) {
            float pulse = (std::sin(pulse_phase_) + 1.0f) * 0.5f;
            float rPad = 2.0f + pulse * 4.0f;
            d2d_draw_ellipse(flX - rPad, flY - rPad, 48.0f + rPad * 2.0f, 48.0f + rPad * 2.0f,
                             Gdiplus::Color((int)(180 - pulse * 120), 168, 85, 247), 2.0f);
        }
        draw_circle_avatar(g, muse_avatar_img_, flX, flY, 48.0f, L"M");

        Gdiplus::RectF badgeRect(flX - 2.0f, flY + 42.0f, 52.0f, 20.0f);
        draw_rounded_box(g, badgeRect, 10.0f,
                         hover_floating_muse_ ? Gdiplus::Color(255, 52, 54, 64) : Gdiplus::Color(255, 36, 38, 45),
                         Gdiplus::Color(255, 58, 60, 70), 1.0f);
        draw_text_dw(g, L"Muse", badgeRect, L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 240, 242, 248), 1, 1, false);
    }

    // Bottom Input Dock Area & Capsule (Matches Screenshot 3 1:1!)
    float capsuleW = (float)min(chat_w - 80, 620);
    float capsuleX = (float)chat_left + ((float)chat_w - capsuleW) / 2.0f;
    float capsuleY = (float)(rc.bottom - 62);
    Gdiplus::RectF capsuleRect(capsuleX, capsuleY, capsuleW, 44.0f);
    draw_rounded_box(g, capsuleRect, 22.0f,
                     Gdiplus::Color(255, 37, 38, 43),  // #25262b
                     Gdiplus::Color(255, 52, 53, 60),  // #34353c
                     1.0f);

    // Clean vector '+' inside left of capsule (NO circle background, matching Screenshot 3!)
    float plusCx = capsuleX + 22.0f;
    float plusCy = capsuleY + 22.0f;
    Gdiplus::Color plusCol = hover_btn_plus_ ? Gdiplus::Color(255, 240, 242, 248) : Gdiplus::Color(255, 160, 164, 176);
    d2d_draw_line(plusCx - 5.5f, plusCy, plusCx + 5.5f, plusCy, plusCol, 1.8f);
    d2d_draw_line(plusCx, plusCy - 5.5f, plusCx, plusCy + 5.5f, plusCol, 1.8f);

    // Right side of capsule: Animated Stop '■' button while streaming, or Send '↑' button when idle
    if (is_chat_generating_) {
        float pulse = (std::sin(pulse_phase_) + 1.0f) * 0.5f;
        Gdiplus::RectF stopCircle(capsuleX + capsuleW - 36.0f, capsuleY + 8.0f, 28.0f, 28.0f);
        draw_rounded_box(g, stopCircle, 14.0f,
                         Gdiplus::Color(255, (int)(185 + pulse * 45), 38, 56),
                         Gdiplus::Color(255, 248, 113, 113), 1.2f);
        d2d_fill_rect(stopCircle.X + 9.5f, stopCircle.Y + 9.5f, 9.0f, 9.0f, Gdiplus::Color(255, 255, 255, 255));
    } else {
        wchar_t inBuf[64] = { 0 };
        GetWindowTextW(h_chat_input_, inBuf, 64);
        if (wcslen(inBuf) > 0 || hover_btn_send_) {
            Gdiplus::RectF sendCircle(capsuleX + capsuleW - 36.0f, capsuleY + 8.0f, 28.0f, 28.0f);
            draw_rounded_box(g, sendCircle, 14.0f, Gdiplus::Color(255, 124, 58, 237), Gdiplus::Color(0,0,0,0), 0.0f);
            Gdiplus::Color arrCol(255, 255, 255, 255);
            float scx = sendCircle.X + 14.0f, scy = sendCircle.Y + 14.0f;
            d2d_draw_line(scx, scy + 5.0f, scx, scy - 5.0f, arrCol, 1.8f);
            d2d_draw_line(scx - 4.0f, scy - 1.0f, scx, scy - 5.0f, arrCol, 1.8f);
            d2d_draw_line(scx + 4.0f, scy - 1.0f, scx, scy - 5.0f, arrCol, 1.8f);
        }
    }
}

void NativeGui::render_arena_view(Gdiplus::Graphics& g, const RECT& rc) {
    int chat_left = 48;
    int right_panel_w = (int)right_panel_cur_w_;
    int chat_right = rc.right - right_panel_w;
    int chat_w = chat_right - chat_left;

    float cardX = (float)chat_left + 20.0f;
    float cardW = (float)(chat_w - 40);
    float cardH = 74.0f;
    Gdiplus::RectF topicCard(cardX, 52.0f, cardW, cardH);
    draw_rounded_box(g, topicCard, 12.0f, Gdiplus::Color(255, 28, 29, 34), Gdiplus::Color(255, 42, 45, 58), 1.0f);

    // Left side: Title and Topic
    draw_text_dw(g, L"⚡ BẦY AGENT TỰ CHỦ VÔ HẠN (AUTONOMOUS SWARM ARENA)",
                 Gdiplus::RectF(cardX + 16.0f, 57.0f, max(200.0f, cardW - 390.0f), 22.0f),
                 L"Segoe UI", 10.5f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);

    std::wstring dispTopic = s2ws("🎯 Chủ đề: " + arena_topic_);
    draw_text_dw(g, dispTopic,
                 Gdiplus::RectF(cardX + 16.0f, 80.0f, max(200.0f, cardW - 390.0f), 40.0f),
                 L"Segoe UI", 9.0f, false, Gdiplus::Color(255, 148, 163, 184), 0, 0, true);

    // Right side Controls Group: Context Window, Speed toggle, Re-role, Swarm Counter, and Mini Avatars
    float ctrlRight = cardX + cardW - 12.0f;

    // 1. Context Window button (Tùy chỉnh độ dài ngữ cảnh)
    float ctxW = 76.0f;
    float ctxX = ctrlRight - ctxW;
    arena_context_btn_rect_ = Gdiplus::RectF(ctxX, 57.0f, ctxW, 28.0f);
    std::wstring ctxText = (arena_context_window_ == 0) ? L"📜 Ctx: ∞" : (s2ws("📜 Ctx: " + std::to_string(arena_context_window_)));
    draw_rounded_box(g, arena_context_btn_rect_, 6.0f,
                     hover_btn_context_ ? Gdiplus::Color(255, 30, 58, 90) : Gdiplus::Color(255, 20, 38, 62),
                     Gdiplus::Color(255, 59, 130, 246), 1.0f);
    draw_text_dw(g, ctxText, arena_context_btn_rect_, L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 191, 219, 254), 1, 1, false);

    // 2. Speed button
    float spdW = 64.0f;
    float spdX = ctxX - spdW - 6.0f;
    arena_speed_btn_rect_ = Gdiplus::RectF(spdX, 57.0f, spdW, 28.0f);
    int curMs = arena_delay_ms_.load();
    std::wstring spdText = (curMs <= 1000) ? L"🚀 0.8s" : ((curMs >= 3000) ? L"🐢 3.0s" : L"⚡ 1.8s");
    draw_rounded_box(g, arena_speed_btn_rect_, 6.0f,
                     hover_btn_speed_ ? Gdiplus::Color(255, 48, 50, 62) : Gdiplus::Color(255, 34, 36, 45),
                     Gdiplus::Color(255, 58, 62, 78), 1.0f);
    draw_text_dw(g, spdText, arena_speed_btn_rect_, L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 226, 232, 240), 1, 1, false);

    // 3. Re-role button (Làm mới bầy)
    float rolW = 98.0f;
    float rolX = spdX - rolW - 6.0f;
    arena_reset_roles_rect_ = Gdiplus::RectF(rolX, 57.0f, rolW, 28.0f);
    draw_rounded_box(g, arena_reset_roles_rect_, 6.0f,
                     hover_btn_reset_roles_ ? Gdiplus::Color(255, 62, 38, 92) : Gdiplus::Color(255, 42, 28, 65),
                     Gdiplus::Color(255, 147, 51, 234), 1.0f);
    draw_text_dw(g, L"🔄 Làm mới", arena_reset_roles_rect_, L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 216, 180, 254), 1, 1, false);

    // 4. Swarm Plus [+] button
    float plusW = 24.0f;
    float plusX = rolX - plusW - 6.0f;
    arena_swarm_plus_rect_ = Gdiplus::RectF(plusX, 57.0f, plusW, 28.0f);
    draw_rounded_box(g, arena_swarm_plus_rect_, 6.0f,
                     hover_btn_swarm_plus_ ? Gdiplus::Color(255, 52, 54, 68) : Gdiplus::Color(255, 36, 38, 48),
                     Gdiplus::Color(255, 60, 64, 80), 1.0f);
    draw_text_dw(g, L"＋", arena_swarm_plus_rect_, L"Segoe UI", 11.0f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);

    // 5. Swarm Counter badge
    float cntW = 72.0f;
    float cntX = plusX - cntW - 4.0f;
    Gdiplus::RectF cntRect(cntX, 57.0f, cntW, 28.0f);
    draw_rounded_box(g, cntRect, 6.0f, Gdiplus::Color(255, 20, 21, 27), Gdiplus::Color(255, 45, 48, 62), 1.0f);
    draw_text_dw(g, s2ws("🤖 " + std::to_string(swarm_size_) + " Con"), cntRect, L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 241, 245, 249), 1, 1, false);

    // 6. Swarm Minus [-] button
    float minW = 24.0f;
    float minX = cntX - minW - 4.0f;
    arena_swarm_minus_rect_ = Gdiplus::RectF(minX, 57.0f, minW, 28.0f);
    draw_rounded_box(g, arena_swarm_minus_rect_, 6.0f,
                     hover_btn_swarm_minus_ ? Gdiplus::Color(255, 52, 54, 68) : Gdiplus::Color(255, 36, 38, 48),
                     Gdiplus::Color(255, 60, 64, 80), 1.0f);
    draw_text_dw(g, L"－", arena_swarm_minus_rect_, L"Segoe UI", 11.0f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);

    // Row 2 of right group: Mini Avatars of active swarm members
    float avRowY = 93.0f;
    float curAvX = minX;
    for (size_t a = 0; a < arena_agents_.size() && a < 12; ++a) {
        const auto& ag = arena_agents_[a];
        bool isAct = (arena_running_ && arena_active_speaker_ == ag.name);
        Gdiplus::Color agCol(255, GetRValue(ag.color), GetGValue(ag.color), GetBValue(ag.color));
        if (isAct) {
            d2d_draw_ellipse(curAvX - 2.0f, avRowY - 2.0f, 22.0f, 22.0f, agCol, 1.6f);
        }
        d2d_fill_ellipse(curAvX, avRowY, 18.0f, 18.0f, agCol);
        std::wstring initW = s2ws(utf8_safe_substr(ag.name, 4));
        if (!initW.empty()) initW = initW.substr(0, 1);
        draw_text_dw(g, initW, Gdiplus::RectF(curAvX, avRowY, 18.0f, 18.0f), L"Segoe UI", 8.0f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);
        curAvX += 22.0f;
    }

    if (arena_running_ && !arena_active_speaker_.empty()) {
        draw_text_dw(g, s2ws("⚡ Đang phát biểu: " + arena_active_speaker_),
                     Gdiplus::RectF(curAvX + 8.0f, avRowY - 1.0f, 160.0f, 20.0f),
                     L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 74, 222, 128), 0, 1, false);
    }

    set_render_clip((float)chat_left, 134.0f, (float)chat_w, (float)(rc.bottom - 134 - 72));

    int startY = 142 - arena_scroll_y_;
    int curY = startY;
    bool showCursor = ((anim_frame_ % 18) < 12);

    std::lock_guard<std::mutex> lock(arena_mutex_);

    for (const auto& msg : arena_messages_) {
        float cardX = (float)chat_left + 64.0f;
        float bubbleW = max(320.0f, (float)(chat_w - 84));
        std::string dispText = msg.text;
        if (msg.is_streaming) {
            if (dispText.empty()) dispText = "● ● ● Đang suy nghĩ...";
            else dispText += (showCursor ? " ▋" : "  ");
        }
        std::wstring wText = s2ws(dispText);

        if (std::abs(msg.cached_col_w - bubbleW) > 0.5f ||
            msg.cached_text_len != msg.text.size() ||
            msg.cached_cmds_count != msg.executed_cmds.size() ||
            msg.cached_was_streaming != msg.is_streaming ||
            msg.cached_card_h <= 0.0f) {
            D2D1_SIZE_F textSz = measure_text_dw(wText, bubbleW - 32.0f, L"Segoe UI", 10.0f, false, true);
            float h = textSz.height + 44.0f;
            for (const auto& cmd : msg.executed_cmds) {
                std::wstring wCmd = s2ws("⚡ Sandbox: " + cmd);
                D2D1_SIZE_F cmdSz = measure_text_dw(wCmd, bubbleW - 52.0f, L"Consolas", 8.5f, false, true);
                h += max(26.0f, cmdSz.height + 12.0f) + 6.0f;
            }
            msg.cached_card_h = max(68.0f, h);
            msg.cached_col_w = bubbleW;
            msg.cached_text_len = msg.text.size();
            msg.cached_cmds_count = msg.executed_cmds.size();
            msg.cached_was_streaming = msg.is_streaming;
        }
        float cardH = msg.cached_card_h;

        if (curY + cardH >= 130 && curY <= rc.bottom - 60) {
            Gdiplus::RectF avRect((float)chat_left + 18.0f, (float)curY + 4.0f, 36.0f, 36.0f);
            Gdiplus::Color avCol(255, GetRValue(msg.color), GetGValue(msg.color), GetBValue(msg.color));
            if (msg.is_streaming) {
                float pulse = (std::sin(pulse_phase_) + 1.0f) * 0.5f;
                float rPad = 2.0f + pulse * 3.5f;
                d2d_draw_ellipse(avRect.X - rPad, avRect.Y - rPad, avRect.Width + rPad * 2.0f, avRect.Height + rPad * 2.0f,
                                 Gdiplus::Color((int)(190 - pulse * 120), GetRValue(msg.color), GetGValue(msg.color), GetBValue(msg.color)), 2.0f);
            }
            d2d_fill_ellipse(avRect.X, avRect.Y, avRect.Width, avRect.Height, avCol);

            std::wstring initial = s2ws(utf8_safe_substr(msg.sender_name, 4));
            if (!initial.empty()) initial = initial.substr(0, 1);
            draw_text_dw(g, initial, avRect, L"Segoe UI", 10.0f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);

            Gdiplus::RectF cardRect(cardX, (float)curY, bubbleW, cardH);
            draw_rounded_box(g, cardRect, 12.0f, Gdiplus::Color(255, 28, 29, 34),
                             msg.is_streaming ? avCol : Gdiplus::Color(255, 42, 45, 58), 1.0f);

            Gdiplus::RectF barRect(cardX + 2.0f, (float)curY + 4.0f, 4.0f, cardH - 8.0f);
            draw_rounded_box(g, barRect, 2.0f, avCol, Gdiplus::Color(0,0,0,0), 0.0f);

            float headY = (float)curY + 8.0f;
            std::wstring wName = s2ws(msg.sender_name);
            draw_text_dw(g, wName, Gdiplus::RectF(cardX + 16.0f, headY, 200.0f, 22.0f),
                         L"Segoe UI", 10.0f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);

            if (!msg.role.empty()) {
                draw_text_dw(g, s2ws("[" + msg.role + "]"),
                             Gdiplus::RectF(cardX + 16.0f + (wName.length() * 8.5f) + 14.0f, headY + 1.5f, 260.0f, 20.0f),
                             L"Segoe UI", 8.5f, false, avCol, 0, 0, false);
            }

            draw_text_dw(g, s2ws(msg.timestamp),
                         Gdiplus::RectF(cardX + bubbleW - 90.0f, headY + 1.5f, 74.0f, 20.0f),
                         L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 100, 116, 139), 2, 0, false);

            float bodyY = headY + 24.0f;
            D2D1_SIZE_F textSz = measure_text_dw(wText, bubbleW - 32.0f, L"Segoe UI", 10.0f, false, true);
            draw_text_dw(g, wText,
                         Gdiplus::RectF(cardX + 16.0f, bodyY, bubbleW - 32.0f, textSz.height + 4.0f),
                         L"Segoe UI", 10.0f, false, Gdiplus::Color(255, 226, 232, 240), 0, 0, true);

            float pillY = bodyY + textSz.height + 8.0f;
            for (const auto& cmd : msg.executed_cmds) {
                std::wstring wCmd = s2ws("⚡ Sandbox Executed: " + cmd);
                D2D1_SIZE_F cmdSz = measure_text_dw(wCmd, bubbleW - 52.0f, L"Consolas", 8.5f, false, true);
                float bH = max(26.0f, cmdSz.height + 12.0f);
                Gdiplus::RectF pillRect(cardX + 16.0f, pillY, bubbleW - 32.0f, bH);
                draw_rounded_box(g, pillRect, 6.0f, Gdiplus::Color(255, 6, 44, 34), Gdiplus::Color(255, 16, 185, 129), 1.0f);
                draw_text_dw(g, wCmd, Gdiplus::RectF(cardX + 26.0f, pillY + 5.0f, bubbleW - 52.0f, bH - 10.0f),
                             L"Consolas", 8.5f, false, Gdiplus::Color(255, 52, 211, 153), 0, 0, true);
                pillY += bH + 6.0f;
            }
        }

        curY += (int)cardH + 14;
    }

    arena_content_height_ = curY - startY;
    int visibleArenaH = max(100, (int)(rc.bottom - 132 - 76));
    if (auto_scroll_arena_) {
        target_arena_scroll_y_ = max(0, arena_content_height_ - visibleArenaH);
    }

    clear_render_clip();

    // Sleek bottom container card for Arena Interject input
    float barX = (float)chat_left + 20.0f;
    float barW = (float)(chat_w - 40);
    float barH = 46.0f;
    float barY = (float)rc.bottom - barH - 10.0f;
    Gdiplus::RectF barRect(barX, barY, barW, barH);
    draw_rounded_box(g, barRect, 10.0f, Gdiplus::Color(255, 28, 29, 34), Gdiplus::Color(255, 42, 45, 58), 1.0f);

    // Label on left
    draw_text_dw(g, L"💬 Can thiệp:", Gdiplus::RectF(barX + 14.0f, barY + 12.0f, 92.0f, 22.0f),
                 L"Segoe UI", 9.5f, true, Gdiplus::Color(255, 148, 163, 184), 0, 1, false);

    // Direct2D "Can thiệp" Button on right (moves seamlessly with animation, zero HWND glitches)
    float btnW = 110.0f;
    float btnH = 30.0f;
    float btnX = barX + barW - btnW - 8.0f;
    float btnY = barY + 8.0f;
    arena_interject_btn_rect_ = Gdiplus::RectF(btnX, btnY, btnW, btnH);

    Gdiplus::Color btnBg = hover_btn_arena_interject_ ? Gdiplus::Color(255, 109, 40, 217) : Gdiplus::Color(255, 88, 28, 135);
    Gdiplus::Color btnBorder = hover_btn_arena_interject_ ? Gdiplus::Color(255, 192, 132, 252) : Gdiplus::Color(255, 147, 51, 234);
    draw_rounded_box(g, arena_interject_btn_rect_, 8.0f, btnBg, btnBorder, 1.0f);
    draw_text_dw(g, L"Can thiệp", arena_interject_btn_rect_, L"Segoe UI", 9.5f, true, Gdiplus::Color(255, 241, 245, 249), 1, 1, false);
}

void NativeGui::render_topic_modal(Gdiplus::Graphics& g, const RECT& rc) {
    if (!show_topic_modal_) return;

    float rW = (float)rc.right;
    float rH = (float)rc.bottom;

    // 1. Full-screen dark translucent backdrop
    d2d_fill_rect(0.0f, 0.0f, rW, rH, Gdiplus::Color(215, 10, 11, 15));

    // 2. Centered Modal Card
    float cardW = min(660.0f, rW - 40.0f);
    float cardH = 430.0f;
    float cardX = (rW - cardW) * 0.5f;
    float cardY = max(36.0f, (rH - cardH) * 0.5f);
    modal_card_rect_ = Gdiplus::RectF(cardX, cardY, cardW, cardH);

    draw_rounded_box(g, modal_card_rect_, 16.0f, Gdiplus::Color(255, 24, 25, 31), Gdiplus::Color(255, 88, 52, 138), 1.5f);

    // Glowing top bar accent
    d2d_draw_line(cardX + 24.0f, cardY + 1.0f, cardX + cardW - 24.0f, cardY + 1.0f, Gdiplus::Color(255, 168, 85, 247), 2.0f);

    // Header
    draw_text_dw(g, L"✏️ THIẾT LẬP CHỦ ĐỀ & QUY MÔ BẦY AGENT (SWARM)",
                 Gdiplus::RectF(cardX + 24.0f, cardY + 18.0f, cardW - 48.0f, 26.0f),
                 L"Segoe UI", 12.0f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);

    draw_text_dw(g, L"Nhập chủ đề tùy ý. Bầy agent sẽ tự phân tích và tự động phân bổ vai trò chuyên môn độc lập.",
                 Gdiplus::RectF(cardX + 24.0f, cardY + 46.0f, cardW - 48.0f, 20.0f),
                 L"Segoe UI", 9.0f, false, Gdiplus::Color(255, 148, 163, 184), 0, 0, false);

    // Topic Label
    draw_text_dw(g, L"CHỦ ĐỀ TRANH LUẬN MỚI:",
                 Gdiplus::RectF(cardX + 24.0f, cardY + 74.0f, 250.0f, 18.0f),
                 L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 192, 132, 252), 0, 0, false);

    // Background for topic input edit control
    Gdiplus::RectF inBoxRect(cardX + 22.0f, cardY + 96.0f, cardW - 44.0f, 68.0f);
    draw_rounded_box(g, inBoxRect, 8.0f, Gdiplus::Color(255, 18, 19, 24), Gdiplus::Color(255, 52, 55, 72), 1.0f);

    // Quick presets section
    draw_text_dw(g, L"💡 GỢI Ý CHỦ ĐỀ HOT (Nhấp để nạp nhanh):",
                 Gdiplus::RectF(cardX + 24.0f, cardY + 176.0f, 350.0f, 18.0f),
                 L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 148, 163, 184), 0, 0, false);

    const wchar_t* presetLabels[4] = {
        L"🛡️ Vượt Sandbox & Air-Gap",
        L"📚 Phương pháp học siêu tốc",
        L"⚡ C++ Direct2D vs Web/Electron",
        L"🤖 Bầy đàn AI Tự Trị & Phân Vai"
    };

    float pW = (cardW - 44.0f - 12.0f) * 0.5f;
    for (int i = 0; i < 4; ++i) {
        float px = cardX + 22.0f + (i % 2) * (pW + 12.0f);
        float py = cardY + 200.0f + (i / 2) * 36.0f;
        modal_preset_rects_[i] = Gdiplus::RectF(px, py, pW, 30.0f);

        bool isHov = (hover_modal_preset_ == i);
        Gdiplus::Color bg = isHov ? Gdiplus::Color(255, 45, 48, 62) : Gdiplus::Color(255, 30, 32, 40);
        Gdiplus::Color brd = isHov ? Gdiplus::Color(255, 168, 85, 247) : Gdiplus::Color(255, 50, 54, 68);
        draw_rounded_box(g, modal_preset_rects_[i], 6.0f, bg, brd, 1.0f);
        draw_text_dw(g, presetLabels[i], modal_preset_rects_[i], L"Segoe UI", 8.5f, false, Gdiplus::Color(255, 226, 232, 240), 1, 1, false);
    }

    // Swarm size row
    float swarmY = cardY + 284.0f;
    draw_text_dw(g, L"QUY MÔ BẦY ĐÀN (SPAM AGENTS):",
                 Gdiplus::RectF(cardX + 24.0f, swarmY + 6.0f, 220.0f, 24.0f),
                 L"Segoe UI", 8.5f, true, Gdiplus::Color(255, 148, 163, 184), 0, 1, false);

    float cntX = cardX + 240.0f;
    // [-] button
    modal_swarm_minus_rect_ = Gdiplus::RectF(cntX, swarmY, 32.0f, 32.0f);
    draw_rounded_box(g, modal_swarm_minus_rect_, 6.0f,
                     hover_btn_swarm_minus_ ? Gdiplus::Color(255, 52, 54, 66) : Gdiplus::Color(255, 34, 36, 45),
                     Gdiplus::Color(255, 60, 64, 80), 1.0f);
    draw_text_dw(g, L"－", modal_swarm_minus_rect_, L"Segoe UI", 12.0f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);

    // Count badge
    Gdiplus::RectF cntBadge(cntX + 38.0f, swarmY, 96.0f, 32.0f);
    draw_rounded_box(g, cntBadge, 6.0f, Gdiplus::Color(255, 18, 19, 24), Gdiplus::Color(255, 52, 55, 72), 1.0f);
    draw_text_dw(g, s2ws("🤖 " + std::to_string(swarm_size_) + " Agent"), cntBadge, L"Segoe UI", 9.5f, true, Gdiplus::Color(255, 241, 245, 249), 1, 1, false);

    // [+] button
    modal_swarm_plus_rect_ = Gdiplus::RectF(cntX + 140.0f, swarmY, 32.0f, 32.0f);
    draw_rounded_box(g, modal_swarm_plus_rect_, 6.0f,
                     hover_btn_swarm_plus_ ? Gdiplus::Color(255, 52, 54, 66) : Gdiplus::Color(255, 34, 36, 45),
                     Gdiplus::Color(255, 60, 64, 80), 1.0f);
    draw_text_dw(g, L"＋", modal_swarm_plus_rect_, L"Segoe UI", 12.0f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);

    // Reset roles button
    modal_reset_roles_rect_ = Gdiplus::RectF(cntX + 184.0f, swarmY, 130.0f, 32.0f);
    draw_rounded_box(g, modal_reset_roles_rect_, 6.0f,
                     hover_btn_reset_roles_ ? Gdiplus::Color(255, 60, 40, 90) : Gdiplus::Color(255, 38, 28, 60),
                     Gdiplus::Color(255, 147, 51, 234), 1.0f);
    draw_text_dw(g, L"🔄 Làm mới bầy", modal_reset_roles_rect_, L"Segoe UI", 9.0f, true, Gdiplus::Color(255, 216, 180, 254), 1, 1, false);

    // Bottom modal buttons
    float btnY = cardY + cardH - 52.0f;
    modal_btn_cancel_rect_ = Gdiplus::RectF(cardX + cardW - 270.0f, btnY, 110.0f, 36.0f);
    draw_rounded_box(g, modal_btn_cancel_rect_, 8.0f,
                     hover_modal_cancel_ ? Gdiplus::Color(255, 48, 50, 62) : Gdiplus::Color(255, 32, 34, 42),
                     Gdiplus::Color(255, 55, 58, 72), 1.0f);
    draw_text_dw(g, L"Hủy bỏ", modal_btn_cancel_rect_, L"Segoe UI", 9.5f, false, Gdiplus::Color(255, 203, 213, 225), 1, 1, false);

    modal_btn_apply_rect_ = Gdiplus::RectF(cardX + cardW - 148.0f, btnY, 126.0f, 36.0f);
    draw_rounded_box(g, modal_btn_apply_rect_, 8.0f,
                     hover_modal_apply_ ? Gdiplus::Color(255, 124, 58, 237) : Gdiplus::Color(255, 109, 40, 217),
                     Gdiplus::Color(255, 168, 85, 247), 1.0f);
    draw_text_dw(g, L"✓ Áp dụng ngay", modal_btn_apply_rect_, L"Segoe UI", 9.5f, true, Gdiplus::Color(255, 255, 255, 255), 1, 1, false);
}

void NativeGui::render_settings_view(Gdiplus::Graphics& g, const RECT& rc) {
    settings_view_.render(dx11_renderer_, rc, right_panel_cur_w_);
}

void NativeGui::render_workspace_view(Gdiplus::Graphics& g, const RECT& rc) {
    int chat_left = 48;
    draw_text_dw(g, L"DANH SÁCH TỆP TRONG SANDBOX", Gdiplus::RectF((float)chat_left + 20.0f, 64.0f, 300.0f, 24.0f),
                 L"Segoe UI", 11.0f, true, Gdiplus::Color(255, 241, 245, 249), 0, 0, false);
    std::wstring previewTitle = L"TRÌNH SOẠN THẢO CANVAS (CHATGPT CANVAS MODE)";
    if (!current_ws_file_.empty()) {
        previewTitle += L" — " + s2ws(current_ws_file_);
    } else {
        previewTitle += L" — (Chưa chọn tệp)";
    }
    draw_text_dw(g, previewTitle, Gdiplus::RectF((float)chat_left + 328.0f, 64.0f, 520.0f, 24.0f),
                 L"Segoe UI", 11.0f, true, Gdiplus::Color(255, 129, 140, 248), 0, 0, false);
}

void NativeGui::render_window(HDC hdc, const RECT& rc) {
    d2d_target_ = get_d2d_target();
    if (!d2d_target_) return;
    current_hdc_ = hdc;
    current_rc_ = rc;
    has_active_clip_ = false;

    ensure_d2d_resources();

    // Single-Pass GPU Direct2D Frame Batch: BindDC only for GDI target
    if (!use_dx11_ && d2d_dc_target_) {
        d2d_dc_target_->BindDC(hdc, &rc);
    }
    d2d_target_->BeginDraw();
    d2d_batch_active_ = true;

    // Main deep dark background (#121316 matching Screenshots 1-5)
    d2d_target_->Clear(D2D1::ColorF(18.0f / 255.0f, 19.0f / 255.0f, 22.0f / 255.0f, 1.0f));

    Gdiplus::Graphics* dummyG = NULL;

    // Render active view (pinned & identity transform for zero HWND displacement)
    if (current_tab_ == ActiveTab::CHAT) {
        render_chat_view(*dummyG, rc);
    } else if (current_tab_ == ActiveTab::ARENA) {
        render_arena_view(*dummyG, rc);
    } else if (current_tab_ == ActiveTab::SETTINGS) {
        render_settings_view(*dummyG, rc);
    } else if (current_tab_ == ActiveTab::WORKSPACE) {
        render_workspace_view(*dummyG, rc);
    }

    // Render top bar, left dock, and right panel
    render_top_bar(*dummyG, rc);
    render_left_dock(*dummyG, rc);
    render_right_panel(*dummyG, rc);

    // Render modern frameless window controls (— □ ✕) on far top-right
    render_window_controls(*dummyG, rc);

    // Render Custom Topic & Swarm modal on top if active
    if (show_topic_modal_) {
        render_topic_modal(*dummyG, rc);
    }

    // Render Saved Sessions History Modal on top if active
    if (session_modal_.is_visible()) {
        session_modal_.render(dx11_renderer_, rc, cached_sessions_);
    }

    // Neon infinity frame border (Matches Screenshots 3, 4 & 5!)
    if (!IsZoomed(hwnd_main_)) {
        Gdiplus::Color purpleCol(255, 109, 40, 217); // #6d28d9
        d2d_draw_line(0.0f, 0.0f, (float)rc.right, 0.0f, purpleCol, 1.5f);
        d2d_draw_line(0.0f, (float)rc.bottom - 1.0f, (float)rc.right, (float)rc.bottom - 1.0f, purpleCol, 1.5f);
        d2d_draw_line(0.0f, 0.0f, 0.0f, (float)rc.bottom, purpleCol, 1.5f);
        d2d_draw_line((float)rc.right - 1.0f, 0.0f, (float)rc.right - 1.0f, (float)rc.bottom, purpleCol, 1.5f);
    }

    if (has_active_clip_) {
        d2d_target_->PopAxisAlignedClip();
        has_active_clip_ = false;
    }

    d2d_batch_active_ = false;
    HRESULT hr = d2d_target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        if (d2d_solid_brush_) { d2d_solid_brush_->Release(); d2d_solid_brush_ = NULL; }
        if (d2d_avatar_bmp_) { d2d_avatar_bmp_->Release(); d2d_avatar_bmp_ = NULL; }
    }

    current_hdc_ = NULL;
}

void NativeGui::on_send_chat() {
    if (is_chat_generating_) return;

    wchar_t buf[2048] = { 0 };
    GetWindowTextW(h_chat_input_, buf, 2048);
    std::string text = ws2s(buf);
    if (text.empty()) return;

    SetWindowTextW(h_chat_input_, L"");

    GuiMessage umsg;
    umsg.sender = "user";
    umsg.sender_name = "Tri";
    umsg.role = "";
    umsg.timestamp = get_current_time_str();
    umsg.text = text;
    umsg.color = RGB(255, 255, 255);

    // Create initial streaming card for Muse immediately so animated thinking dots appear right away
    GuiMessage botPlaceholder;
    botPlaceholder.sender = "muse";
    botPlaceholder.sender_name = "Muse";
    botPlaceholder.role = "";
    botPlaceholder.timestamp = get_current_time_str();
    botPlaceholder.color = RGB(139, 92, 246);
    botPlaceholder.is_streaming = true;

    size_t bot_idx = 0;
    {
        std::lock_guard<std::mutex> lock(chat_mutex_);
        chat_messages_.push_back(umsg);
        chat_messages_.push_back(botPlaceholder);
        bot_idx = chat_messages_.size() - 1;
    }

    auto_scroll_chat_ = true;
    timeline_logs_.push_back("[" + get_current_time_str() + "] User: " + utf8_safe_substr(text, 36));
    InvalidateRect(hwnd_main_, NULL, FALSE);

    log_console("[USER]", text, 15);

    is_chat_generating_ = true;
    chat_interrupt_flag_ = false;

    std::thread([this, text, bot_idx]() {
        std::vector<ai::ChatMessage> history;
        history.push_back({ "system", build_system_prompt() });

        {
            std::lock_guard<std::mutex> lock(chat_mutex_);
            for (size_t i = 0; i < bot_idx && i < chat_messages_.size(); ++i) {
                const auto& m = chat_messages_[i];
                if (m.sender == "user") history.push_back({ "user", m.text });
                else if (m.sender == "muse") history.push_back({ "assistant", m.text });
            }
        }

        std::vector<std::string> executed_cmds;
        int current_turn = 0;
        ai::AiResponse last_resp;
        std::string accumulated_turns_text;
        std::string prev_turn_cmd_sig;
        int identical_cmd_streak = 0;
        const size_t base_history_len = history.size();

        // Infinite autonomous agent loop: runs until task completes (no new bash commands) or user clicks Stop (■)
        while (!chat_interrupt_flag_) {
            current_turn++;
            log_console("[AI]", "Muse đang stream tự chủ lượt #" + std::to_string(current_turn) + "...", 14);

            // Auto-compact intermediate turns when context window grows large during infinite execution
            if (history.size() > base_history_len + 14) {
                std::string summary = "[Tóm tắt tự động các bước Sandbox đã chạy trước đó (" + std::to_string(executed_cmds.size()) + " lệnh)]:\n";
                size_t start_cmd = executed_cmds.size() > 12 ? executed_cmds.size() - 12 : 0;
                for (size_t ci = start_cmd; ci < executed_cmds.size(); ++ci) {
                    summary += "- Lệnh #" + std::to_string(ci + 1) + ": `" + executed_cmds[ci] + "`\n";
                }
                std::vector<ai::ChatMessage> compacted;
                for (size_t hi = 0; hi < base_history_len; ++hi) {
                    compacted.push_back(history[hi]);
                }
                compacted.push_back({ "user", summary });
                // Keep the most recent 6 messages (3 assistant + 3 sandbox output pairs) in full detail
                for (size_t hi = history.size() - 6; hi < history.size(); ++hi) {
                    compacted.push_back(history[hi]);
                }
                history = std::move(compacted);
                log_console("[CONTEXT]", "Đã tự động nén ngữ cảnh trung gian để duy trì vòng lặp tự chủ vô hạn.", 11);
            }

            std::string turn_prefix = accumulated_turns_text.empty() ? "" : (accumulated_turns_text + "\n\n");
            std::string turn_content;

            last_resp = ai_client_->chat_completion_stream(history, [&](const ai::StreamChunk& chunk) -> bool {
                if (chat_interrupt_flag_) return false;

                if (!chunk.delta_content.empty() || !chunk.delta_reasoning.empty()) {
                    turn_content += chunk.delta_content;
                    {
                        std::lock_guard<std::mutex> lock(chat_mutex_);
                        if (bot_idx < chat_messages_.size()) {
                            chat_messages_[bot_idx].text = turn_prefix + turn_content;
                            chat_messages_[bot_idx].reasoning += chunk.delta_reasoning;
                        }
                    }
                    if (!chunk_msg_queued_.exchange(true)) {
                        PostMessageW(hwnd_main_, WM_APP_CHAT_CHUNK, 0, 0);
                    }
                }
                return !chat_interrupt_flag_;
            });

            if (!last_resp.success || chat_interrupt_flag_) break;

            if (turn_content.empty() && !last_resp.content.empty()) {
                turn_content = last_resp.content;
                {
                    std::lock_guard<std::mutex> lock(chat_mutex_);
                    if (bot_idx < chat_messages_.size()) {
                        chat_messages_[bot_idx].text = turn_prefix + turn_content;
                    }
                }
                if (!chunk_msg_queued_.exchange(true)) {
                    PostMessageW(hwnd_main_, WM_APP_CHAT_CHUNK, 0, 0);
                }
            }

            accumulated_turns_text = turn_prefix + turn_content;
            history.push_back({ "assistant", turn_content });

            std::vector<std::string> cmds = ai::AiClient::extract_commands(turn_content);
            if (cmds.empty()) break;

            // Detect if the model is stuck repeating the exact same command set 3 times in a row
            std::string cmd_sig;
            for (const auto& c : cmds) cmd_sig += c + "||";
            if (cmd_sig == prev_turn_cmd_sig) {
                identical_cmd_streak++;
            } else {
                prev_turn_cmd_sig = cmd_sig;
                identical_cmd_streak = 1;
            }

            auto* engine = sandbox_mgr_->get_engine();
            std::string combined_out = "";
            for (const auto& cmd : cmds) {
                if (chat_interrupt_flag_) break;
                executed_cmds.push_back(cmd);
                {
                    std::lock_guard<std::mutex> lock(chat_mutex_);
                    if (bot_idx < chat_messages_.size()) {
                        chat_messages_[bot_idx].executed_cmds = executed_cmds;
                    }
                }
                if (!chunk_msg_queued_.exchange(true)) {
                    PostMessageW(hwnd_main_, WM_APP_CHAT_CHUNK, 0, 0);
                }
                log_console("[SANDBOX]", "Chạy lệnh (#" + std::to_string(executed_cmds.size()) + "): " + cmd, 10);

                sandbox::ExecutionResult res = engine->execute_command(cmd, 30);
                combined_out += "[Command: " + cmd + "]\n[Exit Code: " + std::to_string(res.exit_code) + "]\n";
                if (res.timed_out) {
                    combined_out += "[TIMEOUT]: Lệnh vượt quá giới hạn 30 giây của Win32_Jail và đã bị hủy.\n";
                }
                if (!res.stdout_output.empty()) combined_out += "[STDOUT]:\n" + truncate_sandbox_out(res.stdout_output) + "\n";
                if (!res.stderr_output.empty()) combined_out += "[STDERR]:\n" + truncate_sandbox_out(res.stderr_output) + "\n";
                log_console("[SANDBOX]", "Output: " + truncate_sandbox_out(res.stdout_output, 600), 7);
            }

            if (identical_cmd_streak >= 3) {
                combined_out += "\n[System Notice]: Bạn đã lặp lại chính xác cùng một lệnh 3 lần liên tiếp. Hãy đổi hướng xử lý hoặc tổng kết kết quả cho người dùng, không lặp lại lệnh cũ.\n";
            }

            history.push_back({ "user", "[Terminal Output from Sandbox]:\n" + combined_out });
        }

        {
            std::lock_guard<std::mutex> lock(chat_mutex_);
            if (bot_idx < chat_messages_.size()) {
                chat_messages_[bot_idx].is_streaming = false;
                chat_messages_[bot_idx].executed_cmds = executed_cmds;
                if (chat_interrupt_flag_) {
                    if (chat_messages_[bot_idx].text.empty()) {
                        chat_messages_[bot_idx].text = "[Đã dừng phản hồi bởi người dùng]";
                    } else {
                        chat_messages_[bot_idx].text += "\n\n*[Đã dừng bởi người dùng]*";
                    }
                } else if (!last_resp.success && chat_messages_[bot_idx].text.empty()) {
                    chat_messages_[bot_idx].text = "Lỗi kết nối AI: " + last_resp.error_message + "\nKiểm tra lại cấu hình Endpoint và Model trong phần Cài đặt.";
                }
            }
            today_tasks_.insert(today_tasks_.begin(), {
                utf8_safe_substr(text, 36),
                "Đã phản hồi trực tiếp (Stream) trong phiên chat",
                get_current_time_str(),
                true
            });
        }
        is_chat_generating_ = false;

        PostMessageW(hwnd_main_, WM_APP_CHAT_DONE, 0, 0);
    }).detach();
}

void NativeGui::on_stop_task() {
    if (is_chat_generating_) {
        chat_interrupt_flag_ = true;
        log_console("[SYS]", "Người dùng đã dừng tác vụ stream AI đang chạy.", 12);
        InvalidateRect(hwnd_main_, NULL, FALSE);
    }
}

void NativeGui::on_reset_chat() {
    if (is_chat_generating_) {
        chat_interrupt_flag_ = true;
    }
    {
        std::lock_guard<std::mutex> lock(chat_mutex_);
        chat_messages_.clear();

        GuiMessage greet;
        greet.sender = "muse";
        greet.sender_name = "Muse";
        greet.role = "";
        greet.timestamp = get_current_time_str();
        greet.color = RGB(139, 92, 246);
        greet.text = "Cuộc trò chuyện đã được làm mới! Tôi sẵn sàng cho tác vụ tiếp theo của bạn.";
        chat_messages_.push_back(greet);
    }
    chat_scroll_y_ = 0;
    target_chat_scroll_y_ = 0;
    chat_scroll_float_ = 0.0f;
    auto_scroll_chat_ = true;
    InvalidateRect(hwnd_main_, NULL, FALSE);
    log_console("[SYS]", "Đã làm mới phiên chat (+ Mới).", 14);
}

void NativeGui::on_arena_toggle() {
    if (arena_running_) {
        arena_running_ = false;
        log_console("[ARENA]", "Đã tạm dừng thảo luận đa agent.", 14);
        if (arena_thread_.joinable()) arena_thread_.join();
    } else {
        arena_running_ = true;
        auto_scroll_arena_ = true;
        log_console("[ARENA]", "Bắt đầu chu trình tranh luận vô hạn giữa Muse, CodeCraft và Nova!", 10);
        arena_thread_ = std::thread(&NativeGui::arena_worker_loop, this);
    }
    InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::arena_worker_loop() {
    int consecutive_errors = 0;
    while (arena_running_) {
        const AgentInfo& agent = arena_agents_[arena_speaker_idx_ % arena_agents_.size()];
        arena_active_speaker_ = agent.name;
        PostMessageW(hwnd_main_, WM_APP_ARENA_UPDATE, 0, 0);

        // Extract recent messages (window = 3) to prevent repetition loops and detect user interjections
        std::string debate_transcript;
        std::string latest_user_interjection;
        size_t msg_idx = 0;
        {
            std::lock_guard<std::mutex> lock(arena_mutex_);
            std::vector<const GuiMessage*> recent_valid;
            for (const auto& m : arena_messages_) {
                if (m.text.empty() || m.text.find("[Lỗi") != std::string::npos) continue;
                recent_valid.push_back(&m);
                if (m.sender == "user") {
                    latest_user_interjection = m.text;
                }
            }
            int window_size = arena_context_window_;
            size_t start_idx = 0;
            if (window_size > 0 && recent_valid.size() > (size_t)window_size) {
                start_idx = recent_valid.size() - (size_t)window_size;
            }
            for (size_t k = start_idx; k < recent_valid.size(); ++k) {
                const auto* m = recent_valid[k];
                if (!debate_transcript.empty()) debate_transcript += "\n\n";
                // Không giới hạn số lượng ký tự, giữ nguyên vẹn nội dung của từng tin nhắn
                debate_transcript += m->sender_name + ": " + m->text;
            }

            GuiMessage placeholder;
            placeholder.sender = agent.id;
            placeholder.sender_name = agent.name;
            placeholder.role = "";
            placeholder.timestamp = get_current_time_str();
            placeholder.color = agent.color;
            placeholder.is_streaming = true;
            arena_messages_.push_back(placeholder);
            msg_idx = arena_messages_.size() - 1;
        }
        auto_scroll_arena_ = true;
        PostMessageW(hwnd_main_, WM_APP_ARENA_UPDATE, 0, 0);

        // Basic Sandbox System Prompt customized with agent's name to prevent persona drift
        std::vector<ai::ChatMessage> hist;
        hist.push_back({ "system", build_system_prompt(agent.name) });

        std::string user_turn = "Chủ đề thảo luận: " + arena_topic_ + "\n\n";
        if (!latest_user_interjection.empty()) {
            user_turn += "Ý kiến gần nhất:\n" + debate_transcript +
                         "\n\n[Người dùng can thiệp]: \"" + latest_user_interjection +
                         "\"\nHãy phản hồi trực tiếp yêu cầu này:";
        } else if (debate_transcript.empty()) {
            user_turn += "Hãy bắt đầu phát biểu ý kiến chuyên môn của bạn về chủ đề trên:";
        } else {
            user_turn += "Ý kiến phát biểu gần nhất:\n" + debate_transcript +
                         "\n\nĐến lượt " + agent.name + ". Đưa ra 1 góc nhìn mới hoặc phản biện ngắn gọn (không lặp lại ý kiến phía trên, không xưng là người khác):";
        }
        hist.push_back({ "user", user_turn });

        ai::AiConfig cfg = get_config_for_agent(agent);
        ai::AiClient temp_client;
        temp_client.set_config(cfg);

        log_console("[ARENA]", agent.name + " đang phát biểu (Stream)...", 11);
        ai::AiResponse resp = temp_client.chat_completion_stream(hist, [&](const ai::StreamChunk& chunk) -> bool {
            if (!arena_running_) return false;
            if (!chunk.delta_content.empty()) {
                {
                    std::lock_guard<std::mutex> lock(arena_mutex_);
                    if (msg_idx < arena_messages_.size()) {
                        arena_messages_[msg_idx].text += chunk.delta_content;
                    }
                }
                if (!chunk_msg_queued_.exchange(true)) {
                    PostMessageW(hwnd_main_, WM_APP_ARENA_UPDATE, 0, 0);
                }
            }
            return arena_running_.load();
        });

        auto clean_thinking_preamble = [](std::string& s) {
            // 1. If wrapped in standard <think> tags (even unclosed due to token limit)
            size_t open_think = s.find("<think>");
            if (open_think != std::string::npos) {
                size_t close_think = s.find("</think>", open_think);
                if (close_think != std::string::npos) {
                    s = s.substr(0, open_think) + s.substr(close_think + 8);
                } else {
                    // Unclosed <think> tag: everything after is raw thoughts -> remove
                    s = s.substr(0, open_think);
                }
            }

            // 2. Common raw thinking markers
            const std::string markers[] = {
                "Thinking Process:", "Here's a thinking process", "Thinking Process\n", "**Thinking Process:**",
                "Quá trình suy nghĩ:", "Quy trình suy nghĩ:", "Suy nghĩ của tôi:", "Suy nghĩ:"
            };
            for (const auto& m : markers) {
                size_t p = s.find(m);
                if (p != std::string::npos) {
                    size_t cut = s.find("\n\n**Response:**", p);
                    if (cut == std::string::npos) cut = s.find("\n\nResponse:", p);
                    if (cut == std::string::npos) cut = s.find("\n\nPhát biểu:", p);
                    if (cut == std::string::npos) cut = s.find("\n\n[Ý kiến]:", p);
                    if (cut == std::string::npos) cut = s.find("\n\n**Ý kiến:**", p);
                    if (cut == std::string::npos) cut = s.find("\n\n**Trả lời:**", p);
                    if (cut == std::string::npos) {
                        size_t last_num = s.rfind("\n3.");
                        if (last_num == std::string::npos) last_num = s.rfind("\n2.");
                        if (last_num != std::string::npos) {
                            size_t after_num = s.find("\n\n", last_num);
                            if (after_num != std::string::npos) cut = after_num;
                        }
                    }
                    if (cut != std::string::npos) {
                        size_t next_line = s.find('\n', cut + 2);
                        if (next_line != std::string::npos && next_line + 1 < s.size()) {
                            s = s.substr(next_line + 1);
                        } else {
                            s = s.substr(cut + 2);
                        }
                    } else if (p == 0) {
                        size_t dnl = s.find("\n\n\n");
                        if (dnl != std::string::npos) s = s.substr(dnl + 3);
                        else {
                            // If marker is at start but no clear response header found, cut up to double newline
                            size_t first_p = s.find("\n\n");
                            if (first_p != std::string::npos) s = s.substr(first_p + 2);
                        }
                    }
                }
            }
            while (!s.empty() && (s.front() == ' ' || s.front() == '\n' || s.front() == '\r')) {
                s.erase(0, 1);
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r')) {
                s.pop_back();
            }
        };

        {
            std::lock_guard<std::mutex> lock(arena_mutex_);
            if (msg_idx < arena_messages_.size()) {
                arena_messages_[msg_idx].is_streaming = false;
                if (!resp.success && arena_messages_[msg_idx].text.empty()) {
                    arena_messages_[msg_idx].text = "[Lỗi kết nối AI: " + resp.error_message + " — Vui lòng kiểm tra mô hình/API Key trong tab Cài đặt ⚙️]";
                    consecutive_errors++;
                } else if (arena_messages_[msg_idx].text.empty() && !resp.content.empty()) {
                    arena_messages_[msg_idx].text = resp.content;
                    consecutive_errors = 0;
                } else {
                    consecutive_errors = 0;
                }
                clean_thinking_preamble(arena_messages_[msg_idx].text);

                size_t active_idx = arena_speaker_idx_ % arena_agents_.size();
                arena_messages_[msg_idx].role = "";

                // 1. Extract bash commands if agent proposed files or bash executions in sandbox
                std::vector<std::string> cmds = ai::AiClient::extract_commands(arena_messages_[msg_idx].text);
                if (!cmds.empty()) {
                    auto* engine = sandbox_mgr_->get_engine();
                    std::string combined_out = "";
                    for (const auto& cmd : cmds) {
                        if (!arena_running_) break;
                        arena_messages_[msg_idx].executed_cmds.push_back(cmd);
                        log_console("[ARENA-SANDBOX]", agent.name + " chạy: " + cmd, 10);
                        sandbox::ExecutionResult res = engine->execute_command(cmd, 30);
                        combined_out += "[Command: " + cmd + "]\n[Exit Code: " + std::to_string(res.exit_code) + "]\n";
                        if (res.timed_out) {
                            combined_out += "[TIMEOUT]: Lệnh vượt quá giới hạn 30 giây trong Sandbox.\n";
                        }
                        if (!res.stdout_output.empty()) combined_out += "[STDOUT]:\n" + truncate_sandbox_out(res.stdout_output) + "\n";
                        if (!res.stderr_output.empty()) combined_out += "[STDERR]:\n" + truncate_sandbox_out(res.stderr_output) + "\n";
                    }
                    if (!combined_out.empty()) {
                        arena_messages_[msg_idx].text += "\n\n> ⚡ **Kết quả thực thi Sandbox:**\n```\n" + combined_out + "```";
                    }
                }

                log_console("[ARENA]", arena_agents_[active_idx].name + ": " + arena_messages_[msg_idx].text, 15);
            }
        }

        auto_save_active_arena();

        if (!arena_running_) break;

        if (consecutive_errors >= 3) {
            arena_running_ = false;
            log_console("[ARENA]", "Đã tạm dừng thảo luận đa agent do gặp lỗi kết nối AI liên tiếp.", 12);
            PostMessageW(hwnd_main_, WM_APP_ARENA_UPDATE, 0, 0);
            break;
        }

        arena_speaker_idx_++;
        PostMessageW(hwnd_main_, WM_APP_ARENA_UPDATE, 0, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(arena_delay_ms_));
    }
    arena_active_speaker_ = "";
    PostMessageW(hwnd_main_, WM_APP_ARENA_UPDATE, 0, 0);
}

void NativeGui::on_arena_interject() {
    wchar_t buf[1024] = { 0 };
    GetWindowTextW(h_arena_interject_input_, buf, 1024);
    std::string text = ws2s(buf);
    if (text.empty()) return;

    SetWindowTextW(h_arena_interject_input_, L"");

    GuiMessage msg;
    msg.sender = "user";
    msg.sender_name = "Tri";
    msg.role = "";
    msg.timestamp = get_current_time_str();
    msg.color = RGB(255, 255, 255);
    msg.text = text;

    {
        std::lock_guard<std::mutex> lock(arena_mutex_);
        arena_messages_.push_back(msg);
    }
    auto_save_active_arena();
    InvalidateRect(hwnd_main_, NULL, FALSE);
    log_console("[ARENA-USER]", "Can thiệp: " + text, 14);
}

void NativeGui::on_arena_change_topic() {
    show_topic_modal_ = true;
    if (h_arena_topic_input_) {
        SetWindowTextW(h_arena_topic_input_, s2ws(arena_topic_).c_str());
        ShowWindow(h_arena_topic_input_, SW_SHOW);
    }
    RECT rc;
    GetClientRect(hwnd_main_, &rc);
    layout_controls(rc.right, rc.bottom);
    SetFocus(h_arena_topic_input_);
    SendMessageW(h_arena_topic_input_, EM_SETSEL, 0, -1);
    InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::on_arena_export() {
    std::ofstream ofs("arena_transcript.md");
    if (ofs.is_open()) {
        ofs << "# BIÊN BẢN TRANH LUẬN MULTI-AGENT ARENA\n\n";
        ofs << "**Chủ đề**: " << arena_topic_ << "\n\n---\n\n";
        std::lock_guard<std::mutex> lock(arena_mutex_);
        for (const auto& m : arena_messages_) {
            ofs << "### " << m.sender_name << " - " << m.timestamp << "\n";
            ofs << m.text << "\n\n";
        }
        MessageBoxW(hwnd_main_, L"✓ Đã xuất biên bản thành công vào: arena_transcript.md", L"Xuất biên bản", MB_OK);
    }
}

void NativeGui::auto_save_active_arena() {
    try {
        json::Value root = json::Value::object();
        root["topic"] = arena_topic_;
        root["swarm_size"] = (int)swarm_size_;
        root["context_window"] = (int)arena_context_window_;
        root["delay_ms"] = (int)arena_delay_ms_.load();
        root["updated_at"] = get_current_time_str();

        json::Value msgs = json::Value::array();
        {
            std::lock_guard<std::mutex> lock(arena_mutex_);
            for (const auto& m : arena_messages_) {
                if (m.text.empty() || m.is_streaming) continue;
                json::Value item = json::Value::object();
                item["sender"] = m.sender;
                item["sender_name"] = m.sender_name;
                item["timestamp"] = m.timestamp;
                item["color"] = (int)m.color;
                item["text"] = m.text;
                if (!m.executed_cmds.empty()) {
                    json::Value cmdsArr = json::Value::array();
                    for (const auto& c : m.executed_cmds) cmdsArr.push_back(c);
                    item["executed_cmds"] = cmdsArr;
                }
                msgs.push_back(item);
            }
        }
        root["messages"] = msgs;
        std::string jsonStr = root.dump();
        if (sandbox_mgr_ && sandbox_mgr_->get_engine()) {
            sandbox_mgr_->get_engine()->write_file("arena_active_session.json", jsonStr);
        } else {
            write_file_content("arena_active_session.json", jsonStr);
        }
    } catch (...) {}
}

void NativeGui::auto_load_active_arena() {
    std::string content;
    if (sandbox_mgr_ && sandbox_mgr_->get_engine()) {
        content = sandbox_mgr_->get_engine()->read_file("arena_active_session.json");
        if (content.rfind("Error:", 0) == 0) content.clear();
    }
    if (content.empty()) {
        content = read_file_content("arena_active_session.json", "");
    }
    if (content.empty()) return;
    try {
        json::Value root = json::parse(content);
        if (root.contains("topic")) arena_topic_ = root["topic"].as_string();
        if (root.contains("swarm_size")) set_swarm_size(root["swarm_size"].as_int());
        if (root.contains("context_window")) arena_context_window_ = root["context_window"].as_int();
        if (root.contains("delay_ms")) arena_delay_ms_ = root["delay_ms"].as_int();
        if (root.contains("messages") && root["messages"].is_array()) {
            std::lock_guard<std::mutex> lock(arena_mutex_);
            arena_messages_.clear();
            for (size_t i = 0; i < root["messages"].size(); ++i) {
                const auto& item = root["messages"][i];
                GuiMessage m;
                m.sender = item.contains("sender") ? item["sender"].as_string() : "agent";
                m.sender_name = item.contains("sender_name") ? item["sender_name"].as_string() : "Agent";
                m.timestamp = item.contains("timestamp") ? item["timestamp"].as_string() : "";
                m.color = item.contains("color") ? (COLORREF)item["color"].as_int() : RGB(255, 255, 255);
                m.text = item.contains("text") ? item["text"].as_string() : "";
                m.is_streaming = false;
                m.role = "";
                if (item.contains("executed_cmds") && item["executed_cmds"].is_array()) {
                    for (size_t ci = 0; ci < item["executed_cmds"].size(); ++ci) {
                        m.executed_cmds.push_back(item["executed_cmds"][ci].as_string());
                    }
                }
                arena_messages_.push_back(m);
            }
        }
        log_console("[ARENA-SANDBOX]", "Đã khôi phục phiên từ Sandbox: " + arena_topic_ + " (" + std::to_string(arena_messages_.size()) + " tin nhắn)", 10);
    } catch (...) {}
}

void NativeGui::on_arena_save_session() {
    auto_save_active_arena();

    time_t now = time(nullptr);
    std::string filename = "arena_" + std::to_string(now) + ".json";
    std::string relJsonPath = "history/" + filename;
    std::string relMdPath = "history/arena_" + std::to_string(now) + ".md";

    std::string content;
    if (sandbox_mgr_ && sandbox_mgr_->get_engine()) {
        content = sandbox_mgr_->get_engine()->read_file("arena_active_session.json");
        if (content.rfind("Error:", 0) == 0) content.clear();
    }
    if (content.empty()) {
        content = read_file_content("arena_active_session.json", "");
    }

    if (!content.empty()) {
        if (sandbox_mgr_ && sandbox_mgr_->get_engine()) {
            sandbox_mgr_->get_engine()->write_file(relJsonPath, content);
        }

        std::string mdText = "# PHIÊN TRANH LUẬN MULTI-AGENT ARENA (SANDBOX)\n\n";
        mdText += "**Chủ đề**: " + arena_topic_ + "\n\n";
        mdText += "**Thời gian**: " + get_current_time_str() + "\n\n---\n\n";
        {
            std::lock_guard<std::mutex> lock(arena_mutex_);
            for (const auto& m : arena_messages_) {
                mdText += "### " + m.sender_name + " - " + m.timestamp + "\n";
                mdText += m.text + "\n\n";
                for (const auto& cmd : m.executed_cmds) {
                    mdText += "> `⚡ Sandbox: " + cmd + "`\n\n";
                }
            }
        }
        if (sandbox_mgr_ && sandbox_mgr_->get_engine()) {
            sandbox_mgr_->get_engine()->write_file(relMdPath, mdText);
        }

        auto st = sandbox_mgr_->get_status();
        std::string fullSavedPath = st.workspace_path + "/" + relJsonPath;
        log_console("[ARENA-SANDBOX]", "Đã lưu phiên vào Sandbox Workspace: " + fullSavedPath, 10);
        MessageBoxW(hwnd_main_, (L"✓ Đã lưu phiên thảo luận vào Sandbox Workspace:\n" + s2ws(fullSavedPath)).c_str(), L"Lưu phiên Đấu trường (Sandbox)", MB_OK);
    }
}

void NativeGui::refresh_saved_sessions() {
    cached_sessions_.clear();
    if (!sandbox_mgr_ || !sandbox_mgr_->get_engine()) return;

    auto files = sandbox_mgr_->get_engine()->list_files("history");
    for (const auto& f : files) {
        if (!f.is_directory && f.name.rfind("arena_", 0) == 0 && f.name.size() > 5 && f.name.substr(f.name.size() - 5) == ".json") {
            std::string relPath = "history/" + f.name;
            std::string content = sandbox_mgr_->get_engine()->read_file(relPath);
            if (content.empty() || content.rfind("Error:", 0) == 0) continue;
            try {
                json::Value root = json::parse(content);
                views::SessionInfo s;
                s.filepath = relPath;
                s.filename = f.name;
                s.topic = root.contains("topic") ? root["topic"].as_string() : "(Chưa có chủ đề)";
                s.time_str = root.contains("updated_at") ? root["updated_at"].as_string() : "";
                s.swarm_size = root.contains("swarm_size") ? root["swarm_size"].as_int() : 3;
                s.message_count = (root.contains("messages") && root["messages"].is_array()) ? (int)root["messages"].size() : 0;
                cached_sessions_.push_back(s);
            } catch (...) {}
        }
    }

    // Sort newest first
    std::sort(cached_sessions_.begin(), cached_sessions_.end(), [](const views::SessionInfo& a, const views::SessionInfo& b) {
        return a.filename > b.filename;
    });
}

void NativeGui::on_arena_open_sessions() {
    refresh_saved_sessions();
    session_modal_.set_visible(true);
    InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::on_arena_load_session(int index) {
    if (index < 0 || index >= (int)cached_sessions_.size()) return;
    const auto& s = cached_sessions_[index];
    if (!sandbox_mgr_ || !sandbox_mgr_->get_engine()) return;

    std::string content = sandbox_mgr_->get_engine()->read_file(s.filepath);
    if (content.empty() || content.rfind("Error:", 0) == 0) return;
    try {
        json::Value root = json::parse(content);
        if (root.contains("topic")) arena_topic_ = root["topic"].as_string();
        if (root.contains("swarm_size")) set_swarm_size(root["swarm_size"].as_int());
        if (root.contains("context_window")) arena_context_window_ = root["context_window"].as_int();
        if (root.contains("delay_ms")) arena_delay_ms_ = root["delay_ms"].as_int();
        if (root.contains("messages") && root["messages"].is_array()) {
            std::lock_guard<std::mutex> lock(arena_mutex_);
            arena_messages_.clear();
            for (size_t i = 0; i < root["messages"].size(); ++i) {
                const auto& item = root["messages"][i];
                GuiMessage m;
                m.sender = item.contains("sender") ? item["sender"].as_string() : "agent";
                m.sender_name = item.contains("sender_name") ? item["sender_name"].as_string() : "Agent";
                m.timestamp = item.contains("timestamp") ? item["timestamp"].as_string() : "";
                m.color = item.contains("color") ? (COLORREF)item["color"].as_int() : RGB(255, 255, 255);
                m.text = item.contains("text") ? item["text"].as_string() : "";
                m.is_streaming = false;
                m.role = "";
                if (item.contains("executed_cmds") && item["executed_cmds"].is_array()) {
                    for (size_t ci = 0; ci < item["executed_cmds"].size(); ++ci) {
                        m.executed_cmds.push_back(item["executed_cmds"][ci].as_string());
                    }
                }
                arena_messages_.push_back(m);
            }
        }
        session_modal_.set_visible(false);
        auto_scroll_arena_ = true;
        auto_save_active_arena();
        InvalidateRect(hwnd_main_, NULL, FALSE);
        log_console("[ARENA-SANDBOX]", "Đã nạp phiên tranh luận từ Sandbox: " + s.topic, 10);
    } catch (...) {}
}

void NativeGui::on_arena_delete_session(int index) {
    if (index < 0 || index >= (int)cached_sessions_.size()) return;
    const auto& s = cached_sessions_[index];
    if (sandbox_mgr_ && sandbox_mgr_->get_engine()) {
        sandbox_mgr_->get_engine()->delete_file(s.filepath);
        std::string mdPath = s.filepath;
        if (mdPath.size() > 5 && mdPath.substr(mdPath.size() - 5) == ".json") {
            mdPath = mdPath.substr(0, mdPath.size() - 5) + ".md";
            sandbox_mgr_->get_engine()->delete_file(mdPath);
        }
        log_console("[ARENA-SANDBOX]", "Đã xóa phiên trong Sandbox: " + s.filename, 14);
    }
    refresh_saved_sessions();
    InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::on_settings_provider_changed() {
    int sel = (int)SendMessageW(h_cfg_provider_combo_, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)providers_.size()) return;

    const auto& p = providers_[sel];
    active_provider_id_ = p.id;

    SetWindowTextW(h_cfg_base_url_, s2ws(p.base_url).c_str());
    SetWindowTextW(h_cfg_api_key_, s2ws(p.api_key).c_str());
    SetWindowTextW(h_cfg_model_, s2ws(p.model).c_str());

    ai_client_->set_config(p);
    log_console("[CONFIG]", "Đã chọn nhà cung cấp: " + p.name + " (" + p.model + ")", 11);
}

void NativeGui::on_settings_save() {
    int sel = (int)SendMessageW(h_cfg_provider_combo_, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)providers_.size()) return;

    wchar_t bUrl[512] = { 0 }, bKey[512] = { 0 }, bMod[256] = { 0 };
    GetWindowTextW(h_cfg_base_url_, bUrl, 512);
    GetWindowTextW(h_cfg_api_key_, bKey, 512);
    GetWindowTextW(h_cfg_model_, bMod, 256);

    {
        std::lock_guard<std::mutex> lock(providers_mutex_);
        providers_[sel].base_url = ws2s(bUrl);
        providers_[sel].api_key = ws2s(bKey);
        providers_[sel].model = ws2s(bMod);
        active_provider_id_ = providers_[sel].id;
        ai_client_->set_config(providers_[sel]);
    }

    int engSel = (int)SendMessageW(h_cfg_engine_combo_, CB_GETCURSEL, 0, 0);
    std::string targetEng = (engSel == 1) ? "docker" : "win32";
    sandbox_mgr_->switch_engine(targetEng);
    auto st = sandbox_mgr_->get_status();
    if (engSel == 1 && st.engine_type != "Docker") {
        SendMessageW(h_cfg_engine_combo_, CB_SETCURSEL, 0, 0);
        MessageBoxW(hwnd_main_, L"Docker Daemon (\\\\.\\pipe\\docker_engine) hiện không hoạt động trên máy.\nHệ thống giữ nguyên động cơ thực tế: Win32_Jail (JobObject + Low-Integrity).", L"Thông báo Sandbox", MB_ICONWARNING);
    }

    init_today_tasks();
    save_config_to_disk();
    InvalidateRect(hwnd_main_, NULL, FALSE);
    MessageBoxW(hwnd_main_, L"✓ Đã lưu cấu hình AI & đồng bộ trạng thái Sandbox thực tế!", L"Cài đặt", MB_OK);
    log_console("[CONFIG]", "Đã lưu cài đặt cho provider: " + providers_[sel].name + " | Sandbox: " + st.engine_type, 10);
}

void NativeGui::on_settings_test() {
    int sel = (int)SendMessageW(h_cfg_provider_combo_, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)providers_.size()) return;

    wchar_t bUrl[512] = { 0 }, bKey[512] = { 0 }, bMod[256] = { 0 };
    GetWindowTextW(h_cfg_base_url_, bUrl, 512);
    GetWindowTextW(h_cfg_api_key_, bKey, 512);
    GetWindowTextW(h_cfg_model_, bMod, 256);

    ai::AiConfig cfg;
    cfg.id = providers_[sel].id;
    cfg.name = providers_[sel].name;
    cfg.base_url = ws2s(bUrl);
    cfg.api_key = ws2s(bKey);
    cfg.model = ws2s(bMod);

    log_console("[TEST]", "Đang kiểm tra kết nối tới: " + cfg.base_url + " (Model: " + cfg.model + ")...", 14);

    std::thread([this, cfg]() {
        ai::AiClient test_client;
        test_client.set_config(cfg);
        std::vector<ai::ChatMessage> hist = { { "user", "Hello! Respond with: 'OK'" } };
        auto resp = test_client.chat_completion(hist);

        if (resp.success) {
            MessageBoxW(hwnd_main_, L"✓ Kết nối thành công! AI phản hồi hợp lệ.", L"Kiểm tra kết nối", MB_OK);
            log_console("[TEST]", "Kết nối thành công! Response: " + resp.content, 10);
        } else {
            std::wstring err = L"[-] Kết nối thất bại: " + s2ws(resp.error_message);
            MessageBoxW(hwnd_main_, err.c_str(), L"Lỗi kết nối", MB_ICONERROR);
            log_console("[TEST]", "Thất bại: " + resp.error_message, 12);
        }
    }).detach();
}

void NativeGui::on_airgap_toggle() {
    auto status = sandbox_mgr_->get_status();
    bool newState = !status.internet_enabled;
    sandbox_mgr_->get_engine()->set_internet_access(newState);
    init_today_tasks();
    timeline_logs_.push_back("[" + get_current_time_str() + "] Chuyển mạng: " + (newState ? "ONLINE" : "AIR-GAP"));
    InvalidateRect(hwnd_main_, NULL, FALSE);
    log_console("[NET]", newState ? "Chế độ: ONLINE (Cho phép AI ra ngoài Internet)" : "Chế độ: AIR-GAP (Ngắt toàn bộ mạng ra ngoài)", newState ? 10 : 12);
}

void NativeGui::on_anti_capture_toggle() {
    auto& anti = security::AntiScreenshot::instance();
    anti.toggle();
    auto st = anti.get_status();
    std::string stateStr = st.enabled ? "BẬT" : "TẮT";
    timeline_logs_.push_back("[" + get_current_time_str() + "] SEB Anti-Screenshot: " + stateStr);
    if (st.enabled) {
        log_console("[SECURITY]", "Đã BẬT bảo vệ chống chụp màn hình Safe Exam Browser (Hardware DWM Exclusion WDA_EXCLUDEFROMCAPTURE + Block PrintScreen).", 10);
    } else {
        log_console("[SECURITY]", "Đã TẮT bảo vệ chống chụp màn hình. Cửa sổ hiển thị bình thường với OBS / Snipping Tool.", 14);
    }
    InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::init_ui_engine() {
    ui_dock_rail_ = std::make_shared<cyber_ui::UIDockRail>();
    ui_dock_rail_->set_vector_icon_drawer([this](cyber_ui::UIRenderContext& ctx, int icon_idx, float cx, float cy, const cyber_ui::UIColor& col) {
        Gdiplus::Graphics* dummyG = NULL;
        Gdiplus::Color gdiCol((BYTE)(col.a * 255), (BYTE)(col.r * 255), (BYTE)(col.g * 255), (BYTE)(col.b * 255));
        draw_dock_vector_icon(*dummyG, icon_idx, cx, cy, gdiCol);
    });

    ui_dock_rail_->set_on_icon_click([this](int icon_idx) {
        active_dock_icon_ = icon_idx;
        if (icon_idx == 0) switch_tab(ActiveTab::CHAT);
        else if (icon_idx == 2) switch_tab(ActiveTab::WORKSPACE);
        else if (icon_idx == 5) switch_tab(ActiveTab::ARENA);
        else if (icon_idx == 6) switch_tab(ActiveTab::SETTINGS);
        InvalidateRect(hwnd_main_, NULL, FALSE);
    });

    ui_dock_rail_->set_on_toggle_sidebar([this]() {
        show_right_panel_ = !show_right_panel_;
        right_panel_target_w_ = show_right_panel_ ? 310.0f : 0.0f;
        InvalidateRect(hwnd_main_, NULL, FALSE);
    });

    ui_top_bar_ = std::make_shared<cyber_ui::UITopBar>();
    ui_top_bar_->set_on_new_chat([this]() {
        on_reset_chat();
    });
    ui_top_bar_->set_on_toggle_security([this]() {
        on_airgap_toggle();
    });
}

void NativeGui::on_ws_refresh() {
    SendMessageW(h_ws_listbox_, LB_RESETCONTENT, 0, 0);
    auto files = sandbox_mgr_->get_engine()->list_files();
    for (const auto& f : files) {
        std::wstring line = (f.is_directory ? L"[DIR] " : L"[FILE] ") + s2ws(f.name);
        SendMessageW(h_ws_listbox_, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

void NativeGui::on_ws_file_select() {
    int sel = (int)SendMessageW(h_ws_listbox_, LB_GETCURSEL, 0, 0);
    if (sel < 0) return;

    wchar_t buf[256] = { 0 };
    SendMessageW(h_ws_listbox_, LB_GETTEXT, sel, (LPARAM)buf);
    std::wstring str(buf);
    size_t sp = str.find(L"] ");
    if (sp != std::wstring::npos) str = str.substr(sp + 2);

    std::string path = ws2s(str);
    current_ws_file_ = path;
    std::string content = sandbox_mgr_->get_engine()->read_file(path);
    SetWindowTextW(h_ws_preview_edit_, s2ws(content).c_str());
    InvalidateRect(hwnd_main_, NULL, FALSE);
}

void NativeGui::on_ws_save_file() {
    if (current_ws_file_.empty()) {
        MessageBoxW(hwnd_main_, L"Vui lòng chọn một tệp trong danh sách để lưu!", L"CyberBox Canvas", MB_OK | MB_ICONWARNING);
        return;
    }
    int len = GetWindowTextLengthW(h_ws_preview_edit_);
    std::vector<wchar_t> buf(len + 1, 0);
    GetWindowTextW(h_ws_preview_edit_, buf.data(), len + 1);
    std::string content = ws2s(buf.data());

    bool ok = sandbox_mgr_->get_engine()->write_file(current_ws_file_, content);
    if (ok) {
        log_console("[CANVAS]", "Đã lưu tệp vào Sandbox Workspace: " + current_ws_file_, 10);
        MessageBoxW(hwnd_main_, (L"Đã lưu tệp thành công:\n" + s2ws(current_ws_file_)).c_str(), L"CyberBox Canvas", MB_OK | MB_ICONINFORMATION);
        on_ws_refresh();
    } else {
        log_console("[CANVAS]", "Lỗi khi lưu tệp vào Sandbox: " + current_ws_file_, 12);
        MessageBoxW(hwnd_main_, L"Không thể ghi tệp vào Sandbox!", L"Lỗi Canvas", MB_OK | MB_ICONERROR);
    }
}

void NativeGui::on_ws_open_folder() {
    auto status = sandbox_mgr_->get_status();
    std::string ws_path = status.workspace_path;
    if (ws_path.empty()) ws_path = "sandbox_workspace";
    std::filesystem::path p = std::filesystem::absolute(ws_path);
    if (!std::filesystem::exists(p)) {
        std::filesystem::create_directories(p);
    }
    ShellExecuteW(NULL, L"open", p.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
    log_console("[CANVAS]", "Đã mở thư mục Sandbox Workspace trên Windows Explorer: " + p.string(), 11);
}

void NativeGui::on_ws_run_cmd() {
    wchar_t buf[512] = { 0 };
    GetWindowTextW(h_ws_cmd_input_, buf, 512);
    std::string cmd = ws2s(buf);
    if (cmd.empty()) return;

    SetWindowTextW(h_ws_cmd_input_, L"");
    log_console("[SANDBOX-CMD]", "Thực thi: " + cmd, 14);

    auto res = sandbox_mgr_->get_engine()->execute_command(cmd, 30);
    std::string out = "[Exit Code: " + std::to_string(res.exit_code) + "]\n" + res.stdout_output;
    if (!res.stderr_output.empty()) out += "\n[STDERR]:\n" + res.stderr_output;

    SetWindowTextW(h_ws_preview_edit_, s2ws(out).c_str());
    log_console("[SANDBOX-CMD]", out, 7);
    on_ws_refresh();
}

bool NativeGui::is_point_on_top_bar_control(int x, int y) {
    if (show_topic_modal_ || session_modal_.is_visible()) return true;
    if (y < 0 || y >= 46) return false;

    RECT rc;
    GetClientRect(hwnd_main_, &rc);

    // 1. Window controls in top-right
    if (x >= rc.right - 138 && y < 34) return true;

    // 2. Left dock rail
    if (x < 48) return true;

    // 3. Session tab indicator pill [60, 10, sessionW, 30]
    float sessionW = (current_tab_ == ActiveTab::ARENA || current_tab_ == ActiveTab::WORKSPACE) ? 112.0f : 96.0f;
    if (x >= 60 && x <= 60 + (int)sessionW) return true;

    // 4. Chat '+ Mới' button
    float top_bar_right = (right_panel_cur_w_ > 146.0f) ? (float)(rc.right - (int)right_panel_cur_w_) : ((float)rc.right - 146.0f);
    if (current_tab_ == ActiveTab::CHAT && x >= (int)top_bar_right - 74 && x <= (int)top_bar_right - 14) return true;

    // 5. Security badge
    if (security_badge_rect_.Width > 0.0f && x >= security_badge_rect_.X && x <= security_badge_rect_.X + security_badge_rect_.Width) return true;

    // 6. Anti-capture badge
    if (anti_capture_badge_rect_.Width > 0.0f && x >= anti_capture_badge_rect_.X && x <= anti_capture_badge_rect_.X + anti_capture_badge_rect_.Width) return true;

    // 7. Arena top bar buttons
    if (current_tab_ == ActiveTab::ARENA) {
        if (arena_toggle_btn_rect_.Width > 0.0f && x >= arena_toggle_btn_rect_.X && x <= arena_toggle_btn_rect_.X + arena_toggle_btn_rect_.Width) return true;
        if (arena_topic_btn_rect_.Width > 0.0f && x >= arena_topic_btn_rect_.X && x <= arena_topic_btn_rect_.X + arena_topic_btn_rect_.Width) return true;
        if (arena_save_btn_rect_.Width > 0.0f && x >= arena_save_btn_rect_.X && x <= arena_save_btn_rect_.X + arena_save_btn_rect_.Width) return true;
        if (arena_sessions_btn_rect_.Width > 0.0f && x >= arena_sessions_btn_rect_.X && x <= arena_sessions_btn_rect_.X + arena_sessions_btn_rect_.Width) return true;
        if (arena_export_btn_rect_.Width > 0.0f && x >= arena_export_btn_rect_.X && x <= arena_export_btn_rect_.X + arena_export_btn_rect_.Width) return true;
    }

    // 8. Right panel drawer collapse button (when drawer is open)
    if (right_panel_cur_w_ > 50.0f) {
        float pX = (float)(rc.right - (int)right_panel_cur_w_);
        if (x >= (int)pX + 14 && x <= (int)pX + 44 && y >= 10 && y <= 40) return true;
    }

    // 9. Floating muse avatar (when drawer is collapsed)
    if (right_panel_cur_w_ < 30.0f) {
        int chat_left = 48;
        int chat_w = rc.right - chat_left;
        float flX = (float)chat_left + ((float)chat_w - 48.0f) / 2.0f;
        if (x >= flX - 4 && x <= flX + 52 && y >= 8 && y <= 70) return true;
    }

    return false;
}

void NativeGui::render_window_controls(Gdiplus::Graphics& g, const RECT& rc) {
    float r = (float)rc.right;
    btn_win_min_rect_ = Gdiplus::RectF(r - 138.0f, 0.0f, 46.0f, 32.0f);
    btn_win_max_rect_ = Gdiplus::RectF(r - 92.0f, 0.0f, 46.0f, 32.0f);
    btn_win_close_rect_ = Gdiplus::RectF(r - 46.0f, 0.0f, 46.0f, 32.0f);

    // 1. Minimize Button
    if (hover_win_min_) {
        d2d_fill_rect(btn_win_min_rect_.X, btn_win_min_rect_.Y, btn_win_min_rect_.Width, btn_win_min_rect_.Height, Gdiplus::Color(255, 38, 40, 50));
    }
    Gdiplus::Color minCol = hover_win_min_ ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 160, 164, 178);
    d2d_draw_line(btn_win_min_rect_.X + 18.0f, 16.0f, btn_win_min_rect_.X + 28.0f, 16.0f, minCol, 1.4f);

    // 2. Maximize / Restore Button
    if (hover_win_max_) {
        d2d_fill_rect(btn_win_max_rect_.X, btn_win_max_rect_.Y, btn_win_max_rect_.Width, btn_win_max_rect_.Height, Gdiplus::Color(255, 38, 40, 50));
    }
    Gdiplus::Color maxCol = hover_win_max_ ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 160, 164, 178);
    bool isMax = IsZoomed(hwnd_main_);
    if (isMax) {
        // Restore symbol: two overlapping rectangles
        float mx = btn_win_max_rect_.X + 18.0f;
        draw_rounded_box(g, Gdiplus::RectF(mx + 3.0f, 10.0f, 8.0f, 8.0f), 0.5f, Gdiplus::Color(0,0,0,0), maxCol, 1.1f);
        draw_rounded_box(g, Gdiplus::RectF(mx, 13.0f, 8.0f, 8.0f), 0.5f, Gdiplus::Color(255, 20, 21, 25), maxCol, 1.1f);
    } else {
        // Maximize symbol: single rectangle
        float mx = btn_win_max_rect_.X + 18.0f;
        draw_rounded_box(g, Gdiplus::RectF(mx, 11.0f, 10.0f, 10.0f), 0.5f, Gdiplus::Color(0,0,0,0), maxCol, 1.2f);
    }

    // 3. Close Button
    if (hover_win_close_) {
        d2d_fill_rect(btn_win_close_rect_.X, btn_win_close_rect_.Y, btn_win_close_rect_.Width, btn_win_close_rect_.Height, Gdiplus::Color(255, 232, 17, 35));
    }
    Gdiplus::Color closeCol = hover_win_close_ ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 160, 164, 178);
    float cx = btn_win_close_rect_.X + 18.0f;
    d2d_draw_line(cx, 11.0f, cx + 10.0f, 21.0f, closeCol, 1.4f);
    d2d_draw_line(cx + 10.0f, 11.0f, cx, 21.0f, closeCol, 1.4f);
}

LRESULT NativeGui::handle_message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                NCCALCSIZE_PARAMS* pParams = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                if (IsZoomed(hwnd)) {
                    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi = { sizeof(mi) };
                    if (GetMonitorInfoW(hMon, &mi)) {
                        pParams->rgrc[0] = mi.rcWork;
                    }
                }
                return 0; // True borderless infinity window: strips OS title bar and borders completely!
            }
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }

        case WM_NCHITTEST: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // 1. Resizing borders if not maximized
            if (!IsZoomed(hwnd)) {
                const int border = 6;
                bool top = (pt.y < border);
                bool bottom = (pt.y >= rc.bottom - border);
                bool left = (pt.x < border);
                bool right = (pt.x >= rc.right - border);

                if (top && left) return HTTOPLEFT;
                if (top && right) return HTTOPRIGHT;
                if (bottom && left) return HTBOTTOMLEFT;
                if (bottom && right) return HTBOTTOMRIGHT;
                if (top) return HTTOP;
                if (bottom) return HTBOTTOM;
                if (left) return HTLEFT;
                if (right) return HTRIGHT;
            }

            // 2. Custom Window Control buttons in top-right
            if (pt.y >= 0 && pt.y < 34 && pt.x >= rc.right - 138) {
                return HTCLIENT;
            }

            // 3. Top bar area (y < 46)
            if (pt.y >= 0 && pt.y < 46) {
                // Interactive controls on the top bar
                if (is_point_on_top_bar_control(pt.x, pt.y)) {
                    return HTCLIENT;
                }
                // Empty space on top bar acts as window title bar for dragging & double-click maximize!
                return HTCAPTION;
            }

            return HTCLIENT;
        }

        case WM_GETMINMAXINFO: {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
            lpMMI->ptMinTrackSize.x = 960;
            lpMMI->ptMinTrackSize.y = 640;
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent redundant background erase on high-resolution displays

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            layout_controls(w, h);
            dock_indicator_target_y_ = get_dock_icon_y(active_dock_icon_, (float)h);
            if (use_dx11_) {
                dx11_renderer_.pre_resize();
                dx11_.resize(w, h);
                dx11_renderer_.post_resize(dx11_);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS && pDIS->CtlType == ODT_BUTTON) {
                draw_custom_button(pDIS);
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRc;
            GetClientRect(hwnd, &clientRc);
            int w = max(1, (int)(clientRc.right - clientRc.left));
            int h = max(1, (int)(clientRc.bottom - clientRc.top));

            if (use_dx11_) {
                render_window(hdc, clientRc);
                dx11_.present(1); // 120 FPS hardware presentation sync
            } else {
                // Reuse persistent 32-bit DIB backbuffer across frames (only reallocate on window resize!)
                if (!backbuffer_dc_ || !backbuffer_bmp_ || w != backbuffer_w_ || h != backbuffer_h_) {
                    if (backbuffer_dc_) {
                        if (backbuffer_old_bmp_) SelectObject(backbuffer_dc_, backbuffer_old_bmp_);
                        if (backbuffer_bmp_) DeleteObject(backbuffer_bmp_);
                        DeleteDC(backbuffer_dc_);
                        backbuffer_bits_ = nullptr;
                    }
                    backbuffer_dc_ = CreateCompatibleDC(hdc);
                    BITMAPINFO bmi = { 0 };
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = w;
                    bmi.bmiHeader.biHeight = -h; // Top-down 32-bit BGRA DIB for Direct2D
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;
                    void* pvBits = NULL;
                    backbuffer_bmp_ = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                    if (!backbuffer_bmp_) {
                        backbuffer_bmp_ = CreateCompatibleBitmap(hdc, w, h);
                    }
                    backbuffer_bits_ = pvBits;
                    backbuffer_old_bmp_ = (HBITMAP)SelectObject(backbuffer_dc_, backbuffer_bmp_);
                    backbuffer_w_ = w;
                    backbuffer_h_ = h;
                }

                render_window(backbuffer_dc_, clientRc);
                BitBlt(hdc, 0, 0, w, h, backbuffer_dc_, 0, 0, SRCCOPY);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);

            int chat_left = 48;
            int right_panel_w = (int)right_panel_cur_w_;
            float top_bar_right = (right_panel_cur_w_ > 146.0f) ? (float)(rc.right - (int)right_panel_cur_w_) : ((float)rc.right - 146.0f);
            int chat_right = (int)top_bar_right;
            int chat_w = (rc.right - right_panel_w) - chat_left;

            int prev_dock = hover_dock_icon_;
            int prev_rtab = hover_right_tab_;
            bool prev_new = hover_btn_new_;
            bool prev_session = hover_btn_session_;
            bool prev_plus = hover_btn_plus_;
            bool prev_send = hover_btn_send_;
            bool prev_close = hover_btn_close_;
            bool prev_fl = hover_floating_muse_;
            bool prev_arena_btn = hover_btn_arena_interject_;
            bool prev_arena_tog = hover_btn_arena_toggle_;
            bool prev_arena_top = hover_btn_arena_topic_;
            bool prev_arena_exp = hover_btn_arena_export_;
            bool prev_win_min = hover_win_min_;
            bool prev_win_max = hover_win_max_;
            bool prev_win_close = hover_win_close_;
            bool prev_drw_collapse = hover_btn_drawer_collapse_;
            bool prev_arena_save = hover_btn_arena_save_;
            bool prev_arena_sessions = hover_btn_arena_sessions_;
            bool prev_arena_ctx = hover_btn_context_;

            hover_dock_icon_ = -1;
            hover_right_tab_ = -1;
            hover_btn_new_ = false;
            hover_btn_session_ = false;
            hover_btn_plus_ = false;
            hover_btn_send_ = false;
            hover_btn_close_ = false;
            hover_btn_drawer_collapse_ = false;
            hover_win_min_ = false;
            hover_win_max_ = false;
            hover_win_close_ = false;
            hover_floating_muse_ = false;
            hover_btn_arena_interject_ = false;
            hover_btn_arena_toggle_ = false;
            hover_btn_arena_topic_ = false;
            hover_btn_arena_export_ = false;
            hover_btn_arena_save_ = false;
            hover_btn_arena_sessions_ = false;
            hover_btn_swarm_minus_ = false;
            hover_btn_swarm_plus_ = false;
            hover_btn_reset_roles_ = false;
            hover_btn_speed_ = false;
            hover_btn_context_ = false;
            hover_modal_apply_ = false;
            hover_modal_cancel_ = false;
            hover_modal_preset_ = -1;

            if (session_modal_.is_visible()) {
                if (session_modal_.handle_mouse_move(x, y)) {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }

            if (show_topic_modal_) {
                if (x >= modal_btn_apply_rect_.X && x <= modal_btn_apply_rect_.X + modal_btn_apply_rect_.Width &&
                    y >= modal_btn_apply_rect_.Y && y <= modal_btn_apply_rect_.Y + modal_btn_apply_rect_.Height) {
                    hover_modal_apply_ = true;
                } else if (x >= modal_btn_cancel_rect_.X && x <= modal_btn_cancel_rect_.X + modal_btn_cancel_rect_.Width &&
                           y >= modal_btn_cancel_rect_.Y && y <= modal_btn_cancel_rect_.Y + modal_btn_cancel_rect_.Height) {
                    hover_modal_cancel_ = true;
                }
                for (int i = 0; i < 4; ++i) {
                    if (x >= modal_preset_rects_[i].X && x <= modal_preset_rects_[i].X + modal_preset_rects_[i].Width &&
                        y >= modal_preset_rects_[i].Y && y <= modal_preset_rects_[i].Y + modal_preset_rects_[i].Height) {
                        hover_modal_preset_ = i;
                        break;
                    }
                }
                if (x >= modal_swarm_minus_rect_.X && x <= modal_swarm_minus_rect_.X + modal_swarm_minus_rect_.Width &&
                    y >= modal_swarm_minus_rect_.Y && y <= modal_swarm_minus_rect_.Y + modal_swarm_minus_rect_.Height) {
                    hover_btn_swarm_minus_ = true;
                } else if (x >= modal_swarm_plus_rect_.X && x <= modal_swarm_plus_rect_.X + modal_swarm_plus_rect_.Width &&
                           y >= modal_swarm_plus_rect_.Y && y <= modal_swarm_plus_rect_.Y + modal_swarm_plus_rect_.Height) {
                    hover_btn_swarm_plus_ = true;
                } else if (x >= modal_reset_roles_rect_.X && x <= modal_reset_roles_rect_.X + modal_reset_roles_rect_.Width &&
                           y >= modal_reset_roles_rect_.Y && y <= modal_reset_roles_rect_.Y + modal_reset_roles_rect_.Height) {
                    hover_btn_reset_roles_ = true;
                }

                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Window Controls hover in top-right
            if (y >= 0 && y < 34) {
                if (x >= rc.right - 138 && x < rc.right - 92) hover_win_min_ = true;
                else if (x >= rc.right - 92 && x < rc.right - 46) hover_win_max_ = true;
                else if (x >= rc.right - 46 && x <= rc.right) hover_win_close_ = true;
            }

            // Left Dock hover (centered vertically)
            if (x < 48) {
                float startY = get_dock_start_y((float)rc.bottom);
                float curY = startY;
                for (int i = 0; i < 6; ++i) {
                    if (y >= curY && y <= curY + 36.0f) { hover_dock_icon_ = i; break; }
                    curY += 46.0f;
                }
                float setY = get_dock_icon_y(6, (float)rc.bottom);
                float togY = get_dock_icon_y(7, (float)rc.bottom);
                if (y >= setY && y <= setY + 36.0f) hover_dock_icon_ = 6;
                else if (y >= togY && y <= togY + 36.0f) hover_dock_icon_ = 7;
            }

            // Top Bar hover
            if (y >= 10 && y <= 40) {
                if (x >= 60 && x <= 156) hover_btn_session_ = true;
                else if (current_tab_ == ActiveTab::CHAT && x >= chat_right - 74 && x <= chat_right - 14) hover_btn_new_ = true;
                else if (current_tab_ == ActiveTab::ARENA) {
                    if (x >= arena_toggle_btn_rect_.X && x <= arena_toggle_btn_rect_.X + arena_toggle_btn_rect_.Width) {
                        hover_btn_arena_toggle_ = true;
                    } else if (x >= arena_topic_btn_rect_.X && x <= arena_topic_btn_rect_.X + arena_topic_btn_rect_.Width) {
                        hover_btn_arena_topic_ = true;
                    } else if (x >= arena_export_btn_rect_.X && x <= arena_export_btn_rect_.X + arena_export_btn_rect_.Width) {
                        hover_btn_arena_export_ = true;
                    } else if (x >= arena_save_btn_rect_.X && x <= arena_save_btn_rect_.X + arena_save_btn_rect_.Width) {
                        hover_btn_arena_save_ = true;
                    } else if (x >= arena_sessions_btn_rect_.X && x <= arena_sessions_btn_rect_.X + arena_sessions_btn_rect_.Width) {
                        hover_btn_arena_sessions_ = true;
                    }
                }
            }

            // Floating Muse badge hover (when right panel is closed)
            if (right_panel_cur_w_ < 30.0f) {
                float flX = (float)chat_left + ((float)chat_w - 48.0f) / 2.0f;
                if (x >= flX - 4 && x <= flX + 52 && y >= 8 && y <= 70) {
                    hover_floating_muse_ = true;
                }
            }

            // Chat Bottom Capsule hover
            if (current_tab_ == ActiveTab::CHAT) {
                float capsuleW = (float)min(chat_w - 80, 620);
                float capsuleX = (float)chat_left + ((float)chat_w - capsuleW) / 2.0f;
                float capsuleY = (float)(rc.bottom - 62);
                if (x >= capsuleX + 8.0f && x <= capsuleX + 36.0f && y >= capsuleY + 8.0f && y <= capsuleY + 36.0f) {
                    hover_btn_plus_ = true;
                } else if (x >= capsuleX + capsuleW - 38.0f && x <= capsuleX + capsuleW - 6.0f && y >= capsuleY + 8.0f && y <= capsuleY + 36.0f) {
                    hover_btn_send_ = true;
                }
            }

            // Arena Bottom Bar and Topic Card buttons hover
            if (current_tab_ == ActiveTab::ARENA) {
                if (x >= arena_interject_btn_rect_.X && x <= arena_interject_btn_rect_.X + arena_interject_btn_rect_.Width &&
                    y >= arena_interject_btn_rect_.Y && y <= arena_interject_btn_rect_.Y + arena_interject_btn_rect_.Height) {
                    hover_btn_arena_interject_ = true;
                }
                if (arena_context_btn_rect_.Width > 0 &&
                    x >= arena_context_btn_rect_.X && x <= arena_context_btn_rect_.X + arena_context_btn_rect_.Width &&
                    y >= arena_context_btn_rect_.Y && y <= arena_context_btn_rect_.Y + arena_context_btn_rect_.Height) {
                    hover_btn_context_ = true;
                }
                if (arena_speed_btn_rect_.Width > 0 &&
                    x >= arena_speed_btn_rect_.X && x <= arena_speed_btn_rect_.X + arena_speed_btn_rect_.Width &&
                    y >= arena_speed_btn_rect_.Y && y <= arena_speed_btn_rect_.Y + arena_speed_btn_rect_.Height) {
                    hover_btn_speed_ = true;
                }
                if (arena_reset_roles_rect_.Width > 0 &&
                    x >= arena_reset_roles_rect_.X && x <= arena_reset_roles_rect_.X + arena_reset_roles_rect_.Width &&
                    y >= arena_reset_roles_rect_.Y && y <= arena_reset_roles_rect_.Y + arena_reset_roles_rect_.Height) {
                    hover_btn_reset_roles_ = true;
                }
                if (arena_swarm_plus_rect_.Width > 0 &&
                    x >= arena_swarm_plus_rect_.X && x <= arena_swarm_plus_rect_.X + arena_swarm_plus_rect_.Width &&
                    y >= arena_swarm_plus_rect_.Y && y <= arena_swarm_plus_rect_.Y + arena_swarm_plus_rect_.Height) {
                    hover_btn_swarm_plus_ = true;
                }
                if (arena_swarm_minus_rect_.Width > 0 &&
                    x >= arena_swarm_minus_rect_.X && x <= arena_swarm_minus_rect_.X + arena_swarm_minus_rect_.Width &&
                    y >= arena_swarm_minus_rect_.Y && y <= arena_swarm_minus_rect_.Y + arena_swarm_minus_rect_.Height) {
                    hover_btn_swarm_minus_ = true;
                }
            }

            if (current_tab_ == ActiveTab::SETTINGS) {
                if (settings_view_.on_mouse_move((float)x, (float)y)) {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }

            // Right Panel hover
            if (right_panel_cur_w_ > 50.0f && x >= rc.right - (int)right_panel_cur_w_) {
                float pX = (float)(rc.right - (int)right_panel_cur_w_);
                if (x >= (int)pX + 14 && x <= (int)pX + 44 && y >= 10 && y <= 40) {
                    hover_btn_drawer_collapse_ = true;
                }
                if (y >= 192 && y <= 226) {
                    float segW = (310.0f - 44.0f) / 4.0f;
                    for (int t = 0; t < 4; ++t) {
                        float sx = pX + 22.0f + (t * segW);
                        if (x >= sx && x <= sx + segW) { hover_right_tab_ = t; break; }
                    }
                }
            }

            if (prev_dock != hover_dock_icon_ || prev_rtab != hover_right_tab_ ||
                prev_new != hover_btn_new_ || prev_session != hover_btn_session_ ||
                prev_plus != hover_btn_plus_ || prev_send != hover_btn_send_ ||
                prev_close != hover_btn_close_ || prev_fl != hover_floating_muse_ ||
                prev_arena_btn != hover_btn_arena_interject_ ||
                prev_arena_tog != hover_btn_arena_toggle_ ||
                prev_arena_top != hover_btn_arena_topic_ ||
                prev_arena_exp != hover_btn_arena_export_ ||
                prev_arena_save != hover_btn_arena_save_ ||
                prev_arena_sessions != hover_btn_arena_sessions_ ||
                prev_arena_ctx != hover_btn_context_ ||
                prev_win_min != hover_win_min_ ||
                prev_win_max != hover_win_max_ ||
                prev_win_close != hover_win_close_ ||
                prev_drw_collapse != hover_btn_drawer_collapse_) {
                InvalidateRect(hwnd, NULL, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            hover_dock_icon_ = -1;
            hover_right_tab_ = -1;
            hover_btn_new_ = false;
            hover_btn_session_ = false;
            hover_btn_plus_ = false;
            hover_btn_send_ = false;
            hover_btn_close_ = false;
            hover_btn_drawer_collapse_ = false;
            hover_win_min_ = false;
            hover_win_max_ = false;
            hover_win_close_ = false;
            hover_floating_muse_ = false;
            hover_btn_arena_interject_ = false;
            hover_btn_arena_toggle_ = false;
            hover_btn_arena_topic_ = false;
            hover_btn_arena_export_ = false;
            hover_btn_arena_save_ = false;
            hover_btn_arena_sessions_ = false;
            hover_btn_swarm_minus_ = false;
            hover_btn_swarm_plus_ = false;
            hover_btn_reset_roles_ = false;
            hover_btn_speed_ = false;
            hover_btn_context_ = false;
            hover_modal_apply_ = false;
            hover_modal_cancel_ = false;
            hover_modal_preset_ = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);

            if (session_modal_.is_visible()) {
                int selected_idx = -1;
                views::SessionModalAction act = session_modal_.handle_lbutton_down(x, y, selected_idx);
                if (act == views::SessionModalAction::CLOSE) {
                    session_modal_.set_visible(false);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                } else if (act == views::SessionModalAction::LOAD_SESSION) {
                    on_arena_load_session(selected_idx);
                    session_modal_.set_visible(false);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                } else if (act == views::SessionModalAction::DELETE_SESSION) {
                    on_arena_delete_session(selected_idx);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                } else if (act == views::SessionModalAction::SAVE_NEW) {
                    on_arena_save_session();
                    session_modal_.set_visible(false);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                // Dismiss if clicked outside the card
                const auto& card = session_modal_.get_card_rect();
                if (x < card.X || x > card.X + card.Width || y < card.Y || y > card.Y + card.Height) {
                    session_modal_.set_visible(false);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            // 0. Window Controls (Minimize, Maximize / Restore, Close)
            if (y >= 0 && y < 34) {
                if (x >= rc.right - 138 && x < rc.right - 92) {
                    ShowWindow(hwnd, SW_MINIMIZE);
                    return 0;
                }
                if (x >= rc.right - 92 && x < rc.right - 46) {
                    if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
                    else ShowWindow(hwnd, SW_MAXIMIZE);
                    return 0;
                }
                if (x >= rc.right - 46 && x <= rc.right) {
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    return 0;
                }
            }

            if (show_topic_modal_) {
                // 1. Apply button
                if (x >= modal_btn_apply_rect_.X && x <= modal_btn_apply_rect_.X + modal_btn_apply_rect_.Width &&
                    y >= modal_btn_apply_rect_.Y && y <= modal_btn_apply_rect_.Y + modal_btn_apply_rect_.Height) {
                    wchar_t buf[2048] = { 0 };
                    if (h_arena_topic_input_) {
                        GetWindowTextW(h_arena_topic_input_, buf, 2048);
                    }
                    std::string newTopic = ws2s(buf);
                    while (!newTopic.empty() && (newTopic.front() == ' ' || newTopic.front() == '\r' || newTopic.front() == '\n')) newTopic.erase(0, 1);
                    while (!newTopic.empty() && (newTopic.back() == ' ' || newTopic.back() == '\r' || newTopic.back() == '\n')) newTopic.pop_back();
                    if (!newTopic.empty()) {
                        apply_new_topic(newTopic);
                    } else {
                        show_topic_modal_ = false;
                        update_control_visibility();
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    return 0;
                }

                // 2. Cancel button
                if (x >= modal_btn_cancel_rect_.X && x <= modal_btn_cancel_rect_.X + modal_btn_cancel_rect_.Width &&
                    y >= modal_btn_cancel_rect_.Y && y <= modal_btn_cancel_rect_.Y + modal_btn_cancel_rect_.Height) {
                    show_topic_modal_ = false;
                    update_control_visibility();
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                // 3. Preset pills
                const std::string PRESET_TOPICS[4] = {
                    "Cơ chế thoát Sandbox: Win32 Job Object, Low-Integrity Token & Air-Gap",
                    "Phương pháp học nhanh và tư duy phản biện cho kỹ sư phần mềm",
                    "So sánh kiến trúc Native C++ Win32/Direct2D với Electron/Web trên máy bàn",
                    "Mô hình Swarm AI tự quản: Tự phân vai và tương tác đa tác tử độc lập"
                };
                for (int i = 0; i < 4; ++i) {
                    if (x >= modal_preset_rects_[i].X && x <= modal_preset_rects_[i].X + modal_preset_rects_[i].Width &&
                        y >= modal_preset_rects_[i].Y && y <= modal_preset_rects_[i].Y + modal_preset_rects_[i].Height) {
                        if (h_arena_topic_input_) {
                            SetWindowTextW(h_arena_topic_input_, s2ws(PRESET_TOPICS[i]).c_str());
                            SetFocus(h_arena_topic_input_);
                            SendMessageW(h_arena_topic_input_, EM_SETSEL, 0, -1);
                        }
                        InvalidateRect(hwnd, NULL, FALSE);
                        return 0;
                    }
                }

                // 4. Modal Swarm count controls
                if (x >= modal_swarm_minus_rect_.X && x <= modal_swarm_minus_rect_.X + modal_swarm_minus_rect_.Width &&
                    y >= modal_swarm_minus_rect_.Y && y <= modal_swarm_minus_rect_.Y + modal_swarm_minus_rect_.Height) {
                    remove_swarm_agent();
                    return 0;
                }
                if (x >= modal_swarm_plus_rect_.X && x <= modal_swarm_plus_rect_.X + modal_swarm_plus_rect_.Width &&
                    y >= modal_swarm_plus_rect_.Y && y <= modal_swarm_plus_rect_.Y + modal_swarm_plus_rect_.Height) {
                    add_swarm_agent();
                    return 0;
                }
                if (x >= modal_reset_roles_rect_.X && x <= modal_reset_roles_rect_.X + modal_reset_roles_rect_.Width &&
                    y >= modal_reset_roles_rect_.Y && y <= modal_reset_roles_rect_.Y + modal_reset_roles_rect_.Height) {
                    reset_swarm_roles();
                    return 0;
                }

                // Clicking outside modal card dismisses it
                if (x < modal_card_rect_.X || x > modal_card_rect_.X + modal_card_rect_.Width ||
                    y < modal_card_rect_.Y || y > modal_card_rect_.Y + modal_card_rect_.Height) {
                    show_topic_modal_ = false;
                    update_control_visibility();
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                return 0; // Consume click inside modal body
            }

            int chat_left = 48;
            int right_panel_w = (int)right_panel_cur_w_;
            float top_bar_right = (right_panel_cur_w_ > 146.0f) ? (float)(rc.right - (int)right_panel_cur_w_) : ((float)rc.right - 146.0f);
            int chat_right = (int)top_bar_right;
            int chat_w = (rc.right - right_panel_w) - chat_left;

            // 1. Left Dock clicks (centered vertically)
            if (x < 48) {
                float startY = get_dock_start_y((float)rc.bottom);
                float curY = startY;
                for (int i = 0; i < 6; ++i) {
                    if (y >= curY && y <= curY + 36.0f) {
                        active_dock_icon_ = i;
                        if (i == 0) switch_tab(ActiveTab::CHAT);
                        else if (i == 2) switch_tab(ActiveTab::WORKSPACE);
                        else if (i == 5) switch_tab(ActiveTab::ARENA);
                        else {
                            dock_indicator_target_y_ = get_dock_icon_y(active_dock_icon_, (float)rc.bottom);
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                        return 0;
                    }
                    curY += 46.0f;
                }
                float setY = get_dock_icon_y(6, (float)rc.bottom);
                float togY = get_dock_icon_y(7, (float)rc.bottom);
                if (y >= setY && y <= setY + 36.0f) {
                    switch_tab(ActiveTab::SETTINGS);
                    return 0;
                }
                if (y >= togY && y <= togY + 36.0f) {
                    show_right_panel_ = !show_right_panel_;
                    right_panel_target_w_ = show_right_panel_ ? 310.0f : 0.0f;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                return 0;
            }

            // 2. Top Bar clicks
            if (y >= 10 && y <= 40) {
                if (x >= 60 && x <= 156) {
                    switch_tab(ActiveTab::CHAT);
                    return 0;
                }
                if (current_tab_ == ActiveTab::CHAT && x >= chat_right - 74 && x <= chat_right - 14) {
                    on_reset_chat();
                    return 0;
                }
                // Interactive Security Badge click (Air-Gap Zero-Trust <-> Online Web Bridge)
                if (security_badge_rect_.Width > 0.0f &&
                    x >= security_badge_rect_.X && x <= security_badge_rect_.X + security_badge_rect_.Width) {
                    on_airgap_toggle();
                    return 0;
                }
                // Interactive SEB Anti-Screenshot Badge click
                if (anti_capture_badge_rect_.Width > 0.0f &&
                    x >= anti_capture_badge_rect_.X && x <= anti_capture_badge_rect_.X + anti_capture_badge_rect_.Width) {
                    on_anti_capture_toggle();
                    return 0;
                }
            }

            // 3. Floating Muse Avatar click (re-opens Right Panel!)
            if (right_panel_cur_w_ < 30.0f) {
                float flX = (float)chat_left + ((float)chat_w - 48.0f) / 2.0f;
                if (x >= flX - 4 && x <= flX + 52 && y >= 8 && y <= 70) {
                    show_right_panel_ = true;
                    right_panel_target_w_ = 310.0f;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Reasoning Box Collapse / Expand clicks
            if (current_tab_ == ActiveTab::CHAT) {
                for (const auto& rh : reasoning_hitboxes_) {
                    if (x >= rh.rect.X && x <= rh.rect.X + rh.rect.Width &&
                        y >= rh.rect.Y && y <= rh.rect.Y + rh.rect.Height) {
                        if (rh.msg_idx < chat_messages_.size()) {
                            chat_messages_[rh.msg_idx].reasoning_collapsed = !chat_messages_[rh.msg_idx].reasoning_collapsed;
                            chat_messages_[rh.msg_idx].cached_card_h = 0.0f;
                            InvalidateRect(hwnd, NULL, FALSE);
                            return 0;
                        }
                    }
                }
            }

            // 4. Code Block Copy / Download button clicks
            if (current_tab_ == ActiveTab::CHAT) {
                for (const auto& hb : code_hitboxes_) {
                    if (x >= hb.copy_rect.X && x <= hb.copy_rect.X + hb.copy_rect.Width &&
                        y >= hb.copy_rect.Y && y <= hb.copy_rect.Y + hb.copy_rect.Height) {
                        copy_to_clipboard(hb.code);
                        copied_block_id_ = hb.block_id;
                        downloaded_block_id_ = -1;
                        log_console("[CLIPBOARD]", "Đã sao chép khối mã (" + hb.lang + ") vào Clipboard!", 10);
                        InvalidateRect(hwnd, NULL, FALSE);
                        return 0;
                    }
                    if (x >= hb.download_rect.X && x <= hb.download_rect.X + hb.download_rect.Width &&
                        y >= hb.download_rect.Y && y <= hb.download_rect.Y + hb.download_rect.Height) {
                        std::string fname = "snippet." + hb.lang;
                        sandbox_mgr_->get_engine()->write_file(fname, hb.code);
                        downloaded_block_id_ = hb.block_id;
                        copied_block_id_ = -1;
                        log_console("[DOWNLOAD]", "Đã lưu khối mã vào sandbox_workspace/" + fname, 11);
                        InvalidateRect(hwnd, NULL, FALSE);
                        return 0;
                    }
                }

                // Bottom Capsule clicks
                float capsuleW = (float)min(chat_w - 80, 620);
                float capsuleX = (float)chat_left + ((float)chat_w - capsuleW) / 2.0f;
                float capsuleY = (float)(rc.bottom - 62);
                if (x >= capsuleX + 8.0f && x <= capsuleX + 36.0f && y >= capsuleY + 8.0f && y <= capsuleY + 36.0f) {
                    switch_tab(ActiveTab::WORKSPACE);
                    return 0;
                }
                if (x >= capsuleX + capsuleW - 38.0f && x <= capsuleX + capsuleW - 6.0f && y >= capsuleY + 8.0f && y <= capsuleY + 36.0f) {
                    if (is_chat_generating_) {
                        on_stop_task();
                    } else {
                        on_send_chat();
                    }
                    return 0;
                }
            }

            // 4b. Arena Top Bar, Topic Card and Bottom Capsule clicks
            if (current_tab_ == ActiveTab::ARENA) {
                if (y >= 10 && y <= 40) {
                    if (x >= arena_toggle_btn_rect_.X && x <= arena_toggle_btn_rect_.X + arena_toggle_btn_rect_.Width) {
                        on_arena_toggle();
                        return 0;
                    }
                    if (x >= arena_topic_btn_rect_.X && x <= arena_topic_btn_rect_.X + arena_topic_btn_rect_.Width) {
                        on_arena_change_topic();
                        return 0;
                    }
                    if (x >= arena_export_btn_rect_.X && x <= arena_export_btn_rect_.X + arena_export_btn_rect_.Width) {
                        on_arena_export();
                        return 0;
                    }
                    if (x >= arena_save_btn_rect_.X && x <= arena_save_btn_rect_.X + arena_save_btn_rect_.Width) {
                        on_arena_save_session();
                        return 0;
                    }
                    if (x >= arena_sessions_btn_rect_.X && x <= arena_sessions_btn_rect_.X + arena_sessions_btn_rect_.Width) {
                        on_arena_open_sessions();
                        return 0;
                    }
                }
                // Arena Topic Card interactive controls:
                if (arena_context_btn_rect_.Width > 0 &&
                    x >= arena_context_btn_rect_.X && x <= arena_context_btn_rect_.X + arena_context_btn_rect_.Width &&
                    y >= arena_context_btn_rect_.Y && y <= arena_context_btn_rect_.Y + arena_context_btn_rect_.Height) {
                    cycle_arena_context_window();
                    return 0;
                }
                if (arena_speed_btn_rect_.Width > 0 &&
                    x >= arena_speed_btn_rect_.X && x <= arena_speed_btn_rect_.X + arena_speed_btn_rect_.Width &&
                    y >= arena_speed_btn_rect_.Y && y <= arena_speed_btn_rect_.Y + arena_speed_btn_rect_.Height) {
                    cycle_arena_speed();
                    return 0;
                }
                if (arena_reset_roles_rect_.Width > 0 &&
                    x >= arena_reset_roles_rect_.X && x <= arena_reset_roles_rect_.X + arena_reset_roles_rect_.Width &&
                    y >= arena_reset_roles_rect_.Y && y <= arena_reset_roles_rect_.Y + arena_reset_roles_rect_.Height) {
                    reset_swarm_roles();
                    return 0;
                }
                if (arena_swarm_plus_rect_.Width > 0 &&
                    x >= arena_swarm_plus_rect_.X && x <= arena_swarm_plus_rect_.X + arena_swarm_plus_rect_.Width &&
                    y >= arena_swarm_plus_rect_.Y && y <= arena_swarm_plus_rect_.Y + arena_swarm_plus_rect_.Height) {
                    add_swarm_agent();
                    return 0;
                }
                if (arena_swarm_minus_rect_.Width > 0 &&
                    x >= arena_swarm_minus_rect_.X && x <= arena_swarm_minus_rect_.X + arena_swarm_minus_rect_.Width &&
                    y >= arena_swarm_minus_rect_.Y && y <= arena_swarm_minus_rect_.Y + arena_swarm_minus_rect_.Height) {
                    remove_swarm_agent();
                    return 0;
                }
                // Clicking on topic text area on topic card opens change topic modal
                if (y >= 52 && y <= 126 && x >= chat_left + 20 && x <= chat_right - 350) {
                    on_arena_change_topic();
                    return 0;
                }
                if (x >= arena_interject_btn_rect_.X && x <= arena_interject_btn_rect_.X + arena_interject_btn_rect_.Width &&
                    y >= arena_interject_btn_rect_.Y && y <= arena_interject_btn_rect_.Y + arena_interject_btn_rect_.Height) {
                    on_arena_interject();
                    return 0;
                }
            }

            // 4c. Settings Action Buttons
            if (current_tab_ == ActiveTab::SETTINGS) {
                views::SettingsAction act = settings_view_.on_mouse_down((float)x, (float)y);
                if (act == views::SettingsAction::TEST_AI) {
                    on_settings_test();
                    return 0;
                } else if (act == views::SettingsAction::SAVE_CONFIG) {
                    on_settings_save();
                    return 0;
                } else if (act == views::SettingsAction::RESET_WORKSPACE) {
                    sandbox_mgr_->get_engine()->reset();
                    init_today_tasks();
                    InvalidateRect(hwnd, NULL, FALSE);
                    MessageBoxW(hwnd, L"✓ Đã làm mới workspace sandbox!", L"Thông báo", MB_OK);
                    return 0;
                }
            }

            // 5. Right Panel clicks
            if (right_panel_cur_w_ > 50.0f && x >= rc.right - (int)right_panel_cur_w_) {
                float pX = (float)(rc.right - (int)right_panel_cur_w_);
                // Drawer collapse button
                if (x >= (int)pX + 14 && x <= (int)pX + 44 && y >= 10 && y <= 40) {
                    show_right_panel_ = false;
                    right_panel_target_w_ = 0.0f;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                // 4 Segmented Tabs
                if (y >= 192 && y <= 226) {
                    float segW = (310.0f - 44.0f) / 4.0f;
                    for (int t = 0; t < 4; ++t) {
                        float sx = pX + 22.0f + (t * segW);
                        if (x >= sx && x <= sx + segW) {
                            right_panel_tab_ = t;
                            right_tab_target_x_ = sx;
                            InvalidateRect(hwnd, NULL, FALSE);
                            break;
                        }
                    }
                    return 0;
                }
                // Interactive buttons inside Right Panel Tabs
                if (right_panel_tab_ == 1 && y >= 490 && y <= 526 && x >= pX + 18 && x <= pX + 292) {
                    on_airgap_toggle();
                    return 0;
                }
                if (right_panel_tab_ == 1 && y >= 630 && y <= 662 && x >= pX + 26 && x <= pX + 284) {
                    on_anti_capture_toggle();
                    return 0;
                }
                if (right_panel_tab_ == 3 && y >= 378 && y <= 414 && x >= pX + 18 && x <= pX + 292) {
                    switch_tab(ActiveTab::ARENA);
                    return 0;
                }
            }

            break;
        }

        case WM_MOUSEWHEEL: {
            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            ScreenToClient(hwnd, &pt);

            RECT rc;
            GetClientRect(hwnd, &rc);
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);

            // If cursor is over the Right Panel, scroll the Right Panel task list!
            if (show_right_panel_ && pt.x >= rc.right - 310) {
                right_panel_scroll_y_ -= (delta / 120) * 35;
                if (right_panel_scroll_y_ < 0) right_panel_scroll_y_ = 0;
                if (right_panel_scroll_y_ > 240) right_panel_scroll_y_ = 240;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            if (current_tab_ == ActiveTab::CHAT) {
                int visibleChatH = max(100, (int)(rc.bottom - 46 - 78));
                int maxScroll = max(0, chat_content_height_ - visibleChatH);
                target_chat_scroll_y_ -= (delta / 120) * 75;
                if (target_chat_scroll_y_ < 0) target_chat_scroll_y_ = 0;
                if (target_chat_scroll_y_ > maxScroll) target_chat_scroll_y_ = maxScroll;
                // Pause auto-scroll if user scrolls up; resume if scrolled near bottom
                auto_scroll_chat_ = (target_chat_scroll_y_ >= maxScroll - 30);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (current_tab_ == ActiveTab::ARENA) {
                int visibleArenaH = max(100, (int)(rc.bottom - 132 - 72));
                int maxScroll = max(0, arena_content_height_ - visibleArenaH);
                target_arena_scroll_y_ -= (delta / 120) * 75;
                if (target_arena_scroll_y_ < 0) target_arena_scroll_y_ = 0;
                if (target_arena_scroll_y_ > maxScroll) target_arena_scroll_y_ = maxScroll;
                auto_scroll_arena_ = (target_arena_scroll_y_ >= maxScroll - 30);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == ID_TIMER_ANIM) {
                anim_frame_++;

                // 120 FPS High-precision delta time (dt in seconds)
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                double dt = 0.008; // default ~8.33ms (120 FPS)
                if (perf_freq_.QuadPart > 0 && last_anim_time_.QuadPart > 0) {
                    dt = (double)(now.QuadPart - last_anim_time_.QuadPart) / (double)perf_freq_.QuadPart;
                }
                last_anim_time_ = now;

                // Safety clamp dt to avoid sudden spikes during window move or OS stall
                if (dt < 0.001) dt = 0.001;
                if (dt > 0.050) dt = 0.050;

                // Framerate-independent exponential decay factors: 1 - exp(-lambda * dt)
                // lambda = 14.0 gives smooth spring easing identical across 60Hz, 120Hz, 144Hz, 240Hz
                float decay14 = 1.0f - std::exp(-14.0f * (float)dt);
                float decay12 = 1.0f - std::exp(-12.0f * (float)dt);
                float decay16 = 1.0f - std::exp(-16.0f * (float)dt);

                pulse_phase_ += (float)(dt * 4.5f);
                if (pulse_phase_ > 628.0f) pulse_phase_ -= 628.0f;

                bool needRepaint = is_chat_generating_.load() || arena_running_.load();

                // 1. Smooth Gliding Left Dock Indicator Pill (120 FPS Framerate-independent easing)
                if (std::abs(dock_indicator_target_y_ - dock_indicator_y_) > 0.15f) {
                    dock_indicator_y_ += (dock_indicator_target_y_ - dock_indicator_y_) * decay14;
                    needRepaint = true;
                } else if (dock_indicator_y_ != dock_indicator_target_y_) {
                    dock_indicator_y_ = dock_indicator_target_y_;
                    needRepaint = true;
                }

                // 2. Smooth Sliding Right Panel (Companion Drawer)
                if (std::abs(right_panel_target_w_ - right_panel_cur_w_) > 0.25f) {
                    right_panel_cur_w_ += (right_panel_target_w_ - right_panel_cur_w_) * decay12;
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    layout_controls(rc.right, rc.bottom);
                    needRepaint = true;
                } else if (right_panel_cur_w_ != right_panel_target_w_) {
                    right_panel_cur_w_ = right_panel_target_w_;
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    layout_controls(rc.right, rc.bottom);
                    needRepaint = true;
                }

                // 3. Smooth Gliding Right Panel Tabs Indicator
                if (std::abs(right_tab_target_x_ - right_tab_indicator_x_) > 0.15f) {
                    right_tab_indicator_x_ += (right_tab_target_x_ - right_tab_indicator_x_) * decay14;
                    needRepaint = true;
                } else if (right_tab_indicator_x_ != right_tab_target_x_) {
                    right_tab_indicator_x_ = right_tab_target_x_;
                    needRepaint = true;
                }

                // 4. Tab Transition Progress
                if (tab_transition_progress_ < 1.0f) {
                    tab_transition_progress_ += (float)(dt / 0.20f);
                    if (tab_transition_progress_ >= 1.0f) {
                        tab_transition_progress_ = 1.0f;
                    }
                    needRepaint = true;
                }

                // 5. 120 FPS Sub-Pixel Smooth Scroll for Chat View
                if (std::abs((float)target_chat_scroll_y_ - chat_scroll_float_) > 0.15f) {
                    chat_scroll_float_ += ((float)target_chat_scroll_y_ - chat_scroll_float_) * decay16;
                    int newScroll = (int)std::round(chat_scroll_float_);
                    if (newScroll != chat_scroll_y_) {
                        chat_scroll_y_ = newScroll;
                        needRepaint = true;
                    }
                } else if (chat_scroll_y_ != target_chat_scroll_y_) {
                    chat_scroll_float_ = (float)target_chat_scroll_y_;
                    chat_scroll_y_ = target_chat_scroll_y_;
                    needRepaint = true;
                }

                // 6. 120 FPS Sub-Pixel Smooth Scroll for Arena View
                if (std::abs((float)target_arena_scroll_y_ - arena_scroll_float_) > 0.15f) {
                    arena_scroll_float_ += ((float)target_arena_scroll_y_ - arena_scroll_float_) * decay16;
                    int newScroll = (int)std::round(arena_scroll_float_);
                    if (newScroll != arena_scroll_y_) {
                        arena_scroll_y_ = newScroll;
                        needRepaint = true;
                    }
                } else if (arena_scroll_y_ != target_arena_scroll_y_) {
                    arena_scroll_float_ = (float)target_arena_scroll_y_;
                    arena_scroll_y_ = target_arena_scroll_y_;
                    needRepaint = true;
                }

                if (needRepaint) {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORLISTBOX: {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndCtrl = (HWND)lParam;
            if (hwndCtrl == h_arena_interject_input_) {
                SetTextColor(hdcStatic, RGB(241, 245, 249));
                SetBkColor(hdcStatic, RGB(28, 29, 34)); // #1c1d22 matches arena capsule perfectly
                return (LRESULT)card_brush_;
            }
            if (hwndCtrl == h_arena_topic_input_) {
                SetTextColor(hdcStatic, RGB(241, 245, 249));
                SetBkColor(hdcStatic, RGB(18, 19, 24)); // matches modal topic input box
                return (LRESULT)card_brush_;
            }
            SetTextColor(hdcStatic, RGB(241, 245, 249));
            SetBkColor(hdcStatic, RGB(37, 38, 43)); // #25262b
            return (LRESULT)input_brush_;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_CHAT_INPUT && wmEvent == EN_CHANGE) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else if (wmId == IDC_BTN_ARENA_TOGGLE) on_arena_toggle();
            else if (wmId == IDC_BTN_ARENA_TOPIC) on_arena_change_topic();
            else if (wmId == IDC_BTN_ARENA_EXPORT) on_arena_export();
            else if (wmId == IDC_BTN_ARENA_INTERJECT) on_arena_interject();

            else if (wmId == IDC_CFG_PROVIDER && wmEvent == CBN_SELCHANGE) on_settings_provider_changed();
            else if (wmId == IDC_BTN_CFG_TEST) on_settings_test();
            else if (wmId == IDC_BTN_CFG_SAVE) on_settings_save();
            else if (wmId == IDC_BTN_CFG_RESET_WS) {
                sandbox_mgr_->get_engine()->reset();
                init_today_tasks();
                InvalidateRect(hwnd, NULL, FALSE);
                MessageBoxW(hwnd, L"✓ Đã làm mới workspace sandbox!", L"Thông báo", MB_OK);
            }

            else if (wmId == IDC_BTN_WS_REFRESH) on_ws_refresh();
            else if (wmId == IDC_BTN_WS_OPEN_FOLDER) on_ws_open_folder();
            else if (wmId == IDC_BTN_WS_SAVE_FILE) on_ws_save_file();
            else if (wmId == IDC_WS_LISTBOX && wmEvent == LBN_SELCHANGE) on_ws_file_select();
            else if (wmId == IDC_BTN_WS_CMD_RUN) on_ws_run_cmd();

            return 0;
        }

        case WM_APP_CHAT_CHUNK:
        case WM_APP_CHAT_DONE:
        case WM_APP_ARENA_UPDATE: {
            chunk_msg_queued_ = false;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_CLIPBOARDUPDATE: {
            security::AntiScreenshot::instance().handle_clipboard_update();
            return 0;
        }

        case WM_DISPLAYCHANGE: {
            security::AntiScreenshot::instance().handle_display_change();
            break;
        }

        case WM_ACTIVATE: {
            bool hasFocus = (LOWORD(wParam) != WA_INACTIVE);
            security::AntiScreenshot::instance().handle_focus_change(hasFocus);
            break;
        }

        case WM_DESTROY: {
            security::AntiScreenshot::instance().shutdown();
            KillTimer(hwnd, ID_TIMER_ANIM);
            timeEndPeriod(1);
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int NativeGui::run_message_loop() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            HWND focus = GetFocus();
            if (focus == h_chat_input_) {
                on_send_chat();
                continue;
            } else if (focus == h_arena_interject_input_) {
                on_arena_interject();
                continue;
            } else if (focus == h_ws_cmd_input_) {
                on_ws_run_cmd();
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

} // namespace gui
