#pragma once
#ifndef CYBERBOX_SETTINGS_VIEW_HPP
#define CYBERBOX_SETTINGS_VIEW_HPP

#include <windows.h>
#include <gdiplus.h>
#include "../graphics/d2d_renderer.hpp"

namespace views {

enum class SettingsAction {
    NONE,
    TEST_AI,
    SAVE_CONFIG,
    RESET_WORKSPACE
};

class SettingsView {
public:
    SettingsView() = default;

    void render(graphics::D2DRenderer& r, const RECT& rc, float rightPanelW);

    bool on_mouse_move(float x, float y);
    SettingsAction on_mouse_down(float x, float y);

    const Gdiplus::RectF& get_btn_test_rect() const { return btn_test_rect_; }
    const Gdiplus::RectF& get_btn_save_rect() const { return btn_save_rect_; }
    const Gdiplus::RectF& get_btn_reset_rect() const { return btn_reset_rect_; }

private:
    Gdiplus::RectF btn_test_rect_;
    Gdiplus::RectF btn_save_rect_;
    Gdiplus::RectF btn_reset_rect_;

    bool hover_btn_test_ = false;
    bool hover_btn_save_ = false;
    bool hover_btn_reset_ = false;
};

} // namespace views

#endif // CYBERBOX_SETTINGS_VIEW_HPP
