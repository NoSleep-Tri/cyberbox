#include "arena_view.hpp"
#include <cmath>
#include <algorithm>

namespace views {

static std::wstring s2ws(const std::string& raw) {
    if (raw.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), NULL, 0);
    std::wstring w(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), &w[0], size);
    return w;
}

void ArenaView::render(
    graphics::D2DRenderer& r,
    const RECT& rc,
    float rightPanelW,
    const std::string& currentTopic,
    int swarmSize,
    int speedMs,
    const std::vector<gui::AgentInfo>& agents,
    const std::vector<gui::GuiMessage>& messages,
    bool isRunning,
    const std::string& activeSpeaker,
    int scrollY,
    float pulsePhase,
    int& outContentHeight
) {
    int chat_left = 48;
    int right_panel_w = (int)rightPanelW;
    int chat_right = rc.right - right_panel_w;
    int chat_w = chat_right - chat_left;

    // 1. Topic & Swarm Header Card
    float cardX = (float)chat_left + 20.0f;
    float cardW = (float)(chat_w - 40);
    float cardH = 74.0f;
    r.draw_rounded_box(cardX, 52.0f, cardW, cardH, 12.0f,
                       graphics::D2DColor(28, 29, 34), graphics::D2DColor(42, 45, 58), 1.0f);

    // Left side: Title and Topic
    r.draw_text(L"⚡ BẦY AGENT TỰ CHỦ VÔ HẠN (AUTONOMOUS SWARM ARENA)",
                cardX + 16.0f, 57.0f, max(200.0f, cardW - 390.0f), 22.0f,
                L"Segoe UI", 10.5f, true, graphics::D2DColor(241, 245, 249), 0, 0, false);

    std::wstring dispTopic = s2ws("🎯 Chủ đề: " + currentTopic + " [✏️ Nhấp để đổi]");
    r.draw_text(dispTopic,
                cardX + 16.0f, 80.0f, max(200.0f, cardW - 390.0f), 40.0f,
                L"Segoe UI", 9.0f, false, graphics::D2DColor(148, 163, 184), 0, 0, true);

    // Right side Controls Group: Speed toggle, Re-role, Swarm Counter, and Mini Avatars
    float ctrlRight = cardX + cardW - 12.0f;

    // 1. Speed button
    float spdW = 68.0f;
    float spdX = ctrlRight - spdW;
    speed_btn_rect_ = Gdiplus::RectF(spdX, 57.0f, spdW, 28.0f);
    std::wstring spdText = (speedMs <= 1000) ? L"🚀 0.8s" : ((speedMs >= 3000) ? L"🐢 3.0s" : L"⚡ 1.8s");
    r.draw_rounded_box(spdX, 57.0f, spdW, 28.0f, 6.0f,
                       hover_speed_ ? graphics::D2DColor(48, 50, 62) : graphics::D2DColor(34, 36, 45),
                       graphics::D2DColor(58, 62, 78), 1.0f);
    r.draw_text(spdText, spdX, 57.0f, spdW, 28.0f, L"Segoe UI", 8.5f, true, graphics::D2DColor(226, 232, 240), 1, 1, false);

    // 2. Re-role button
    float rolW = 104.0f;
    float rolX = spdX - rolW - 8.0f;
    reset_roles_rect_ = Gdiplus::RectF(rolX, 57.0f, rolW, 28.0f);
    r.draw_rounded_box(rolX, 57.0f, rolW, 28.0f, 6.0f,
                       hover_reset_roles_ ? graphics::D2DColor(62, 38, 92) : graphics::D2DColor(42, 28, 65),
                       graphics::D2DColor(147, 51, 234), 1.0f);
    r.draw_text(L"🔄 Làm mới bầy", rolX, 57.0f, rolW, 28.0f, L"Segoe UI", 8.5f, true, graphics::D2DColor(216, 180, 254), 1, 1, false);

    // 3. Swarm Plus [+] button
    float plusW = 26.0f;
    float plusX = rolX - plusW - 8.0f;
    swarm_plus_rect_ = Gdiplus::RectF(plusX, 57.0f, plusW, 28.0f);
    r.draw_rounded_box(plusX, 57.0f, plusW, 28.0f, 6.0f,
                       hover_swarm_plus_ ? graphics::D2DColor(52, 54, 68) : graphics::D2DColor(36, 38, 48),
                       graphics::D2DColor(60, 64, 80), 1.0f);
    r.draw_text(L"＋", plusX, 57.0f, plusW, 28.0f, L"Segoe UI", 11.0f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);

    // 4. Swarm Counter badge
    float cntW = 76.0f;
    float cntX = plusX - cntW - 4.0f;
    r.draw_rounded_box(cntX, 57.0f, cntW, 28.0f, 6.0f,
                       graphics::D2DColor(20, 21, 27), graphics::D2DColor(45, 48, 62), 1.0f);
    r.draw_text(s2ws("🤖 " + std::to_string(swarmSize) + " Agent"), cntX, 57.0f, cntW, 28.0f,
                L"Segoe UI", 8.5f, true, graphics::D2DColor(241, 245, 249), 1, 1, false);

    // 5. Swarm Minus [-] button
    float minW = 26.0f;
    float minX = cntX - minW - 4.0f;
    swarm_minus_rect_ = Gdiplus::RectF(minX, 57.0f, minW, 28.0f);
    r.draw_rounded_box(minX, 57.0f, minW, 28.0f, 6.0f,
                       hover_swarm_minus_ ? graphics::D2DColor(52, 54, 68) : graphics::D2DColor(36, 38, 48),
                       graphics::D2DColor(60, 64, 80), 1.0f);
    r.draw_text(L"－", minX, 57.0f, minW, 28.0f, L"Segoe UI", 11.0f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);

    // Row 2 of right group: Mini Avatars of active swarm members
    float avRowY = 93.0f;
    float curAvX = minX;
    for (size_t a = 0; a < agents.size() && a < 12; ++a) {
        const auto& ag = agents[a];
        bool isAct = (isRunning && activeSpeaker == ag.name);
        graphics::D2DColor agCol(ag.color);
        if (isAct) {
            r.draw_ellipse(curAvX + 9.0f, avRowY + 9.0f, 11.0f, 11.0f, agCol, 1.6f);
        }
        r.fill_ellipse(curAvX + 9.0f, avRowY + 9.0f, 9.0f, 9.0f, agCol);
        std::wstring initW = s2ws(ag.name);
        if (!initW.empty()) initW = initW.substr(0, 1);
        r.draw_text(initW, curAvX, avRowY, 18.0f, 18.0f, L"Segoe UI", 8.0f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);
        curAvX += 22.0f;
    }

    if (isRunning && !activeSpeaker.empty()) {
        r.draw_text(s2ws("⚡ Đang phát biểu: " + activeSpeaker),
                    curAvX + 8.0f, avRowY - 1.0f, 160.0f, 20.0f,
                    L"Segoe UI", 8.5f, true, graphics::D2DColor(74, 222, 128), 0, 1, false);
    }

    // 2. Arena Scrollable Message View
    r.set_clip((float)chat_left, 134.0f, (float)chat_w, (float)(rc.bottom - 134 - 72));

    int curY = 142 - scrollY;
    float colW = (float)min(chat_w - 60, 680);
    float colX = (float)chat_left + ((float)chat_w - colW) / 2.0f;

    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        bool isUser = (msg.sender == "user");
        bool isSys = (msg.sender == "sys");

        float pad = 16.0f;
        std::wstring textW = s2ws(msg.text);
        D2D1_SIZE_F textSz = r.measure_text(textW, colW - pad * 2.0f, L"Segoe UI", 9.8f, false, true, 20.0f);
        float bubbleH = max(42.0f, textSz.height + 36.0f);

        if (curY + bubbleH >= 100 && curY <= rc.bottom - 60) {
            graphics::D2DColor bubbleBg = isUser ? graphics::D2DColor(24, 30, 42) :
                                          (isSys ? graphics::D2DColor(28, 22, 38) : graphics::D2DColor(24, 25, 30));
            graphics::D2DColor borderCol = isUser ? graphics::D2DColor(45, 60, 90) :
                                           (isSys ? graphics::D2DColor(147, 51, 234) : graphics::D2DColor(40, 42, 50));
            r.draw_rounded_box(colX, (float)curY, colW, bubbleH, 10.0f, bubbleBg, borderCol, 1.0f);

            graphics::D2DColor badgeCol(msg.color);
            std::wstring senderHeader;
            if (!msg.role.empty()) {
                senderHeader = s2ws(msg.sender_name + " • " + msg.role + " • " + msg.timestamp);
            } else {
                senderHeader = s2ws(msg.sender_name + " • " + msg.timestamp);
            }
            r.draw_text(senderHeader, colX + pad, (float)curY + 8.0f, colW - pad * 2.0f, 18.0f,
                        L"Segoe UI", 8.8f, true, badgeCol, 0, 0, false);

            // Message Text Body
            r.draw_text(textW, colX + pad, (float)curY + 28.0f, colW - pad * 2.0f, bubbleH - 32.0f,
                        L"Segoe UI", 9.8f, false, graphics::D2DColor(226, 232, 240), 0, 0, true, 20.0f);
        }

        curY += (int)bubbleH + 12;
    }

    outContentHeight = curY + scrollY - 142;
    r.clear_clip();

    // 3. Bottom Interjection Capsule
    float arenaBarX = (float)chat_left + 20.0f;
    float arenaBarW = (float)(chat_w - 40);
    float arenaBarY = (float)(rc.bottom - 56);
    r.draw_rounded_box(arenaBarX, arenaBarY, arenaBarW, 44.0f, 10.0f,
                       graphics::D2DColor(24, 25, 30), graphics::D2DColor(42, 45, 58), 1.0f);
    r.draw_text(L"🗣️ CAN THIỆP:", arenaBarX + 14.0f, arenaBarY + 12.0f, 110.0f, 20.0f,
                L"Segoe UI", 8.5f, true, graphics::D2DColor(168, 85, 247), 0, 1, false);

    // Interject button
    float btnW = 110.0f;
    float btnX = arenaBarX + arenaBarW - btnW - 8.0f;
    r.draw_rounded_box(btnX, arenaBarY + 7.0f, btnW, 30.0f, 8.0f,
                       graphics::D2DColor(109, 40, 217), graphics::D2DColor(147, 51, 234), 1.0f);
    r.draw_text(L"Can thiệp", btnX, arenaBarY + 7.0f, btnW, 30.0f,
                L"Segoe UI", 9.5f, true, graphics::D2DColor(241, 245, 249), 1, 1, false);
}

