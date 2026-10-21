#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "sandbox_engine.hpp"
#include <windows.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

namespace sandbox {

class Win32JailSandbox : public ISandboxEngine {
private:
    std::string workspace_dir_;
    HANDLE job_object_ = NULL;
    std::atomic<bool> internet_enabled_{true};
    std::atomic<bool> is_running_{false};
    std::vector<PortMapping> port_mappings_;
    mutable std::mutex mutex_;

    void setup_job_limits();
    void create_shims();
    static std::string sanitize_command_for_windows(std::string cmd);
    std::string resolve_safe_path(const std::string& rel_path);
    bool apply_firewall_rule(bool block_internet);
    bool validate_security_policy(const std::string& cmd, std::string& violation_reason);
    bool set_low_integrity_folder(const std::string& path);
    HANDLE create_jail_token();
    bool handle_heredoc_write(const std::string& command, ExecutionResult& result);

public:
    Win32JailSandbox(const std::string& workspace_dir = "sandbox_workspace");
    ~Win32JailSandbox() override;

    bool initialize() override;
    void shutdown() override;
    bool reset() override;

    ExecutionResult execute_command(const std::string& command, int timeout_seconds = 30) override;

    bool set_internet_access(bool enabled) override;
    bool get_internet_access() const override;
    bool add_port_forward(int container_port, int host_port, const std::string& desc = "") override;
    bool remove_port_forward(int host_port) override;
    std::vector<PortMapping> get_port_mappings() const override;

    std::vector<FileItem> list_files(const std::string& relative_path = "") override;
    std::string read_file(const std::string& relative_path) override;
    bool write_file(const std::string& relative_path, const std::string& content) override;
    bool delete_file(const std::string& relative_path) override;

    SandboxStatus get_status() override;
};

} // namespace sandbox
