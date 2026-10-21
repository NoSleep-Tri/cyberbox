#include "ai_client.hpp"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>
#include <regex>

#pragma comment(lib, "winhttp.lib")

namespace ai {

AiClient::AiClient(const AiConfig& config) : config_(config) {}

void AiClient::set_config(const AiConfig& config) {
    config_ = config;
}

AiConfig AiClient::get_config() const {
    return config_;
}

static bool parse_url(const std::string& url, bool& is_https, std::wstring& host, INTERNET_PORT& port, std::wstring& path) {
    std::string s = url;
    if (s.rfind("https://", 0) == 0) {
        is_https = true;
        s = s.substr(8);
        port = INTERNET_DEFAULT_HTTPS_PORT;
    } else if (s.rfind("http://", 0) == 0) {
        is_https = false;
        s = s.substr(7);
        port = INTERNET_DEFAULT_HTTP_PORT;
    } else {
        is_https = true;
        port = INTERNET_DEFAULT_HTTPS_PORT;
    }

    size_t slash_pos = s.find('/');
    std::string host_port_str = (slash_pos == std::string::npos) ? s : s.substr(0, slash_pos);
    std::string path_str = (slash_pos == std::string::npos) ? "" : s.substr(slash_pos);

    size_t colon_pos = host_port_str.find(':');
    if (colon_pos != std::string::npos) {
        std::string port_str = host_port_str.substr(colon_pos + 1);
        host_port_str = host_port_str.substr(0, colon_pos);
        try {
            port = static_cast<INTERNET_PORT>(std::stoi(port_str));
        } catch (...) {}
    } else if (!is_https && (host_port_str == "127.0.0.1" || host_port_str == "localhost")) {
        port = 11434;
    }

    host = std::wstring(host_port_str.begin(), host_port_str.end());
    path = std::wstring(path_str.begin(), path_str.end());
    return true;
}

static std::string to_utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::string AiClient::http_post(const std::string& full_url, const std::string& auth_header, const std::string& json_payload) {
    bool is_https = true;
    std::wstring host_w, path_w;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;

    if (!parse_url(full_url, is_https, host_w, port, path_w)) {
        return "";
    }

    DWORD access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    if (host_w.find(L"192.168.") == 0 || host_w.find(L"10.") == 0 || 
        host_w.find(L"172.") == 0 || host_w.find(L"127.") == 0 || 
        host_w == L"localhost") {
        access_type = WINHTTP_ACCESS_TYPE_NO_PROXY;
    }

    HINTERNET hSession = WinHttpOpen(
        L"CyberBoxAI/1.0",
        access_type,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return "";

    // Set generous timeouts for local LLM inference (e.g. Ollama reasoning models):
    // Resolve: 15s, Connect: 15s, Send: 30s, Receive: 600s (10 minutes)
    WinHttpSetTimeouts(hSession, 15000, 15000, 30000, 600000);

    HINTERNET hConnect = WinHttpConnect(hSession, host_w.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD req_flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        path_w.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        req_flags
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Set headers
    std::wstring headers_w = L"Content-Type: application/json\r\n";
    if (!auth_header.empty()) {
        std::wstring auth_w(auth_header.begin(), auth_header.end());
        headers_w += L"Authorization: " + auth_w + L"\r\n";
    }

    BOOL bResults = WinHttpSendRequest(
        hRequest,
        headers_w.c_str(),
        static_cast<DWORD>(headers_w.length()),
        (LPVOID)json_payload.c_str(),
        static_cast<DWORD>(json_payload.length()),
        static_cast<DWORD>(json_payload.length()),
        0
    );

    std::string response;
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                response.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

std::string AiClient::http_get(const std::string& full_url, const std::string& auth_header) {
    bool is_https = true;
    std::wstring host_w, path_w;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;

    if (!parse_url(full_url, is_https, host_w, port, path_w)) {
        return "";
    }

    DWORD access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    if (host_w.find(L"192.168.") == 0 || host_w.find(L"10.") == 0 || 
        host_w.find(L"172.") == 0 || host_w.find(L"127.") == 0 || 
        host_w == L"localhost") {
        access_type = WINHTTP_ACCESS_TYPE_NO_PROXY;
    }

    HINTERNET hSession = WinHttpOpen(
        L"CyberBoxAI/1.0",
        access_type,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return "";

    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 30000);

    HINTERNET hConnect = WinHttpConnect(hSession, host_w.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD req_flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path_w.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        req_flags
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring headers_w = L"Accept: application/json\r\n";
    if (!auth_header.empty()) {
        std::wstring auth_w(auth_header.begin(), auth_header.end());
        headers_w += L"Authorization: " + auth_w + L"\r\n";
    }

    BOOL bResults = WinHttpSendRequest(
        hRequest,
        headers_w.c_str(),
        static_cast<DWORD>(headers_w.length()),
        NULL,
        0,
        0,
        0
    );

    std::string response;
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                response.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

AiResponse AiClient::chat_completion(const std::vector<ChatMessage>& messages) {
    return chat_completion(messages, config_);
}

AiResponse AiClient::chat_completion(const std::vector<ChatMessage>& messages, const AiConfig& config) {
    AiResponse resp;

    std::string endpoint = config.base_url;
    if (endpoint.back() == '/') endpoint.pop_back();
    if (endpoint.rfind("/chat/completions") == std::string::npos) {
        endpoint += "/chat/completions";
    }

    // Build payload
    json::Value root = json::Value::object();
    root["model"] = config.model;
    root["temperature"] = config.temperature;
    root["max_tokens"] = config.max_tokens;

    json::Value msg_arr = json::Value::array();
    for (const auto& m : messages) {
        json::Value obj = json::Value::object();
        obj["role"] = m.role;
        obj["content"] = m.content;
        msg_arr.push_back(obj);
    }
    root["messages"] = msg_arr;

    std::string auth_header = "";
    if (!config.api_key.empty()) {
        auth_header = "Bearer " + config.api_key;
    }

    std::string raw_json = http_post(endpoint, auth_header, root.dump());
    if (raw_json.empty()) {
        resp.success = false;
        resp.error_message = "Failed to connect to AI server at: " + endpoint;
        return resp;
    }

    json::Value parsed = json::parse(raw_json);
    if (parsed.contains("error")) {
        resp.success = false;
        resp.error_message = parsed["error"]["message"].as_string("Unknown AI API error");
        return resp;
    }

    if (parsed.contains("choices") && parsed["choices"].size() > 0) {
        const auto& choice = parsed["choices"][0];
        if (choice.contains("message")) {
            const auto& msg = choice["message"];
            resp.success = true;
            if (msg.contains("content") && msg["content"].is_string()) {
                resp.content = msg["content"].as_string();
            }
            // For reasoning models (e.g. MiniCPM-16k, DeepSeek-R1, QwQ):
            if (resp.content.empty()) {
                if (msg.contains("reasoning") && msg["reasoning"].is_string()) {
                    resp.content = msg["reasoning"].as_string();
                } else if (msg.contains("reasoning_content") && msg["reasoning_content"].is_string()) {
                    resp.content = msg["reasoning_content"].as_string();
                }
            }
            // Check for native OpenAI tool_calls
            if (msg.contains("tool_calls") && msg["tool_calls"].size() > 0) {
                for (size_t t = 0; t < msg["tool_calls"].size(); ++t) {
                    const auto& tc = msg["tool_calls"][t];
                    if (tc.contains("function")) {
                        std::string fn_name = tc["function"]["name"].as_string();
                        std::string fn_args = tc["function"]["arguments"].as_string();
                        std::string cmd = "";
                        try {
                            json::Value args_json = json::parse(fn_args);
                            if (args_json.contains("command")) cmd = args_json["command"].as_string();
                            else if (args_json.contains("cmd")) cmd = args_json["cmd"].as_string();
                        } catch (...) {}
                        if (cmd.empty()) cmd = fn_args;
                        if (!cmd.empty()) {
                            if (!resp.content.empty()) resp.content += "\n";
                            resp.content += "<function name=\"" + fn_name + "\"><param name=\"command\"><![CDATA[" + cmd + "]]></param></function>";
                        }
                    }
                }
            }
            if (parsed.contains("usage")) {
                resp.prompt_tokens = parsed["usage"]["prompt_tokens"].as_int(0);
                resp.completion_tokens = parsed["usage"]["completion_tokens"].as_int(0);
            }
            return resp;
        }
    }

    resp.success = false;
    resp.error_message = "Invalid response format received from LLM.";
    return resp;
}

AiResponse AiClient::chat_completion_stream(const std::vector<ChatMessage>& messages, StreamCallback on_chunk) {
    return chat_completion_stream(messages, config_, on_chunk);
}

AiResponse AiClient::chat_completion_stream(const std::vector<ChatMessage>& messages, const AiConfig& config, StreamCallback on_chunk) {
    AiResponse resp;

    std::string endpoint = config.base_url;
    if (endpoint.back() == '/') endpoint.pop_back();
    if (endpoint.rfind("/chat/completions") == std::string::npos) {
        endpoint += "/chat/completions";
    }

    // Build payload with stream = true
    json::Value root = json::Value::object();
    root["model"] = config.model;
    root["temperature"] = config.temperature;
    root["max_tokens"] = config.max_tokens;
    root["stream"] = true;

    json::Value msg_arr = json::Value::array();
    for (const auto& m : messages) {
        json::Value obj = json::Value::object();
        obj["role"] = m.role;
        obj["content"] = m.content;
        msg_arr.push_back(obj);
    }
    root["messages"] = msg_arr;

    std::string auth_header = "";
    if (!config.api_key.empty()) {
        auth_header = "Bearer " + config.api_key;
    }

    bool is_https = true;
    std::wstring host_w, path_w;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    if (!parse_url(endpoint, is_https, host_w, port, path_w)) {
        resp.success = false;
        resp.error_message = "Invalid URL: " + endpoint;
        return resp;
    }

    DWORD access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    if (host_w.find(L"192.168.") == 0 || host_w.find(L"10.") == 0 || 
        host_w.find(L"172.") == 0 || host_w.find(L"127.") == 0 || 
        host_w == L"localhost") {
        access_type = WINHTTP_ACCESS_TYPE_NO_PROXY;
    }

    HINTERNET hSession = WinHttpOpen(
        L"CyberBoxAI/1.0",
        access_type,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) {
        resp.success = false;
        resp.error_message = "Failed to initialize WinHTTP session.";
        return resp;
    }

    WinHttpSetTimeouts(hSession, 15000, 15000, 30000, 600000);

    HINTERNET hConnect = WinHttpConnect(hSession, host_w.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        resp.success = false;
        resp.error_message = "Failed to connect to host: " + to_utf8(host_w);
        return resp;
    }

    DWORD req_flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        path_w.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        req_flags
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.success = false;
        resp.error_message = "Failed to open WinHTTP request.";
        return resp;
    }

    std::wstring headers_w = L"Content-Type: application/json\r\nAccept: text/event-stream\r\n";
    if (!auth_header.empty()) {
        std::wstring auth_w(auth_header.begin(), auth_header.end());
        headers_w += L"Authorization: " + auth_w + L"\r\n";
    }

    std::string json_payload = root.dump();
    BOOL bResults = WinHttpSendRequest(
        hRequest,
        headers_w.c_str(),
        static_cast<DWORD>(headers_w.length()),
        (LPVOID)json_payload.c_str(),
        static_cast<DWORD>(json_payload.length()),
        static_cast<DWORD>(json_payload.length()),
        0
    );

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (!bResults) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.success = false;
        resp.error_message = "Failed to connect to AI server at " + endpoint + " (WinHTTP error " + std::to_string(err) + ").";
        return resp;
    }

    std::string full_content = "";
    std::string full_reasoning = "";
    std::string sse_buffer = "";
    std::string raw_accum = "";
    bool in_think_tag = false;
    bool stream_done = false;

    RepetitionDetector content_detector(30);
    RepetitionDetector reasoning_detector(30);

    DWORD dwSize = 0;
    while (!stream_done && WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
        std::vector<char> buffer(dwSize + 1);
        DWORD dwDownloaded = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;
        sse_buffer.append(buffer.data(), dwDownloaded);
        raw_accum.append(buffer.data(), dwDownloaded);

        size_t newline_pos;
        while (!stream_done && (newline_pos = sse_buffer.find('\n')) != std::string::npos) {
            std::string line = sse_buffer.substr(0, newline_pos);
            sse_buffer.erase(0, newline_pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line.rfind("data:", 0) == 0) {
                std::string data_str = line.substr(5);
                if (!data_str.empty() && data_str[0] == ' ') data_str.erase(0, 1);
                if (data_str == "[DONE]") {
                    stream_done = true;
                    break;
                }
                try {
                    json::Value parsed = json::parse(data_str);
                    if (parsed.contains("error")) {
                        resp.success = false;
                        resp.error_message = parsed["error"]["message"].as_string("AI API Stream Error");
                        stream_done = true;
                        break;
                    }
                    if (parsed.contains("choices") && parsed["choices"].size() > 0) {
                        const auto& choice = parsed["choices"][0];
                        if (choice.contains("delta")) {
                            const auto& delta = choice["delta"];
                            std::string d_content = delta.contains("content") ? delta["content"].as_string() : "";
                            std::string d_reasoning = "";
                            if (delta.contains("reasoning")) d_reasoning = delta["reasoning"].as_string();
                            else if (delta.contains("reasoning_content")) d_reasoning = delta["reasoning_content"].as_string();

                            // Robust inline <think> / </think> tag stream parser:
                            // Preserves all text before and after tags without losing tokens
                            if (!d_content.empty()) {
                                if (d_reasoning.empty()) {
                                    std::string piece = d_content;
                                    std::string out_content;
                                    std::string out_reasoning;
                                    while (!piece.empty()) {
                                        if (!in_think_tag) {
                                            size_t tag_pos = piece.find("<think>");
                                            if (tag_pos == std::string::npos) {
                                                out_content += piece;
                                                piece.clear();
                                            } else {
                                                out_content += piece.substr(0, tag_pos);
                                                in_think_tag = true;
                                                piece = piece.substr(tag_pos + 7);
                                            }
                                        } else {
                                            size_t end_tag_pos = piece.find("</think>");
                                            if (end_tag_pos == std::string::npos) {
                                                out_reasoning += piece;
                                                piece.clear();
                                            } else {
                                                out_reasoning += piece.substr(0, end_tag_pos);
                                                in_think_tag = false;
                                                piece = piece.substr(end_tag_pos + 8);
                                            }
                                        }
                                    }
                                    d_content = out_content;
                                    d_reasoning = out_reasoning;
                                } else if (in_think_tag) {
                                    d_reasoning += d_content;
                                    d_content = "";
                                }
                            }

                            // Real-time Repetition Detector (Anti-Loop Guard):
                            // If model begins looping repetitive sentences/tokens, immediately abort stream!
                            if (!d_content.empty()) {
                                if (content_detector.is_repeating(d_content)) {
                                    stream_done = true;
                                    break;
                                }
                            }
                            if (!d_reasoning.empty()) {
                                if (reasoning_detector.is_repeating(d_reasoning)) {
                                    stream_done = true;
                                    break;
                                }
                            }

                            if (!d_content.empty() || !d_reasoning.empty()) {
                                full_content += d_content;
                                full_reasoning += d_reasoning;
                                if (on_chunk) {
                                    on_chunk({ d_content, d_reasoning, false });
                                }
                            }
                        }
                    }
                } catch (...) {}
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Fallback if server returned non-SSE JSON or error JSON
    if (full_content.empty() && full_reasoning.empty() && !raw_accum.empty()) {
        try {
            json::Value parsed = json::parse(raw_accum);
            if (parsed.contains("error")) {
                resp.success = false;
                resp.error_message = parsed["error"]["message"].as_string("AI API Error");
                return resp;
            }
            if (parsed.contains("choices") && parsed["choices"].size() > 0) {
                const auto& msg = parsed["choices"][0]["message"];
                if (msg.contains("content") && msg["content"].is_string()) {
                    full_content = msg["content"].as_string();
                    if (on_chunk && !full_content.empty()) {
                        on_chunk({ full_content, "", false });
                    }
                }
            }
        } catch (...) {}
    }

    if (on_chunk) {
        on_chunk({ "", "", true });
    }

    if (full_content.empty() && full_reasoning.empty() && !resp.error_message.empty()) {
        resp.success = false;
        return resp;
    }

    resp.success = !(full_content.empty() && full_reasoning.empty());
    if (!resp.success && resp.error_message.empty()) {
        resp.error_message = "Không nhận được phản hồi từ máy chủ AI (" + endpoint + "). Hãy kiểm tra dịch vụ AI hoặc Model ID.";
    }
    resp.content = full_content.empty() ? full_reasoning : full_content;
    return resp;
}

std::vector<std::string> AiClient::fetch_models(const AiConfig& config) {
    std::vector<std::string> models;
    std::string endpoint = config.base_url;
    if (endpoint.back() == '/') endpoint.pop_back();
    if (endpoint.rfind("/chat/completions") != std::string::npos) {
        endpoint = endpoint.substr(0, endpoint.rfind("/chat/completions"));
    }
    if (endpoint.rfind("/models") == std::string::npos) {
        endpoint += "/models";
    }

    std::string auth_header = "";
    if (!config.api_key.empty()) {
        auth_header = "Bearer " + config.api_key;
    }

    std::string raw = http_get(endpoint, auth_header);
    if (raw.empty()) {
        // Fallback for native Ollama port /api/tags if /v1/models fails
        if (config.base_url.find(":11434") != std::string::npos) {
            std::string ollama_tags = config.base_url;
            size_t v1_pos = ollama_tags.find("/v1");
            if (v1_pos != std::string::npos) {
                ollama_tags = ollama_tags.substr(0, v1_pos) + "/api/tags";
                raw = http_get(ollama_tags, "");
            }
        }
    }
    if (raw.empty()) return models;

    try {
        json::Value root = json::parse(raw);
        if (root.contains("data") && root["data"].is_array()) {
            for (size_t i = 0; i < root["data"].size(); ++i) {
                const auto& item = root["data"][i];
                if (item.contains("id") && item["id"].is_string()) {
                    models.push_back(item["id"].as_string());
                } else if (item.contains("name") && item["name"].is_string()) {
                    models.push_back(item["name"].as_string());
                }
            }
        } else if (root.contains("models") && root["models"].is_array()) {
            for (size_t i = 0; i < root["models"].size(); ++i) {
                const auto& item = root["models"][i];
                if (item.contains("name") && item["name"].is_string()) {
                    models.push_back(item["name"].as_string());
                } else if (item.contains("id") && item["id"].is_string()) {
                    models.push_back(item["id"].as_string());
                } else if (item.contains("model") && item["model"].is_string()) {
                    models.push_back(item["model"].as_string());
                }
            }
        }
    } catch (...) {}

    return models;
}

static std::string clean_extracted_command(std::string cmd) {
    // Strip CDATA wrapper if present: <![CDATA[ ... ]]>
    size_t cdata_start = cmd.find("<![CDATA[");
    if (cdata_start != std::string::npos) {
        cmd = cmd.substr(cdata_start + 9);
        size_t cdata_end = cmd.rfind("]]>");
        if (cdata_end != std::string::npos) {
            cmd = cmd.substr(0, cdata_end);
        }
    }
    // Trim
    cmd.erase(0, cmd.find_first_not_of(" \n\r\t"));
    cmd.erase(cmd.find_last_not_of(" \n\r\t") + 1);
    return cmd;
}

std::vector<std::string> AiClient::extract_commands(const std::string& ai_text) {
    std::vector<std::string> commands;

    // Pattern 1: <function ...><param name="command"><![CDATA[...]]></param></function>
    {
        std::regex re("<function[^>]*>[\\s\\S]*?<param[^>]*name=[\"']?(?:command|cmd)[\"']?[^>]*>([\\s\\S]*?)</param>[\\s\\S]*?</function>", std::regex::icase);
        auto it = std::sregex_iterator(ai_text.begin(), ai_text.end(), re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string cmd = clean_extracted_command((*it)[1].str());
            if (!cmd.empty()) commands.push_back(cmd);
        }
    }

    // Pattern 2: Standalone <param name="command">...</param>
    if (commands.empty()) {
        std::regex re("<param[^>]*name=[\"']?(?:command|cmd)[\"']?[^>]*>([\\s\\S]*?)</param>", std::regex::icase);
        auto it = std::sregex_iterator(ai_text.begin(), ai_text.end(), re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string cmd = clean_extracted_command((*it)[1].str());
            if (!cmd.empty()) commands.push_back(cmd);
        }
    }

    // Pattern 3: <execute>...</execute>
    if (commands.empty()) {
        std::regex re("<execute>([\\s\\S]*?)</execute>", std::regex::icase);
        auto it = std::sregex_iterator(ai_text.begin(), ai_text.end(), re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string cmd = clean_extracted_command((*it)[1].str());
            if (!cmd.empty()) commands.push_back(cmd);
        }
    }

    // Pattern 4: <bash>...</bash> or <cmd>...</cmd> or <shell>...</shell> or <exec>...</exec>
    if (commands.empty()) {
        std::regex re("<(?:bash|cmd|shell|exec|run)>([\\s\\S]*?)</(?:bash|cmd|shell|exec|run)>", std::regex::icase);
        auto it = std::sregex_iterator(ai_text.begin(), ai_text.end(), re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string cmd = clean_extracted_command((*it)[1].str());
            if (!cmd.empty()) commands.push_back(cmd);
        }
    }

    // Pattern 5: ```bash\ncommand\n``` or ```sh\ncommand\n``` or ```shell\ncommand\n``` or ```cmd\n...```
    if (commands.empty()) {
        std::regex re("```(?:bash|sh|shell|cmd|powershell)\r?\n([\\s\\S]*?)```", std::regex::icase);
        auto it = std::sregex_iterator(ai_text.begin(), ai_text.end(), re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string cmd = clean_extracted_command((*it)[1].str());
            if (!cmd.empty()) commands.push_back(cmd);
        }
    }

    // Pattern 6: <tool_call> ... {"command": "..."} ... </tool_call>
    if (commands.empty()) {
        std::regex re("<tool_call>([\\s\\S]*?)</tool_call>", std::regex::icase);
        auto it = std::sregex_iterator(ai_text.begin(), ai_text.end(), re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string raw = clean_extracted_command((*it)[1].str());
            try {
                json::Value tc = json::parse(raw);
                if (tc.contains("arguments") && tc["arguments"].contains("command")) {
                    commands.push_back(clean_extracted_command(tc["arguments"]["command"].as_string()));
                } else if (tc.contains("command")) {
                    commands.push_back(clean_extracted_command(tc["command"].as_string()));
                }
            } catch (...) {
                if (!raw.empty()) commands.push_back(raw);
            }
        }
    }

    return commands;
}

} // namespace ai
