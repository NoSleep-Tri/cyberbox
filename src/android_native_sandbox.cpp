#include "android_native_sandbox.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <algorithm>

#if defined(__ANDROID__) || defined(__linux__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#endif

namespace sandbox {

AndroidNativeSandbox::AndroidNativeSandbox(const std::string& workspace_dir) {
    if (!workspace_dir.empty()) {
        workspace_dir_ = workspace_dir;
    } else {
#if defined(__ANDROID__)
        // Mặc định thư mục app private data an toàn trên Android
        workspace_dir_ = "/data/data/com.cyberbox.ai/files/sandbox_workspace";
#else
        workspace_dir_ = "sandbox_workspace";
#endif
    }
}

AndroidNativeSandbox::~AndroidNativeSandbox() {
    shutdown();
}

bool AndroidNativeSandbox::set_workspace_directory(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path.empty()) return false;
    workspace_dir_ = path;
    try {
        std::filesystem::create_directories(workspace_dir_);
        return true;
    } catch (...) {
        return false;
    }
}

std::string AndroidNativeSandbox::get_workspace_directory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return workspace_dir_;
}

std::string AndroidNativeSandbox::resolve_safe_path(const std::string& relative_path) {
    try {
        std::filesystem::path base = std::filesystem::absolute(workspace_dir_);
        std::filesystem::path target = std::filesystem::absolute(base / relative_path);

        // Chống Path Traversal (../..) ra ngoài thư mục làm việc đã chỉ định
        std::string base_str = base.lexically_normal().string();
        std::string target_str = target.lexically_normal().string();

        if (target_str.rfind(base_str, 0) == 0) {
            return target_str;
        }
    } catch (...) {}
    return "";
}

bool AndroidNativeSandbox::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::filesystem::create_directories(workspace_dir_);
        std::filesystem::create_directories(workspace_dir_ + "/history");
        is_running_ = true;
        return true;
    } catch (...) {
        is_running_ = false;
        return false;
    }
}

void AndroidNativeSandbox::shutdown() {
    is_running_ = false;
}

bool AndroidNativeSandbox::reset() {
    shutdown();
    return initialize();
}

ExecutionResult AndroidNativeSandbox::execute_command(const std::string& command, int timeout_seconds) {
    ExecutionResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!is_running_) {
        result.stderr_output = "Error: Android Sandbox is not running.";
        result.exit_code = -1;
        return result;
    }

#if defined(__ANDROID__) || defined(__linux__)
    int pipe_stdout[2];
    int pipe_stderr[2];
    if (pipe(pipe_stdout) != 0 || pipe(pipe_stderr) != 0) {
        result.stderr_output = "Error: Failed to create pipes for Android sandbox.";
        result.exit_code = -1;
        return result;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Tiến trình con trong Sandbox Android
        close(pipe_stdout[0]);
        close(pipe_stderr[0]);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        dup2(pipe_stderr[1], STDERR_FILENO);
        close(pipe_stdout[1]);
        close(pipe_stderr[1]);

        // Đổi thư mục làm việc vào workspace linh hoạt
        chdir(workspace_dir_.c_str());

        // Chạy qua Android Native Shell (/system/bin/sh)
        execl("/system/bin/sh", "sh", "-c", command.c_str(), (char*)NULL);
        _exit(127);
    } else if (pid > 0) {
        close(pipe_stdout[1]);
        close(pipe_stderr[1]);

        // Đọc output
        char buffer[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(pipe_stdout[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            result.stdout_output.append(buffer, bytes_read);
        }
        while ((bytes_read = read(pipe_stderr[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            result.stderr_output.append(buffer, bytes_read);
        }

        close(pipe_stdout[0]);
        close(pipe_stderr[0]);

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else {
            result.exit_code = -1;
        }
    } else {
        result.stderr_output = "Error: Fork failed on Android.";
        result.exit_code = -1;
    }
#else
    // Stub khi compile / test trên Windows/Host
    result.exit_code = 0;
    result.stdout_output = "[Android Native Sandbox Stub]: " + command + "\n";
#endif

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return result;
}

bool AndroidNativeSandbox::set_internet_access(bool enabled) {
    internet_enabled_ = enabled;
    return true;
}

bool AndroidNativeSandbox::get_internet_access() const {
    return internet_enabled_.load();
}

bool AndroidNativeSandbox::add_port_forward(int container_port, int host_port, const std::string& desc) {
    std::lock_guard<std::mutex> lock(mutex_);
    port_mappings_.push_back({ container_port, host_port, desc, true });
    return true;
}

bool AndroidNativeSandbox::remove_port_forward(int host_port) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove_if(port_mappings_.begin(), port_mappings_.end(),
        [host_port](const PortMapping& pm) { return pm.host_port == host_port; });
    if (it != port_mappings_.end()) {
        port_mappings_.erase(it, port_mappings_.end());
        return true;
    }
    return false;
}

std::vector<PortMapping> AndroidNativeSandbox::get_port_mappings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return port_mappings_;
}

std::vector<FileItem> AndroidNativeSandbox::list_files(const std::string& relative_path) {
    std::vector<FileItem> items;
    std::string safe_dir = resolve_safe_path(relative_path);
    if (safe_dir.empty()) return items;

    try {
        if (!std::filesystem::exists(safe_dir) || !std::filesystem::is_directory(safe_dir)) {
            return items;
        }
        for (const auto& entry : std::filesystem::directory_iterator(safe_dir)) {
            FileItem item;
            item.name = entry.path().filename().string();
            item.path = relative_path.empty() ? item.name : (relative_path + "/" + item.name);
            item.is_directory = entry.is_directory();
            if (entry.is_regular_file()) {
                item.size_bytes = entry.file_size();
            }
            items.push_back(item);
        }
    } catch (...) {}
    return items;
}

std::string AndroidNativeSandbox::read_file(const std::string& relative_path) {
    std::string safe_file = resolve_safe_path(relative_path);
    if (safe_file.empty()) return "Error: Path traversal or invalid path.";

    std::ifstream ifs(safe_file, std::ios::binary);
    if (!ifs.is_open()) return "Error: Cannot open file.";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

bool AndroidNativeSandbox::write_file(const std::string& relative_path, const std::string& content) {
    std::string safe_file = resolve_safe_path(relative_path);
    if (safe_file.empty()) return false;

    try {
        std::filesystem::path p(safe_file);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream ofs(safe_file, std::ios::binary);
        if (!ofs.is_open()) return false;
        ofs.write(content.data(), content.size());
        return ofs.good();
    } catch (...) {
        return false;
    }
}

bool AndroidNativeSandbox::delete_file(const std::string& relative_path) {
    std::string safe_file = resolve_safe_path(relative_path);
    if (safe_file.empty()) return false;
    try {
        return std::filesystem::remove(safe_file);
    } catch (...) {
        return false;
    }
}

SandboxStatus AndroidNativeSandbox::get_status() {
    std::lock_guard<std::mutex> lock(mutex_);
    SandboxStatus status;
    status.engine_type = "Android_Native_Sandbox";
    status.container_id = "Android_App_Sandbox";
    status.is_running = is_running_.load();
    status.internet_enabled = internet_enabled_.load();
    status.memory_limit_bytes = 2048ULL * 1024ULL * 1024ULL; // 2 GB
    status.ports = port_mappings_;
    status.workspace_path = workspace_dir_;
    return status;
}

} // namespace sandbox
