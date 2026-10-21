#pragma once
#ifndef CYBERBOX_CHAT_VIEW_HPP
#define CYBERBOX_CHAT_VIEW_HPP

#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include "../common_types.hpp"
#include "../graphics/d2d_renderer.hpp"

namespace views {

enum class ChatClickAction {
    NONE = 0,
    COPY_CODE,
    DOWNLOAD_CODE,
    TOGGLE_REASONING,
    PLUS_CLICKED,
    SEND_CLICKED,
    STOP_CLICKED,
    FLOATING_MUSE_CLICKED
};

struct ChatClickResult {
    ChatClickAction action = ChatClickAction::NONE;
    int block_id = -1;
    std::string lang;
    std::string code;
    size_t msg_idx = 0;
};

class ChatView {
public:
    ChatView() = default;

    void render(
        graphics::D2DRenderer& r,
        const RECT& rc,
        float rightPanelW,
        std::vector<gui::GuiMessage>& messages,
        std::mutex& chatMutex,
        bool isGenerating,
        int scrollY,
        int animFrame,
        float pulsePhase,
        int copiedBlockId,
        int downloadedBlockId,
        bool hasInputText,
        int& outContentHeight
    );

    bool handle_mouse_move(int x, int y);
    ChatClickResult handle_lbutton_down(int x, int y, bool isGenerating);

    void reset_feedback() {
        // Feedback reset hook
    }

private:
    std::vector<gui::CodeBlockHitBox> code_hitboxes_;
    std::vector<gui::ReasoningHitBox> reasoning_hitboxes_;

    Gdiplus::RectF btn_plus_rect_;
    Gdiplus::RectF btn_action_rect_;
    Gdiplus::RectF floating_muse_rect_;

    bool hover_btn_plus_ = false;
    bool hover_btn_action_ = false;
    bool hover_floating_muse_ = false;
};

} // namespace views

#endif // CYBERBOX_CHAT_VIEW_HPP
