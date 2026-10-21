#pragma once
#include <string>
#include <vector>
#include <android_native_app_glue.h>
#include "android_native_sandbox.hpp"

namespace cyberbox {
namespace android {

struct AndroidAppSettings {
    std::string api_endpoint = "http://10.0.2.2:11434/api/generate"; // Ollama on host emulator or LAN IP
    std::string model_name = "qwen2.5-coder:7b";
    std::string workspace_dir = ""; // Empty = default to app internal files dir
    int context_window = 3;
    bool enable_sandbox = true;
};

class AndroidAppEngine {
private:
    struct android_app* app_ = nullptr;
    AndroidAppSettings settings_;
    sandbox::AndroidNativeSandbox sandbox_;
    bool initialized_ = false;

    // UI state
    int current_tab_ = 0; // 0: Chat, 1: Arena, 2: Workspace Files, 3: Settings
    std::vector<std::string> chat_history_;
    char input_buf_[1024] = {0};
    char custom_path_buf_[512] = {0};

public:
    AndroidAppEngine(struct android_app* app);
    ~AndroidAppEngine();

    bool init();
    void render_frame();
    void process_input();
    void cleanup();

    void set_flexible_workspace(const std::string& path);
};

} // namespace android
} // namespace cyberbox
