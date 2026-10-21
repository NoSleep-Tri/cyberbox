#include "docker_sandbox.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

namespace sandbox {

DockerSandbox::DockerSandbox(const std::string& image) : image_name_(image) {}

DockerSandbox::~DockerSandbox() {
    shutdown();
}

std::string DockerSandbox::http_request(const std::string& method, const std::string& path, const std::string& body) {
    HANDLE hPipe = CreateFileA(
        pipe_name_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        return "";
    }

    std::ostringstream req;
    req << method << " " << path << " HTTP/1.1\r\n";
    req << "Host: localhost\r\n";
    req << "User-Agent: JoyfulSandboxC++\r\n";
    req << "Accept: */*\r\n";
    if (!body.empty()) {
        req << "Content-Type: application/json\r\n";
        req << "Content-Length: " << body.length() << "\r\n";
    }
    req << "Connection: close\r\n\r\n";
    if (!body.empty()) {
        req << body;
    }

    std::string req_str = req.str();
    DWORD written = 0;
    if (!WriteFile(hPipe, req_str.c_str(), static_cast<DWORD>(req_str.length()), &written, NULL)) {
        CloseHandle(hPipe);
        return "";
    }

    std::string response;
    char buffer[4096];
    DWORD read_bytes = 0;
    while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &read_bytes, NULL) && read_bytes > 0) {
        buffer[read_bytes] = '\0';
        response.append(buffer, read_bytes);
    }

    CloseHandle(hPipe);
    return response;
}

static std::string extract_http_body(const std::string& raw) {
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return raw;
    }
    std::string headers = raw.substr(0, header_end);
    std::string body = raw.substr(header_end + 4);

    // If chunked transfer encoding
    if (headers.find("Transfer-Encoding: chunked") != std::string::npos) {
        std::string unchunked;
        size_t pos = 0;
        while (pos < body.size()) {
            size_t line_end = body.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            std::string hex_len = body.substr(pos, line_end - pos);
            size_t chunk_len = 0;
            try {
                chunk_len = std::stoul(hex_len, nullptr, 16);
            } catch (...) {
                break;
            }
            if (chunk_len == 0) break;
            pos = line_end + 2;
            if (pos + chunk_len <= body.size()) {
                unchunked.append(body.substr(pos, chunk_len));
                pos += chunk_len + 2; // skip trailing \r\n
            } else {
                unchunked.append(body.substr(pos));
                break;
            }
        }
        return unchunked;
    }
    return body;
}

bool DockerSandbox::ping_docker() {
    std::string res = http_request("GET", "/_ping");
    return res.find("200 OK") != std::string::npos || res.find("OK") != std::string::npos;
}

bool DockerSandbox::remove_existing_container() {
    // Delete existing container with same name if any
    http_request("DELETE", "/containers/" + container_name_ + "?force=true");
    return true;
}

bool DockerSandbox::create_container() {
    remove_existing_container();

    json::Value root = json::Value::object();
    root["Image"] = image_name_;
    
    json::Value cmd = json::Value::array();
    cmd.push_back("/bin/sh");
    cmd.push_back("-c");
    cmd.push_back("mkdir -p /workspace && cd /workspace && tail -f /dev/null");
    root["Cmd"] = cmd;
    root["WorkingDir"] = "/workspace";
    root["OpenStdin"] = true;
    root["Tty"] = false;

    json::Value host_config = json::Value::object();
    // 2 GB RAM limit
    host_config["Memory"] = static_cast<long long>(2147483648LL);
    // Limit to 2 CPUs (2 * 1e9 nano CPUs)
    host_config["NanoCpus"] = static_cast<long long>(2000000000LL);
    host_config["PidsLimit"] = 128;
    host_config["NetworkMode"] = internet_enabled_.load() ? "bridge" : "none";

    // Port mappings
    json::Value port_bindings = json::Value::object();
    for (const auto& pm : port_mappings_) {
        std::string c_port_key = std::to_string(pm.container_port) + "/tcp";
        json::Value binding_arr = json::Value::array();
        json::Value host_port_obj = json::Value::object();
        host_port_obj["HostPort"] = std::to_string(pm.host_port);
        host_port_obj["HostIp"] = "127.0.0.1";
        binding_arr.push_back(host_port_obj);
        port_bindings[c_port_key] = binding_arr;
    }
    host_config["PortBindings"] = port_bindings;
    root["HostConfig"] = host_config;

    std::string res = http_request("POST", "/containers/create?name=" + container_name_, root.dump());
    if (res.find("201 Created") == std::string::npos) {
        return false;
    }

    std::string body = extract_http_body(res);
    json::Value json_res = json::parse(body);
    if (json_res.contains("Id")) {
        container_id_ = json_res["Id"].as_string();
        return true;
    }
    return false;
}

