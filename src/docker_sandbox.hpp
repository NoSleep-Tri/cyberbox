#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "sandbox_engine.hpp"
#include <windows.h>
#include <mutex>
#include <atomic>
#include <map>

namespace sandbox {

class DockerSandbox : public ISandboxEngine {
private:
    std::string pipe_name_ = "\\\\.\\pipe\\docker_engine";
    std::string container_id_;
    std::string container_name_ = "joyful_ai_sandbox";
    std::string image_name_ = "alpine:latest";
    std::atomic<bool> internet_enabled_{true};
    std::atomic<bool> is_running_{false};
    std::vector<PortMapping> port_mappings_;
    mutable std::mutex mutex_;

    // Named pipe HTTP communication with Docker Engine daemon
    std::string http_request(const std::string& method, const std::string& path, const std::string& body = "");
    bool ping_docker();
    bool pull_image_if_needed();
    bool create_container();
    bool start_container();
    bool remove_existing_container();

public:
    DockerSandbox(const std::string& image = "alpine:latest");
    ~DockerSandbox() override;

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
