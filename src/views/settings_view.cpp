#include "settings_view.hpp"
#include <algorithm>

namespace views {

void SettingsView::render(graphics::D2DRenderer& r, const RECT& rc, float rightPanelW) {
    int chat_left = 48;
    int right_panel_w = (int)rightPanelW;
    int chat_right = rc.right - right_panel_w;
    int chat_w = chat_right - chat_left;

    float cardW = (float)min(chat_w - 64, 720);
    float cardX = (float)chat_left + ((float)chat_w - cardW) / 2.0f;
    float cardY = 64.0f;
    float cardH = 500.0f;

    // Card background & modern subtle border
    r.draw_rounded_box(cardX, cardY, cardW, cardH, 14.0f,
                       graphics::D2DColor(28, 29, 34), graphics::D2DColor(42, 45, 58), 1.0f);

    // Title
    r.draw_text(L"CẤU HÌNH NHÀ CUNG CẤP AI & ĐỘNG CƠ SANDBOX",
                cardX + 26.0f, cardY + 20.0f, cardW - 52.0f, 28.0f,
                L"Segoe UI", 13.0f, true, graphics::D2DColor(241, 245, 249), 0, 0, false);

    // Labels (clean 70px vertical rhythm)
    float labelX = cardX + 26.0f;
    graphics::D2DColor labelCol(226, 232, 240);
    r.draw_text(L"1. Nhà cung cấp AI (Provider):", labelX, cardY + 58.0f, 400.0f, 22.0f, L"Segoe UI", 9.5f, true, labelCol, 0, 0, false);
    r.draw_text(L"2. Endpoint Base URL:", labelX, cardY + 128.0f, 400.0f, 22.0f, L"Segoe UI", 9.5f, true, labelCol, 0, 0, false);
    r.draw_text(L"3. API Key (Bearer Token):", labelX, cardY + 198.0f, 400.0f, 22.0f, L"Segoe UI", 9.5f, true, labelCol, 0, 0, false);
    r.draw_text(L"4. Mô hình AI (Model ID):", labelX, cardY + 268.0f, 400.0f, 22.0f, L"Segoe UI", 9.5f, true, labelCol, 0, 0, false);
    r.draw_text(L"5. Động cơ Sandbox cô lập:", labelX, cardY + 338.0f, 400.0f, 22.0f, L"Segoe UI", 9.5f, true, labelCol, 0, 0, false);

    // Pure Hardware-Accelerated Direct2D Action Buttons (Row Y = cardY + 424, clear 30px below engine combo)
    float btnY = cardY + 424.0f;
    float btnH = 38.0f;

    // Button 1: Test connection
    float btn1X = labelX;
    float btn1W = 160.0f;
    btn_test_rect_ = Gdiplus::RectF(btn1X, btnY, btn1W, btnH);
    r.draw_rounded_box(btn1X, btnY, btn1W, btnH, 8.0f,
                       hover_btn_test_ ? graphics::D2DColor(45, 51, 69) : graphics::D2DColor(32, 35, 46),
                       hover_btn_test_ ? graphics::D2DColor(99, 102, 241) : graphics::D2DColor(55, 60, 78), 1.0f);
    r.draw_text(L"⚡ Kiểm tra kết nối", btn1X, btnY, btn1W, btnH,
                L"Segoe UI", 9.5f, true, graphics::D2DColor(226, 232, 240), 1, 1, false);

    // Button 2: Save config
    float btn2X = btn1X + btn1W + 14.0f;
    float btn2W = 190.0f;
    btn_save_rect_ = Gdiplus::RectF(btn2X, btnY, btn2W, btnH);
    r.draw_rounded_box(btn2X, btnY, btn2W, btnH, 8.0f,
                       hover_btn_save_ ? graphics::D2DColor(76, 29, 149) : graphics::D2DColor(50, 32, 85),
                       hover_btn_save_ ? graphics::D2DColor(168, 85, 247) : graphics::D2DColor(126, 34, 206), 1.0f);
    r.draw_text(L"💾 Lưu & Đặt mặc định", btn2X, btnY, btn2W, btnH,
                L"Segoe UI", 9.5f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);

    // Button 3: Reset Workspace Sandbox
    float btn3X = btn2X + btn2W + 14.0f;
    float btn3W = 160.0f;
    btn_reset_rect_ = Gdiplus::RectF(btn3X, btnY, btn3W, btnH);
    r.draw_rounded_box(btn3X, btnY, btn3W, btnH, 8.0f,
                       hover_btn_reset_ ? graphics::D2DColor(65, 30, 40) : graphics::D2DColor(42, 24, 30),
                       hover_btn_reset_ ? graphics::D2DColor(225, 29, 72) : graphics::D2DColor(120, 35, 50), 1.0f);
    r.draw_text(L"🔄 Reset Sandbox", btn3X, btnY, btn3W, btnH,
                L"Segoe UI", 9.5f, true, graphics::D2DColor(254, 205, 211), 1, 1, false);
}

bool SettingsView::on_mouse_move(float x, float y) {
    bool prev1 = hover_btn_test_;
    bool prev2 = hover_btn_save_;
    bool prev3 = hover_btn_reset_;

    hover_btn_test_ = (btn_test_rect_.Width > 0 &&
                       x >= btn_test_rect_.X && x <= btn_test_rect_.X + btn_test_rect_.Width &&
                       y >= btn_test_rect_.Y && y <= btn_test_rect_.Y + btn_test_rect_.Height);

    hover_btn_save_ = (btn_save_rect_.Width > 0 &&
                       x >= btn_save_rect_.X && x <= btn_save_rect_.X + btn_save_rect_.Width &&
                       y >= btn_save_rect_.Y && y <= btn_save_rect_.Y + btn_save_rect_.Height);

    hover_btn_reset_ = (btn_reset_rect_.Width > 0 &&
                        x >= btn_reset_rect_.X && x <= btn_reset_rect_.X + btn_reset_rect_.Width &&
                        y >= btn_reset_rect_.Y && y <= btn_reset_rect_.Y + btn_reset_rect_.Height);

    return (prev1 != hover_btn_test_ || prev2 != hover_btn_save_ || prev3 != hover_btn_reset_);
}

SettingsAction SettingsView::on_mouse_down(float x, float y) {
    if (btn_test_rect_.Width > 0 &&
        x >= btn_test_rect_.X && x <= btn_test_rect_.X + btn_test_rect_.Width &&
        y >= btn_test_rect_.Y && y <= btn_test_rect_.Y + btn_test_rect_.Height) {
        return SettingsAction::TEST_AI;
    }
    if (btn_save_rect_.Width > 0 &&
        x >= btn_save_rect_.X && x <= btn_save_rect_.X + btn_save_rect_.Width &&
        y >= btn_save_rect_.Y && y <= btn_save_rect_.Y + btn_save_rect_.Height) {
        return SettingsAction::SAVE_CONFIG;
    }
    if (btn_reset_rect_.Width > 0 &&
        x >= btn_reset_rect_.X && x <= btn_reset_rect_.X + btn_reset_rect_.Width &&
        y >= btn_reset_rect_.Y && y <= btn_reset_rect_.Y + btn_reset_rect_.Height) {
        return SettingsAction::RESET_WORKSPACE;
    }
    return SettingsAction::NONE;
}

} // namespace views
