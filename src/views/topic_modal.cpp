#include "topic_modal.hpp"
#include <algorithm>

namespace views {

void TopicModal::render(graphics::D2DRenderer& r, const RECT& rc, int swarmSize) {
    if (!visible_) return;

    float rW = (float)rc.right;
    float rH = (float)rc.bottom;

    // 1. Full-screen dark translucent backdrop
    r.fill_rect(0.0f, 0.0f, rW, rH, graphics::D2DColor(10, 11, 15, 215));

    // 2. Centered Modal Card
    float cardW = min(660.0f, rW - 40.0f);
    float cardH = 430.0f;
    float cardX = (rW - cardW) * 0.5f;
    float cardY = max(36.0f, (rH - cardH) * 0.5f);
    modal_card_rect_ = Gdiplus::RectF(cardX, cardY, cardW, cardH);

    r.draw_rounded_box(cardX, cardY, cardW, cardH, 16.0f,
                       graphics::D2DColor(24, 25, 31), graphics::D2DColor(88, 52, 138), 1.5f);

    // Glowing top bar accent
    r.draw_line(cardX + 24.0f, cardY + 1.0f, cardX + cardW - 24.0f, cardY + 1.0f,
                graphics::D2DColor(168, 85, 247), 2.0f);

    // Header
    r.draw_text(L"✏️ THIẾT LẬP CHỦ ĐỀ & QUY MÔ BẦY AGENT (SWARM)",
                cardX + 24.0f, cardY + 18.0f, cardW - 48.0f, 26.0f,
                L"Segoe UI", 12.0f, true, graphics::D2DColor(241, 245, 249), 0, 0, false);

    r.draw_text(L"Nhập chủ đề tùy ý. Bầy agent hoàn toàn không bị gán vai trò định sẵn, tự do tranh luận độc lập.",
                cardX + 24.0f, cardY + 46.0f, cardW - 48.0f, 20.0f,
                L"Segoe UI", 9.0f, false, graphics::D2DColor(148, 163, 184), 0, 0, false);

    // Topic Label
    r.draw_text(L"CHỦ ĐỀ TRANH LUẬN MỚI:",
                cardX + 24.0f, cardY + 74.0f, 250.0f, 18.0f,
                L"Segoe UI", 8.5f, true, graphics::D2DColor(192, 132, 252), 0, 0, false);

    // Inset background for topic input edit control
    r.draw_rounded_box(cardX + 22.0f, cardY + 96.0f, cardW - 44.0f, 68.0f, 8.0f,
                       graphics::D2DColor(18, 19, 24), graphics::D2DColor(52, 55, 72), 1.0f);

    // Quick presets section
    r.draw_text(L"💡 GỢI Ý CHỦ ĐỀ HOT (Nhấp để nạp nhanh):",
                cardX + 24.0f, cardY + 176.0f, 350.0f, 18.0f,
                L"Segoe UI", 8.5f, true, graphics::D2DColor(148, 163, 184), 0, 0, false);

    const wchar_t* presetLabels[4] = {
        L"🛡️ Vượt Sandbox & Air-Gap",
        L"📚 Phương pháp học siêu tốc",
        L"⚡ C++ Direct2D vs Web/Electron",
        L"🤖 Bầy đàn AI Tự Trị Độc Lập"
    };

    float pW = (cardW - 44.0f - 12.0f) * 0.5f;
    for (int i = 0; i < 4; ++i) {
        float px = cardX + 22.0f + (i % 2) * (pW + 12.0f);
        float py = cardY + 200.0f + (i / 2) * 36.0f;
        modal_preset_rects_[i] = Gdiplus::RectF(px, py, pW, 30.0f);

        bool isHov = (hover_modal_preset_ == i);
        graphics::D2DColor bg = isHov ? graphics::D2DColor(45, 48, 62) : graphics::D2DColor(30, 32, 40);
        graphics::D2DColor brd = isHov ? graphics::D2DColor(168, 85, 247) : graphics::D2DColor(50, 54, 68);
        r.draw_rounded_box(px, py, pW, 30.0f, 6.0f, bg, brd, 1.0f);
        r.draw_text(presetLabels[i], px, py, pW, 30.0f,
                    L"Segoe UI", 8.5f, false, graphics::D2DColor(226, 232, 240), 1, 1, false);
    }

    // Swarm size row
    float swarmY = cardY + 284.0f;
    r.draw_text(L"QUY MÔ BẦY ĐÀN (SPAM AGENTS):",
                cardX + 24.0f, swarmY + 6.0f, 220.0f, 24.0f,
                L"Segoe UI", 8.5f, true, graphics::D2DColor(148, 163, 184), 0, 1, false);

    float cntX = cardX + 240.0f;
    // [-] button
    modal_swarm_minus_rect_ = Gdiplus::RectF(cntX, swarmY, 32.0f, 32.0f);
    r.draw_rounded_box(cntX, swarmY, 32.0f, 32.0f, 6.0f,
                       hover_btn_swarm_minus_ ? graphics::D2DColor(52, 54, 66) : graphics::D2DColor(34, 36, 45),
                       graphics::D2DColor(60, 64, 80), 1.0f);
    r.draw_text(L"－", cntX, swarmY, 32.0f, 32.0f, L"Segoe UI", 12.0f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);

    // Count badge
    r.draw_rounded_box(cntX + 38.0f, swarmY, 96.0f, 32.0f, 6.0f,
                       graphics::D2DColor(18, 19, 24), graphics::D2DColor(52, 55, 72), 1.0f);
    std::wstring cntStr = L"🤖 " + std::to_wstring(swarmSize) + L" Agent";
    r.draw_text(cntStr, cntX + 38.0f, swarmY, 96.0f, 32.0f, L"Segoe UI", 9.5f, true, graphics::D2DColor(241, 245, 249), 1, 1, false);

    // [+] button
    modal_swarm_plus_rect_ = Gdiplus::RectF(cntX + 140.0f, swarmY, 32.0f, 32.0f);
    r.draw_rounded_box(cntX + 140.0f, swarmY, 32.0f, 32.0f, 6.0f,
                       hover_btn_swarm_plus_ ? graphics::D2DColor(52, 54, 66) : graphics::D2DColor(34, 36, 45),
                       graphics::D2DColor(60, 64, 80), 1.0f);
    r.draw_text(L"＋", cntX + 140.0f, swarmY, 32.0f, 32.0f, L"Segoe UI", 12.0f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);

    // Reset roles button
    modal_reset_roles_rect_ = Gdiplus::RectF(cntX + 184.0f, swarmY, 130.0f, 32.0f);
    r.draw_rounded_box(cntX + 184.0f, swarmY, 130.0f, 32.0f, 6.0f,
                       hover_btn_reset_roles_ ? graphics::D2DColor(60, 40, 90) : graphics::D2DColor(38, 28, 60),
                       graphics::D2DColor(147, 51, 234), 1.0f);
    r.draw_text(L"🔄 Làm mới bầy", cntX + 184.0f, swarmY, 130.0f, 32.0f, L"Segoe UI", 9.0f, true, graphics::D2DColor(216, 180, 254), 1, 1, false);

    // Bottom modal buttons
    float btnY = cardY + cardH - 52.0f;
    modal_btn_cancel_rect_ = Gdiplus::RectF(cardX + cardW - 270.0f, btnY, 110.0f, 36.0f);
    r.draw_rounded_box(cardX + cardW - 270.0f, btnY, 110.0f, 36.0f, 8.0f,
                       hover_modal_cancel_ ? graphics::D2DColor(48, 50, 62) : graphics::D2DColor(32, 34, 42),
                       graphics::D2DColor(55, 58, 72), 1.0f);
    r.draw_text(L"Hủy bỏ", cardX + cardW - 270.0f, btnY, 110.0f, 36.0f, L"Segoe UI", 9.5f, false, graphics::D2DColor(203, 213, 225), 1, 1, false);

    modal_btn_apply_rect_ = Gdiplus::RectF(cardX + cardW - 148.0f, btnY, 126.0f, 36.0f);
    r.draw_rounded_box(cardX + cardW - 148.0f, btnY, 126.0f, 36.0f, 8.0f,
                       hover_modal_apply_ ? graphics::D2DColor(124, 58, 237) : graphics::D2DColor(109, 40, 217),
                       graphics::D2DColor(168, 85, 247), 1.0f);
    r.draw_text(L"✓ Áp dụng ngay", cardX + cardW - 148.0f, btnY, 126.0f, 36.0f, L"Segoe UI", 9.5f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);
}

bool TopicModal::handle_mouse_move(int x, int y) {
    if (!visible_) return false;

    bool prev_apply = hover_modal_apply_;
    bool prev_cancel = hover_modal_cancel_;
    int prev_preset = hover_modal_preset_;
    bool prev_minus = hover_btn_swarm_minus_;
    bool prev_plus = hover_btn_swarm_plus_;
    bool prev_roles = hover_btn_reset_roles_;

    hover_modal_apply_ = false;
    hover_modal_cancel_ = false;
    hover_modal_preset_ = -1;
    hover_btn_swarm_minus_ = false;
    hover_btn_swarm_plus_ = false;
    hover_btn_reset_roles_ = false;

    if (x >= modal_btn_apply_rect_.X && x <= modal_btn_apply_rect_.X + modal_btn_apply_rect_.Width &&
        y >= modal_btn_apply_rect_.Y && y <= modal_btn_apply_rect_.Y + modal_btn_apply_rect_.Height) {
        hover_modal_apply_ = true;
    } else if (x >= modal_btn_cancel_rect_.X && x <= modal_btn_cancel_rect_.X + modal_btn_cancel_rect_.Width &&
               y >= modal_btn_cancel_rect_.Y && y <= modal_btn_cancel_rect_.Y + modal_btn_cancel_rect_.Height) {
        hover_modal_cancel_ = true;
    }

    for (int i = 0; i < 4; ++i) {
        if (x >= modal_preset_rects_[i].X && x <= modal_preset_rects_[i].X + modal_preset_rects_[i].Width &&
            y >= modal_preset_rects_[i].Y && y <= modal_preset_rects_[i].Y + modal_preset_rects_[i].Height) {
            hover_modal_preset_ = i;
            break;
        }
    }

    if (x >= modal_swarm_minus_rect_.X && x <= modal_swarm_minus_rect_.X + modal_swarm_minus_rect_.Width &&
        y >= modal_swarm_minus_rect_.Y && y <= modal_swarm_minus_rect_.Y + modal_swarm_minus_rect_.Height) {
        hover_btn_swarm_minus_ = true;
    } else if (x >= modal_swarm_plus_rect_.X && x <= modal_swarm_plus_rect_.X + modal_swarm_plus_rect_.Width &&
               y >= modal_swarm_plus_rect_.Y && y <= modal_swarm_plus_rect_.Y + modal_swarm_plus_rect_.Height) {
        hover_btn_swarm_plus_ = true;
    } else if (x >= modal_reset_roles_rect_.X && x <= modal_reset_roles_rect_.X + modal_reset_roles_rect_.Width &&
               y >= modal_reset_roles_rect_.Y && y <= modal_reset_roles_rect_.Y + modal_reset_roles_rect_.Height) {
        hover_btn_reset_roles_ = true;
    }

    return (prev_apply != hover_modal_apply_ || prev_cancel != hover_modal_cancel_ ||
            prev_preset != hover_modal_preset_ || prev_minus != hover_btn_swarm_minus_ ||
            prev_plus != hover_btn_swarm_plus_ || prev_roles != hover_btn_reset_roles_);
}

ModalAction TopicModal::handle_lbutton_down(int x, int y, int& outPresetIdx) {
    if (!visible_) return ModalAction::NONE;

    if (x >= modal_btn_apply_rect_.X && x <= modal_btn_apply_rect_.X + modal_btn_apply_rect_.Width &&
        y >= modal_btn_apply_rect_.Y && y <= modal_btn_apply_rect_.Y + modal_btn_apply_rect_.Height) {
        return ModalAction::APPLY;
    }

    if (x >= modal_btn_cancel_rect_.X && x <= modal_btn_cancel_rect_.X + modal_btn_cancel_rect_.Width &&
        y >= modal_btn_cancel_rect_.Y && y <= modal_btn_cancel_rect_.Y + modal_btn_cancel_rect_.Height) {
        return ModalAction::CANCEL;
    }

    for (int i = 0; i < 4; ++i) {
        if (x >= modal_preset_rects_[i].X && x <= modal_preset_rects_[i].X + modal_preset_rects_[i].Width &&
            y >= modal_preset_rects_[i].Y && y <= modal_preset_rects_[i].Y + modal_preset_rects_[i].Height) {
            outPresetIdx = i;
            return ModalAction::PRESET_SELECTED;
        }
    }

    if (x >= modal_swarm_minus_rect_.X && x <= modal_swarm_minus_rect_.X + modal_swarm_minus_rect_.Width &&
        y >= modal_swarm_minus_rect_.Y && y <= modal_swarm_minus_rect_.Y + modal_swarm_minus_rect_.Height) {
        return ModalAction::SWARM_MINUS;
    }

    if (x >= modal_swarm_plus_rect_.X && x <= modal_swarm_plus_rect_.X + modal_swarm_plus_rect_.Width &&
        y >= modal_swarm_plus_rect_.Y && y <= modal_swarm_plus_rect_.Y + modal_swarm_plus_rect_.Height) {
        return ModalAction::SWARM_PLUS;
    }

    if (x >= modal_reset_roles_rect_.X && x <= modal_reset_roles_rect_.X + modal_reset_roles_rect_.Width &&
        y >= modal_reset_roles_rect_.Y && y <= modal_reset_roles_rect_.Y + modal_reset_roles_rect_.Height) {
        return ModalAction::RESET_ROLES;
    }

    // Clicking outside modal card
    if (x < modal_card_rect_.X || x > modal_card_rect_.X + modal_card_rect_.Width ||
        y < modal_card_rect_.Y || y > modal_card_rect_.Y + modal_card_rect_.Height) {
        return ModalAction::DISMISS_OUTSIDE;
    }

    return ModalAction::NONE;
}

} // namespace views
