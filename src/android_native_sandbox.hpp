#pragma once
#include "sandbox_engine.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace sandbox {

class AndroidNativeSandbox : public ISandboxEngine {
private:
    std::string workspace_dir_;
    std::string internal_data_dir_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> internet_enabled_{true};
    mutable std::mutex mutex_;
    std::vector<PortMapping> port_mappings_;

    std::string resolve_safe_path(const std::string& relative_path);

public:
    explicit AndroidNativeSandbox(const std::string& workspace_dir = "");
    virtual ~AndroidNativeSandbox();

    // Cấu hình linh hoạt thư mục làm việc (External Storage, App Private Dir, SDCard)
    bool set_workspace_directory(const std::string& path);
    std::string get_workspace_directory() const;

    // ISandboxEngine Lifecycle
    bool initialize() override;
    void shutdown() override;
    bool reset() override;

    // Command Execution (via /system/bin/sh, toybox, or chroot/proot)
    ExecutionResult execute_command(const std::string& command, int timeout_seconds = 30) override;

    // Network & Port Controls
    bool set_internet_access(bool enabled) override;
    bool get_internet_access() const override;
    bool add_port_forward(int container_port, int host_port, const std::string& desc = "") override;
    bool remove_port_forward(int host_port) override;
    std::vector<PortMapping> get_port_mappings() const override;

    // Filesystem within sandbox (Safe sandbox isolation inside designated workspace)
    std::vector<FileItem> list_files(const std::string& relative_path = "") override;
    std::string read_file(const std::string& relative_path) override;
    bool write_file(const std::string& relative_path, const std::string& content) override;
    bool delete_file(const std::string& relative_path) override;

    // Status & Diagnostics
    SandboxStatus get_status() override;
};

} // namespace sandbox