bool DockerSandbox::start_container() {
    if (container_id_.empty()) return false;
    std::string res = http_request("POST", "/containers/" + container_id_ + "/start");
    return (res.find("204 No Content") != std::string::npos || res.find("304 Not Modified") != std::string::npos);
}

bool DockerSandbox::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ping_docker()) {
        std::cerr << "[DockerSandbox] Cannot connect to Docker named pipe: " << pipe_name_ << "\n";
        return false;
    }

    if (!create_container()) {
        std::cerr << "[DockerSandbox] Failed to create container\n";
        return false;
    }

    if (!start_container()) {
        std::cerr << "[DockerSandbox] Failed to start container\n";
        return false;
    }

    is_running_ = true;
    std::cout << "[DockerSandbox] Successfully initialized container ID: " << container_id_.substr(0, 12) << "\n";
    return true;
}

void DockerSandbox::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!container_id_.empty()) {
        http_request("POST", "/containers/" + container_id_ + "/stop?t=2");
        http_request("DELETE", "/containers/" + container_id_ + "?force=true");
        container_id_.clear();
    }
    is_running_ = false;
}

bool DockerSandbox::reset() {
    shutdown();
    return initialize();
}

ExecutionResult DockerSandbox::execute_command(const std::string& command, int timeout_seconds) {
    ExecutionResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!is_running_ || container_id_.empty()) {
        result.stderr_output = "Error: Docker container is not running.";
        result.exit_code = -1;
        return result;
    }

    // Step 1: Create exec instance
    json::Value exec_config = json::Value::object();
    exec_config["AttachStdout"] = true;
    exec_config["AttachStderr"] = true;
    exec_config["Tty"] = false;
    
    json::Value cmd = json::Value::array();
    cmd.push_back("/bin/sh");
    cmd.push_back("-c");
    cmd.push_back("cd /workspace && " + command);
    exec_config["Cmd"] = cmd;

    std::string res = http_request("POST", "/containers/" + container_id_ + "/exec", exec_config.dump());
    if (res.find("201 Created") == std::string::npos) {
        result.stderr_output = "Failed to create exec instance in container.";
        result.exit_code = -1;
        return result;
    }

    std::string body = extract_http_body(res);
    json::Value exec_json = json::parse(body);
    std::string exec_id = exec_json["Id"].as_string();
    if (exec_id.empty()) {
        result.stderr_output = "No exec ID returned by Docker daemon.";
        result.exit_code = -1;
        return result;
    }

    // Step 2: Start exec instance and read multiplexed stream
    json::Value start_config = json::Value::object();
    start_config["Detach"] = false;
    start_config["Tty"] = false;

    std::string stream_raw = http_request("POST", "/exec/" + exec_id + "/start", start_config.dump());
    std::string stream_body = extract_http_body(stream_raw);

    // Parse Docker demuxed frames
    // Format: [1 byte type (1=stdout, 2=stderr)][3 bytes pad][4 bytes size big-endian][payload]
    size_t offset = 0;
    while (offset + 8 <= stream_body.size()) {
        unsigned char type = static_cast<unsigned char>(stream_body[offset]);
        uint32_t frame_len = (static_cast<unsigned char>(stream_body[offset + 4]) << 24) |
                             (static_cast<unsigned char>(stream_body[offset + 5]) << 16) |
                             (static_cast<unsigned char>(stream_body[offset + 6]) << 8)  |
                             (static_cast<unsigned char>(stream_body[offset + 7]));
        offset += 8;

        if (offset + frame_len > stream_body.size()) {
            std::string chunk = stream_body.substr(offset);
            if (type == 2) result.stderr_output += chunk;
            else result.stdout_output += chunk;
            break;
        }

        std::string chunk = stream_body.substr(offset, frame_len);
        if (type == 2) {
            result.stderr_output += chunk;
        } else {
            result.stdout_output += chunk;
        }
        offset += frame_len;
    }

    // If output was raw text without frame headers
    if (result.stdout_output.empty() && result.stderr_output.empty() && !stream_body.empty()) {
        result.stdout_output = stream_body;
    }

    // Step 3: Inspect exec for exit code
    std::string inspect_res = http_request("GET", "/exec/" + exec_id + "/json");
    std::string inspect_body = extract_http_body(inspect_res);
    json::Value inspect_json = json::parse(inspect_body);
    result.exit_code = inspect_json["ExitCode"].as_int(0);

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return result;
}

