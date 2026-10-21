#include "session_modal.hpp"
#include <algorithm>

namespace views {

static std::wstring s2ws(const std::string& raw) {
    if (raw.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), NULL, 0);
    std::wstring w(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), &w[0], size);
    return w;
}

void SessionModal::render(graphics::D2DRenderer& r, const RECT& rc, const std::vector<SessionInfo>& sessions) {
    if (!visible_) return;

    float rW = (float)(rc.right - rc.left);
    float rH = (float)(rc.bottom - rc.top);

    // Semi-transparent backdrop overlay
    r.draw_rect(0.0f, 0.0f, rW, rH, graphics::D2DColor(0, 0, 0, 160));

    // Centered Card
    float cardW = min(680.0f, rW - 40.0f);
    float cardH = min(520.0f, rH - 60.0f);
    float cardX = (rW - cardW) * 0.5f;
    float cardY = max(30.0f, (rH - cardH) * 0.5f);
    modal_card_rect_ = Gdiplus::RectF(cardX, cardY, cardW, cardH);

    // Card background
    r.draw_rounded_box(cardX, cardY, cardW, cardH, 14.0f,
                       graphics::D2DColor(24, 25, 30), graphics::D2DColor(50, 54, 70), 1.2f);

    // Header Title
    r.draw_text(L"📁 QUẢN LÝ PHIÊN TRANH LUẬN (ARENA SESSIONS)",
                cardX + 24.0f, cardY + 20.0f, cardW - 70.0f, 26.0f,
                L"Segoe UI", 12.0f, true, graphics::D2DColor(241, 245, 249), 0, 0, false);

    // Close button (X) at top right
    float closeSz = 28.0f;
    float closeX = cardX + cardW - closeSz - 18.0f;
    float closeY = cardY + 18.0f;
    modal_btn_close_rect_ = Gdiplus::RectF(closeX, closeY, closeSz, closeSz);
    r.draw_rounded_box(closeX, closeY, closeSz, closeSz, 6.0f,
                       hover_btn_close_ ? graphics::D2DColor(60, 25, 35) : graphics::D2DColor(35, 36, 45),
                       graphics::D2DColor(65, 70, 85), 1.0f);
    r.draw_text(L"✕", closeX, closeY, closeSz, closeSz, L"Segoe UI", 10.0f, true,
                hover_btn_close_ ? graphics::D2DColor(254, 205, 211) : graphics::D2DColor(203, 213, 225), 1, 1, false);

    // Subtitle
    r.draw_text(L"Lưu trữ và quản lý phiên trực tiếp trong Sandbox Workspace (tệp JSON & Markdown biên bản):",
                cardX + 24.0f, cardY + 50.0f, cardW - 48.0f, 20.0f,
                L"Segoe UI", 8.5f, false, graphics::D2DColor(148, 163, 184), 0, 0, false);

    // List of Sessions (max 4 visible on current page)
    item_rects_.clear();
    item_load_btn_rects_.clear();
    item_del_btn_rects_.clear();

    float itemY = cardY + 78.0f;
    float itemW = cardW - 48.0f;
    float itemH = 68.0f;

    if (sessions.empty()) {
        r.draw_rounded_box(cardX + 24.0f, itemY, itemW, 120.0f, 8.0f,
                           graphics::D2DColor(20, 21, 26), graphics::D2DColor(40, 44, 56), 1.0f);
        r.draw_text(L"Chưa có phiên thảo luận nào được lưu trong thư mục history/.\nNhấp nút bên dưới để lưu phiên thảo luận hiện tại!",
                    cardX + 36.0f, itemY + 36.0f, itemW - 24.0f, 50.0f,
                    L"Segoe UI", 9.5f, false, graphics::D2DColor(148, 163, 184), 1, 1, false);
    } else {
        int maxShow = min((int)sessions.size(), 4);
        for (int i = 0; i < maxShow; ++i) {
            const auto& s = sessions[i];
            Gdiplus::RectF iRc(cardX + 24.0f, itemY, itemW, itemH);
            item_rects_.push_back(iRc);

            bool isHov = (hover_item_idx_ == i);
            r.draw_rounded_box(iRc.X, iRc.Y, iRc.Width, iRc.Height, 8.0f,
                               isHov ? graphics::D2DColor(32, 34, 44) : graphics::D2DColor(26, 28, 35),
                               graphics::D2DColor(48, 52, 68), 1.0f);

            // Topic title
            std::wstring wTopic = s2ws("🎯 " + s.topic);
            r.draw_text(wTopic, iRc.X + 14.0f, iRc.Y + 8.0f, itemW - 210.0f, 24.0f,
                        L"Segoe UI", 9.5f, true, graphics::D2DColor(241, 245, 249), 0, 0, false);

            // Metadata row: Time, Agent count, message count
            std::wstring meta = s2ws("🕒 " + s.time_str + "  •  🤖 " + std::to_string(s.swarm_size) + " Agent  •  💬 " + std::to_string(s.message_count) + " lượt");
            r.draw_text(meta, iRc.X + 14.0f, iRc.Y + 36.0f, itemW - 210.0f, 20.0f,
                        L"Segoe UI", 8.5f, false, graphics::D2DColor(148, 163, 184), 0, 0, false);

            // Load Button
            float btnLoadW = 86.0f;
            float btnLoadH = 32.0f;
            float btnLoadX = iRc.X + itemW - btnLoadW - 80.0f;
            float btnLoadY = iRc.Y + 18.0f;
            Gdiplus::RectF loadRc(btnLoadX, btnLoadY, btnLoadW, btnLoadH);
            item_load_btn_rects_.push_back(loadRc);

            bool isLoadHov = (hover_load_idx_ == i);
            r.draw_rounded_box(btnLoadX, btnLoadY, btnLoadW, btnLoadH, 6.0f,
                               isLoadHov ? graphics::D2DColor(88, 28, 135) : graphics::D2DColor(55, 25, 95),
                               isLoadHov ? graphics::D2DColor(192, 132, 252) : graphics::D2DColor(147, 51, 234), 1.0f);
            r.draw_text(L"📂 Nạp lại", btnLoadX, btnLoadY, btnLoadW, btnLoadH,
                        L"Segoe UI", 8.5f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);

            // Delete Button
            float btnDelW = 66.0f;
            float btnDelH = 32.0f;
            float btnDelX = iRc.X + itemW - btnDelW - 8.0f;
            float btnDelY = iRc.Y + 18.0f;
            Gdiplus::RectF delRc(btnDelX, btnDelY, btnDelW, btnDelH);
            item_del_btn_rects_.push_back(delRc);

            bool isDelHov = (hover_del_idx_ == i);
            r.draw_rounded_box(btnDelX, btnDelY, btnDelW, btnDelH, 6.0f,
                               isDelHov ? graphics::D2DColor(65, 25, 35) : graphics::D2DColor(42, 24, 30),
                               isDelHov ? graphics::D2DColor(248, 113, 113) : graphics::D2DColor(159, 18, 57), 1.0f);
            r.draw_text(L"🗑️ Xóa", btnDelX, btnDelY, btnDelW, btnDelH,
                        L"Segoe UI", 8.5f, true, graphics::D2DColor(254, 205, 211), 1, 1, false);

            itemY += itemH + 10.0f;
        }
    }

    // Bottom Action Row
    float btmY = cardY + cardH - 52.0f;
    float saveBtnW = 220.0f;
    float saveBtnH = 36.0f;
    modal_btn_save_new_rect_ = Gdiplus::RectF(cardX + 24.0f, btmY, saveBtnW, saveBtnH);
    r.draw_rounded_box(modal_btn_save_new_rect_.X, modal_btn_save_new_rect_.Y, saveBtnW, saveBtnH, 8.0f,
                       hover_btn_save_new_ ? graphics::D2DColor(76, 29, 149) : graphics::D2DColor(50, 32, 85),
                       hover_btn_save_new_ ? graphics::D2DColor(168, 85, 247) : graphics::D2DColor(126, 34, 206), 1.0f);
    r.draw_text(L"💾 Lưu phiên thảo luận hiện tại",
                modal_btn_save_new_rect_.X, modal_btn_save_new_rect_.Y, saveBtnW, saveBtnH,
                L"Segoe UI", 9.0f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);
}

