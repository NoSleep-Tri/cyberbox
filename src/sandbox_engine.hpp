#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "simple_json.hpp"

namespace sandbox {

struct ExecutionResult {
    int exit_code = -1;
    std::string stdout_output;
    std::string stderr_output;
    double execution_time_ms = 0.0;
    bool timed_out = false;
};

struct FileItem {
    std::string name;
    std::string path;
    bool is_directory = false;
    uint64_t size_bytes = 0;
    std::string modified_time;
};

struct PortMapping {
    int container_port = 0;
    int host_port = 0;
    std::string description;
    bool active = false;
};

struct SandboxStatus {
    std::string engine_type;       // "Docker" or "Win32_Jail"
    std::string container_id;      // Container ID or Process Job name
    bool is_running = false;
    bool internet_enabled = false; // Airgap status
    uint64_t memory_used_bytes = 0;
    uint64_t memory_limit_bytes = 0;
    int active_processes = 0;
    std::vector<PortMapping> ports;
    std::string workspace_path;
};

class ISandboxEngine {
public:
    virtual ~ISandboxEngine() = default;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool reset() = 0;

    // Command Execution
    virtual ExecutionResult execute_command(const std::string& command, int timeout_seconds = 30) = 0;

    // Network & Port Controls
    virtual bool set_internet_access(bool enabled) = 0;
    virtual bool get_internet_access() const = 0;
    virtual bool add_port_forward(int container_port, int host_port, const std::string& desc = "") = 0;
    virtual bool remove_port_forward(int host_port) = 0;
    virtual std::vector<PortMapping> get_port_mappings() const = 0;

    // Filesystem within sandbox
    virtual std::vector<FileItem> list_files(const std::string& relative_path = "") = 0;
    virtual std::string read_file(const std::string& relative_path) = 0;
    virtual bool write_file(const std::string& relative_path, const std::string& content) = 0;
    virtual bool delete_file(const std::string& relative_path) = 0;

    // Status & Diagnostics
    virtual SandboxStatus get_status() = 0;
};

} // namespace sandbox