bool ArenaView::handle_mouse_move(int x, int y) {
    bool prev_sp = hover_speed_;
    bool prev_ro = hover_reset_roles_;
    bool prev_pl = hover_swarm_plus_;
    bool prev_mi = hover_swarm_minus_;

    hover_speed_ = false;
    hover_reset_roles_ = false;
    hover_swarm_plus_ = false;
    hover_swarm_minus_ = false;

    if (speed_btn_rect_.Width > 0 &&
        x >= speed_btn_rect_.X && x <= speed_btn_rect_.X + speed_btn_rect_.Width &&
        y >= speed_btn_rect_.Y && y <= speed_btn_rect_.Y + speed_btn_rect_.Height) {
        hover_speed_ = true;
    }
    if (reset_roles_rect_.Width > 0 &&
        x >= reset_roles_rect_.X && x <= reset_roles_rect_.X + reset_roles_rect_.Width &&
        y >= reset_roles_rect_.Y && y <= reset_roles_rect_.Y + reset_roles_rect_.Height) {
        hover_reset_roles_ = true;
    }
    if (swarm_plus_rect_.Width > 0 &&
        x >= swarm_plus_rect_.X && x <= swarm_plus_rect_.X + swarm_plus_rect_.Width &&
        y >= swarm_plus_rect_.Y && y <= swarm_plus_rect_.Y + swarm_plus_rect_.Height) {
        hover_swarm_plus_ = true;
    }
    if (swarm_minus_rect_.Width > 0 &&
        x >= swarm_minus_rect_.X && x <= swarm_minus_rect_.X + swarm_minus_rect_.Width &&
        y >= swarm_minus_rect_.Y && y <= swarm_minus_rect_.Y + swarm_minus_rect_.Height) {
        hover_swarm_minus_ = true;
    }

    return (prev_sp != hover_speed_ || prev_ro != hover_reset_roles_ ||
            prev_pl != hover_swarm_plus_ || prev_mi != hover_swarm_minus_);
}