bool SessionModal::handle_mouse_move(int x, int y) {
    if (!visible_) return false;

    bool prevClose = hover_btn_close_;
    bool prevSave = hover_btn_save_new_;
    int prevLoad = hover_load_idx_;
    int prevDel = hover_del_idx_;

    hover_btn_close_ = (x >= modal_btn_close_rect_.X && x <= modal_btn_close_rect_.X + modal_btn_close_rect_.Width &&
                        y >= modal_btn_close_rect_.Y && y <= modal_btn_close_rect_.Y + modal_btn_close_rect_.Height);

    hover_btn_save_new_ = (x >= modal_btn_save_new_rect_.X && x <= modal_btn_save_new_rect_.X + modal_btn_save_new_rect_.Width &&
                           y >= modal_btn_save_new_rect_.Y && y <= modal_btn_save_new_rect_.Y + modal_btn_save_new_rect_.Height);

    hover_load_idx_ = -1;
    for (size_t i = 0; i < item_load_btn_rects_.size(); ++i) {
        const auto& r = item_load_btn_rects_[i];
        if (x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height) {
            hover_load_idx_ = (int)i;
            break;
        }
    }

    hover_del_idx_ = -1;
    for (size_t i = 0; i < item_del_btn_rects_.size(); ++i) {
        const auto& r = item_del_btn_rects_[i];
        if (x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height) {
            hover_del_idx_ = (int)i;
            break;
        }
    }

    return (prevClose != hover_btn_close_ || prevSave != hover_btn_save_new_ ||
            prevLoad != hover_load_idx_ || prevDel != hover_del_idx_);
}

