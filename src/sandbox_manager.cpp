#include "sandbox_manager.hpp"
#include <iostream>

namespace sandbox {

SandboxManager::SandboxManager() : preferred_engine_type_("auto") {}

SandboxManager::~SandboxManager() {
    if (current_engine_) {
        current_engine_->shutdown();
    }
}

bool SandboxManager::initialize(const std::string& preferred_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    preferred_engine_type_ = preferred_type;

    if (preferred_type == "docker" || preferred_type == "auto") {
        auto docker = std::make_unique<DockerSandbox>();
        if (docker->initialize()) {
            current_engine_ = std::move(docker);
            std::cout << "[SandboxManager] Active Engine: Docker Container (True Linux OS Sandbox)\n";
            return true;
        }
        std::cout << "[SandboxManager] Docker not reachable, falling back to Native Win32 Jail Sandbox...\n";
    }

    // Fallback or explicit Win32 Jail
    auto win32_jail = std::make_unique<Win32JailSandbox>("sandbox_workspace");
    if (win32_jail->initialize()) {
        current_engine_ = std::move(win32_jail);
        std::cout << "[SandboxManager] Active Engine: Native Win32 JobObject Jail Sandbox\n";
        return true;
    }

    std::cerr << "[SandboxManager] Failed to initialize any sandbox engine!\n";
    return false;
}

bool SandboxManager::switch_engine(const std::string& engine_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // If current engine is already healthy and matches, keep it
    if (current_engine_ && current_engine_->get_status().is_running) {
        std::string cur_type = current_engine_->get_status().engine_type;
        if (engine_type == "docker" && cur_type == "Docker") return true;
        if (engine_type == "win32" && cur_type == "Win32_Jail") return true;
        if (engine_type == "auto") return true;
    }

    if (current_engine_) {
        current_engine_->shutdown();
        current_engine_.reset();
    }

    preferred_engine_type_ = engine_type;

    if (engine_type == "docker" || engine_type == "auto") {
        auto docker = std::make_unique<DockerSandbox>();
        if (docker->initialize()) {
            current_engine_ = std::move(docker);
            std::cout << "[SandboxManager] Switched to Docker Engine\n";
            return true;
        }
    }

    // Default / Win32 Jail fallback
    auto win32 = std::make_unique<Win32JailSandbox>("sandbox_workspace");
    if (win32->initialize()) {
        current_engine_ = std::move(win32);
        std::cout << "[SandboxManager] Switched to Win32 Jail Sandbox\n";
        return true;
    }

    return false;
}

ISandboxEngine* SandboxManager::get_engine() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_engine_ || !current_engine_->get_status().is_running) {
        auto win32 = std::make_unique<Win32JailSandbox>("sandbox_workspace");
        if (win32->initialize()) {
            current_engine_ = std::move(win32);
        }
    }
    return current_engine_.get();
}

SandboxStatus SandboxManager::get_status() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_engine_ || !current_engine_->get_status().is_running) {
        auto win32 = std::make_unique<Win32JailSandbox>("sandbox_workspace");
        if (win32->initialize()) {
            current_engine_ = std::move(win32);
        }
    }
    if (current_engine_) {
        return current_engine_->get_status();
    }
    SandboxStatus s;
    s.engine_type = "Win32_Jail";
    s.is_running = true;
    return s;
}

} // namespace sandbox