ArenaClickAction ArenaView::handle_lbutton_down(int x, int y, int chatLeft, int chatRight) {
    if (speed_btn_rect_.Width > 0 &&
        x >= speed_btn_rect_.X && x <= speed_btn_rect_.X + speed_btn_rect_.Width &&
        y >= speed_btn_rect_.Y && y <= speed_btn_rect_.Y + speed_btn_rect_.Height) {
        return ArenaClickAction::SPEED_TOGGLE;
    }
    if (reset_roles_rect_.Width > 0 &&
        x >= reset_roles_rect_.X && x <= reset_roles_rect_.X + reset_roles_rect_.Width &&
        y >= reset_roles_rect_.Y && y <= reset_roles_rect_.Y + reset_roles_rect_.Height) {
        return ArenaClickAction::RESET_ROLES;
    }
    if (swarm_plus_rect_.Width > 0 &&
        x >= swarm_plus_rect_.X && x <= swarm_plus_rect_.X + swarm_plus_rect_.Width &&
        y >= swarm_plus_rect_.Y && y <= swarm_plus_rect_.Y + swarm_plus_rect_.Height) {
        return ArenaClickAction::SWARM_PLUS;
    }
    if (swarm_minus_rect_.Width > 0 &&
        x >= swarm_minus_rect_.X && x <= swarm_minus_rect_.X + swarm_minus_rect_.Width &&
        y >= swarm_minus_rect_.Y && y <= swarm_minus_rect_.Y + swarm_minus_rect_.Height) {
        return ArenaClickAction::SWARM_MINUS;
    }

    // Clicking on topic card text area to edit topic
    if (y >= 52 && y <= 126 && x >= chatLeft + 20 && x <= chatRight - 350) {
        return ArenaClickAction::CHANGE_TOPIC;
    }

    return ArenaClickAction::NONE;
}

} // namespace views
