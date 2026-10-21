#pragma once
#ifndef CYBERBOX_COMMON_TYPES_HPP
#define CYBERBOX_COMMON_TYPES_HPP

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>

namespace gui {

enum class ActiveTab {
    CHAT = 0,
    ARENA = 1,
    SETTINGS = 2,
    WORKSPACE = 3
};

struct TextPart {
    bool is_code = false;
    std::string lang;
    std::string text;
};

struct CodeBlockHitBox {
    Gdiplus::RectF copy_rect;
    Gdiplus::RectF download_rect;
    std::string lang;
    std::string code;
    int block_id = 0;
};

struct ReasoningHitBox {
    Gdiplus::RectF rect;
    size_t msg_idx = 0;
};

struct GuiMessage {
    std::string sender;           // "user", "muse", "codecraft", "nova", "sandbox", "system"
    std::string sender_name;      // "Tri", "Muse", "CodeCraft", "Nova", "Sandbox"
    std::string role;             // "User", "Kiến trúc sư", "Kỹ sư C++", "Chuyên gia Bảo mật", or emergent role
    std::string text;
    std::string reasoning;        // Live <think> / reasoning stream content
    bool reasoning_collapsed = false; // Toggle to collapse/expand thought stream
    std::string timestamp;
    std::string attachment_title; // Thumbnail header title if user sent attachment
    std::string attachment_sub;   // Thumbnail subtitle if user sent attachment
    std::vector<std::string> executed_cmds;
    COLORREF color = RGB(255, 255, 255);
    bool is_streaming = false;    // True while tokens are actively streaming

    // High-DPI / High-Res Layout & Markdown Cache (avoids re-measuring unchanged messages)
    mutable float cached_col_w = 0.0f;
    mutable size_t cached_text_len = 0xFFFFFFFF;
    mutable size_t cached_reasoning_len = 0xFFFFFFFF;
    mutable size_t cached_cmds_count = 0xFFFFFFFF;
    mutable bool cached_was_streaming = false;
    mutable bool cached_reasoning_collapsed = false;
    mutable float cached_card_h = 0.0f;
    mutable float cached_reasoning_h = 0.0f;
    mutable float cached_user_w = 0.0f;
    mutable float cached_user_h = 0.0f;
    mutable std::vector<TextPart> cached_parts;
};

struct AgentInfo {
    std::string id;
    std::string name;
    std::string role = "Tự phân vai...";
    std::string provider_id = "ollama";
    std::string custom_model = "";
    COLORREF color = RGB(175, 82, 222);
};

struct TodayTaskItem {
    std::string title;
    std::string desc;
    std::string time;
    bool is_done = true;
};

} // namespace gui

#endif // CYBERBOX_COMMON_TYPES_HPP
