#pragma once
#include "sandbox_engine.hpp"
#include "docker_sandbox.hpp"
#include "win32_jail_sandbox.hpp"
#include <memory>
#include <mutex>

namespace sandbox {

class SandboxManager {
private:
    std::unique_ptr<ISandboxEngine> current_engine_;
    std::string preferred_engine_type_;
    mutable std::mutex mutex_;

public:
    SandboxManager();
    ~SandboxManager();

    bool initialize(const std::string& preferred_type = "auto");
    bool switch_engine(const std::string& engine_type);
    
    ISandboxEngine* get_engine();
    SandboxStatus get_status();
};

} // namespace sandbox