SessionModalAction SessionModal::handle_lbutton_down(int x, int y, int& outSelectedIndex) {
    if (!visible_) return SessionModalAction::NONE;

    if (x >= modal_btn_close_rect_.X && x <= modal_btn_close_rect_.X + modal_btn_close_rect_.Width &&
        y >= modal_btn_close_rect_.Y && y <= modal_btn_close_rect_.Y + modal_btn_close_rect_.Height) {
        visible_ = false;
        return SessionModalAction::CLOSE;
    }

    if (x >= modal_btn_save_new_rect_.X && x <= modal_btn_save_new_rect_.X + modal_btn_save_new_rect_.Width &&
        y >= modal_btn_save_new_rect_.Y && y <= modal_btn_save_new_rect_.Y + modal_btn_save_new_rect_.Height) {
        return SessionModalAction::SAVE_NEW;
    }

    for (size_t i = 0; i < item_load_btn_rects_.size(); ++i) {
        const auto& r = item_load_btn_rects_[i];
        if (x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height) {
            outSelectedIndex = (int)i;
            return SessionModalAction::LOAD_SESSION;
        }
    }

    for (size_t i = 0; i < item_del_btn_rects_.size(); ++i) {
        const auto& r = item_del_btn_rects_[i];
        if (x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height) {
            outSelectedIndex = (int)i;
            return SessionModalAction::DELETE_SESSION;
        }
    }

    // Click outside modal card dismisses it
    if (x < modal_card_rect_.X || x > modal_card_rect_.X + modal_card_rect_.Width ||
        y < modal_card_rect_.Y || y > modal_card_rect_.Y + modal_card_rect_.Height) {
        visible_ = false;
        return SessionModalAction::CLOSE;
    }

    return SessionModalAction::NONE;
}

} // namespace views