bool DockerSandbox::set_internet_access(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_running_ || container_id_.empty()) {
        internet_enabled_ = enabled;
        return true;
    }

    if (enabled == internet_enabled_.load()) {
        return true;
    }

    if (enabled) {
        // Connect to bridge network
        json::Value body = json::Value::object();
        body["Container"] = container_id_;
        std::string res = http_request("POST", "/networks/bridge/connect", body.dump());
        if (res.find("200 OK") != std::string::npos || res.find("204 No Content") != std::string::npos) {
            internet_enabled_ = true;
            return true;
        }
    } else {
        // Disconnect from bridge network
        json::Value body = json::Value::object();
        body["Container"] = container_id_;
        body["Force"] = true;
        std::string res = http_request("POST", "/networks/bridge/disconnect", body.dump());
        if (res.find("200 OK") != std::string::npos || res.find("204 No Content") != std::string::npos) {
            internet_enabled_ = false;
            return true;
        }
    }

    // If dynamic connect/disconnect isn't supported, recreate container with the new network mode
    internet_enabled_ = enabled;
    create_container();
    start_container();
    return true;
}

bool DockerSandbox::get_internet_access() const {
    return internet_enabled_.load();
}

bool DockerSandbox::add_port_forward(int container_port, int host_port, const std::string& desc) {
    std::lock_guard<std::mutex> lock(mutex_);
    PortMapping pm;
    pm.container_port = container_port;
    pm.host_port = host_port;
    pm.description = desc;
    pm.active = true;
    port_mappings_.push_back(pm);

    // Recreate container with new port mappings
    if (is_running_) {
        create_container();
        start_container();
    }
    return true;
}

bool DockerSandbox::remove_port_forward(int host_port) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = port_mappings_.begin(); it != port_mappings_.end(); ++it) {
        if (it->host_port == host_port) {
            port_mappings_.erase(it);
            if (is_running_) {
                create_container();
                start_container();
            }
            return true;
        }
    }
    return false;
}

std::vector<PortMapping> DockerSandbox::get_port_mappings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return port_mappings_;
}

std::vector<FileItem> DockerSandbox::list_files(const std::string& relative_path) {
    std::vector<FileItem> items;
    std::string target = "/workspace/" + relative_path;
    // Execute ls -la in container
    ExecutionResult res = execute_command("ls -la " + target + " 2>/dev/null", 10);
    std::istringstream stream(res.stdout_output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line.find("total") == 0) continue;
        std::istringstream liness(line);
        std::string perms, links, owner, group, size_str, m1, m2, m3, name;
        if (liness >> perms >> links >> owner >> group >> size_str >> m1 >> m2 >> m3 >> name) {
            if (name == "." || name == "..") continue;
            FileItem item;
            item.name = name;
            item.path = relative_path.empty() ? name : (relative_path + "/" + name);
            item.is_directory = (perms[0] == 'd');
            try {
                item.size_bytes = std::stoull(size_str);
            } catch (...) {
                item.size_bytes = 0;
            }
            item.modified_time = m1 + " " + m2 + " " + m3;
            items.push_back(item);
        }
    }
    return items;
}

std::string DockerSandbox::read_file(const std::string& relative_path) {
    ExecutionResult res = execute_command("cat /workspace/" + relative_path, 10);
    return res.stdout_output;
}

bool DockerSandbox::write_file(const std::string& relative_path, const std::string& content) {
    // Escape and write using base64 decoding inside container
    // To handle arbitrary binary or multi-line files without escaping issues:
    // echo -n "<base64>" | base64 -d > /workspace/...
    // Let's do base64 encoding
    static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    int val = 0, valb = -6;
    for (unsigned char c : content) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            b64.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) b64.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (b64.size() % 4) b64.push_back('=');

    std::string cmd = "mkdir -p $(dirname /workspace/" + relative_path + ") && echo \"" + b64 + "\" | base64 -d > /workspace/" + relative_path;
    ExecutionResult res = execute_command(cmd, 15);
    return res.exit_code == 0;
}

bool DockerSandbox::delete_file(const std::string& relative_path) {
    ExecutionResult res = execute_command("rm -rf /workspace/" + relative_path, 10);
    return res.exit_code == 0;
}

SandboxStatus DockerSandbox::get_status() {
    std::lock_guard<std::mutex> lock(mutex_);
    SandboxStatus status;
    status.engine_type = "Docker";
    status.container_id = container_id_.substr(0, 12);
    status.is_running = is_running_.load();
    status.internet_enabled = internet_enabled_.load();
    status.memory_limit_bytes = 2147483648LL;
    status.ports = port_mappings_;
    status.workspace_path = "/workspace";
    return status;
}

} // namespace sandbox
