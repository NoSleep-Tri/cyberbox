#pragma once
#include <string>
#include <vector>
#include <functional>
#include "simple_json.hpp"

namespace ai {

struct ChatMessage {
    std::string role;    // "system", "user", "assistant", "tool"
    std::string content;
};

struct AiConfig {
    std::string id = "ollama";
    std::string name = "Ollama (Local)";
    std::string base_url = "http://127.0.0.1:11434/v1";
    std::string api_key = "";
    std::string model = "maternion/minicpm5-heretic:2b-q4_K_M";
    double temperature = 0.7;
    int max_tokens = 4096;
    int max_agent_turns = 10;
};

struct AiResponse {
    bool success = false;
    std::string content;
    std::string error_message;
    int prompt_tokens = 0;
    int completion_tokens = 0;
};

struct StreamChunk {
    std::string delta_content;
    std::string delta_reasoning;
    bool is_done = false;
};
using StreamCallback = std::function<void(const StreamChunk&)>;

// Streaming Loop Detector: Prevents repeating token cycles in real-time
class RepetitionDetector {
private:
    std::string history_buffer_;
    size_t ngram_length_ = 30; // Phrases/sentences of 30+ characters
public:
    explicit RepetitionDetector(size_t ngram_len = 30) : ngram_length_(ngram_len) {}

    void reset() {
        history_buffer_.clear();
    }

    bool is_repeating(const std::string& new_chunk) {
        history_buffer_ += new_chunk;

        if (history_buffer_.length() > ngram_length_ * 2) {
            std::string tail = history_buffer_.substr(history_buffer_.length() - ngram_length_);
            std::string body = history_buffer_.substr(0, history_buffer_.length() - ngram_length_);

            // Ignore trivial uniform repetitions (e.g. repeated dashes, spaces, equals)
            bool is_trivial = true;
            char first_c = tail[0];
            for (char c : tail) {
                if (c != first_c && c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                    is_trivial = false;
                    break;
                }
            }
            if (is_trivial) return false;

            if (body.find(tail) != std::string::npos) {
                return true; // Loop detected -> abort stream immediately!
            }
        }
        return false;
    }

    const std::string& get_buffer() const {
        return history_buffer_;
    }
};

class AiClient {
private:
    AiConfig config_;

    // Native WinHTTP request helper
    std::string http_post(const std::string& full_url, const std::string& auth_header, const std::string& json_payload);
    std::string http_get(const std::string& full_url, const std::string& auth_header);

public:
    explicit AiClient(const AiConfig& config = AiConfig());
    
    void set_config(const AiConfig& config);
    AiConfig get_config() const;

    // Chat completion (uses default config)
    AiResponse chat_completion(const std::vector<ChatMessage>& messages);
    // Chat completion (uses custom config for specific agent or provider)
    AiResponse chat_completion(const std::vector<ChatMessage>& messages, const AiConfig& config);

    // Streaming Chat completion (with reasoning/thinking tokens)
    AiResponse chat_completion_stream(const std::vector<ChatMessage>& messages, StreamCallback on_chunk);
    AiResponse chat_completion_stream(const std::vector<ChatMessage>& messages, const AiConfig& config, StreamCallback on_chunk);
    
    // Fetch available model names from provider
    std::vector<std::string> fetch_models(const AiConfig& config);

    // Command extractor helper: extracts commands requested by AI to run in sandbox
    static std::vector<std::string> extract_commands(const std::string& ai_text);
};

} // namespace ai
