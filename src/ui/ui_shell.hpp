#pragma once
#include "ui_element.hpp"
#include "ui_layout.hpp"
#include "ui_components.hpp"
#include "ui_theme.hpp"
#include <string>
#include <functional>
#include <cmath>

namespace cyber_ui {

// Vector icon drawing delegate function
using DrawVectorIconFunc = std::function<void(UIRenderContext& ctx, int icon_idx, float cx, float cy, const UIColor& col)>;

class UIDockRail : public UIElement {
public:
    UIDockRail() {
        width_ = 48.0f;
        background_color_ = UIColor::from_rgb(16, 17, 20); // #101114
        border_color_ = UIColor::from_rgb(32, 33, 39);     // #202127
        border_width_ = 1.0f;
    }

    UIDockRail* set_active_icon(int icon_idx) {
        active_icon_ = icon_idx;
        update_target_indicator_y();
        return this;
    }

    int active_icon() const { return active_icon_; }

    UIDockRail* set_vector_icon_drawer(DrawVectorIconFunc drawer) {
        icon_drawer_ = drawer;
        return this;
    }

    void set_on_icon_click(std::function<void(int)> callback) {
        on_icon_click_ = callback;
    }

    void set_on_toggle_sidebar(std::function<void()> callback) {
        on_toggle_sidebar_ = callback;
    }

    float get_dock_start_y(float client_h) const {
        float available_h = client_h - 50.0f - 110.0f;
        float total_icons_h = 6.0f * 36.0f + 5.0f * 10.0f; // 266px
        float top_margin = (std::max)(12.0f, (available_h - total_icons_h) * 0.5f);
        return 50.0f + top_margin;
    }

    float get_dock_icon_y(int icon_idx, float client_h) const {
        if (icon_idx >= 0 && icon_idx < 6) {
            return get_dock_start_y(client_h) + (float)icon_idx * 46.0f;
        } else if (icon_idx == 6) {
            return client_h - 96.0f;
        } else if (icon_idx == 7) {
            return client_h - 50.0f;
        }
        return 60.0f;
    }

    void update_target_indicator_y() {
        if (bounds_.height > 0.0f) {
            indicator_target_y_ = get_dock_icon_y(active_icon_, bounds_.height);
            if (indicator_y_ <= 0.0f) {
                indicator_y_ = indicator_target_y_;
            }
        }
    }

    void layout(const UIRect& bounds) override {
        bounds_ = bounds;
        update_target_indicator_y();
    }

    void update_animation(float delta_time) override {
        UIElement::update_animation(delta_time);

        // 120 FPS Sub-pixel exponential decay indicator gliding
        float speed = 18.0f;
        indicator_y_ += (indicator_target_y_ - indicator_y_) * (1.0f - std::exp(-speed * delta_time));
        if (std::abs(indicator_target_y_ - indicator_y_) < 0.1f) {
            indicator_y_ = indicator_target_y_;
        }
    }

    std::shared_ptr<UIElement> hit_test(float x, float y) override {
        if (!visible_ || !enabled_) return nullptr;
        if (bounds_.contains(x, y)) {
            return shared_from_this();
        }
        return nullptr;
    }

    void on_mouse_enter() override {
        UIElement::on_mouse_enter();
    }

    void on_mouse_leave() override {
        UIElement::on_mouse_leave();
        hover_icon_ = -1;
    }

    int get_icon_at_pos(float y) const {
        float start_y = get_dock_start_y(bounds_.height);
        float cur_y = start_y;
        for (int i = 0; i < 6; ++i) {
            if (y >= cur_y && y <= cur_y + 36.0f) return i;
            cur_y += 46.0f;
        }
        float set_y = get_dock_icon_y(6, bounds_.height);
        float tog_y = get_dock_icon_y(7, bounds_.height);
        if (y >= set_y && y <= set_y + 36.0f) return 6;
        if (y >= tog_y && y <= tog_y + 36.0f) return 7;
        return -1;
    }

    bool on_mouse_click(float x, float y, int button) override {
        if (button != 0) return false;
        int icon_idx = get_icon_at_pos(y);
        if (icon_idx >= 0) {
            if (icon_idx == 7) {
                if (on_toggle_sidebar_) on_toggle_sidebar_();
            } else {
                active_icon_ = icon_idx;
                update_target_indicator_y();
                if (on_icon_click_) on_icon_click_(icon_idx);
            }
            return true;
        }
        return false;
    }

    void render_self(UIRenderContext& ctx) override {
        // Draw dock rail background
        ctx.fill_rect(bounds_, background_color_);

        // Draw vertical right divider line
        ctx.draw_line(
            UIPoint(bounds_.right() - 0.5f, bounds_.top()),
            UIPoint(bounds_.right() - 0.5f, bounds_.bottom()),
            border_color_,
            1.0f
        );

        // 1. Draw smoothly gliding active indicator pill
        if (active_icon_ >= 0 && active_icon_ <= 6) {
            UIRect glide_box(6.0f, indicator_y_, 36.0f, 36.0f);
            UIColor glide_bg = UIColor::from_rgb(38, 39, 46);
            UIColor glide_border = UIColor::from_rgb(52, 54, 64);
            ctx.fill_rounded_rect(glide_box, 18.0f, glide_bg);
            ctx.draw_rounded_rect(glide_box, 18.0f, glide_border, 1.0f);

            // Subtle neon violet accent indicator LED on the edge
            UIRect led_rect(0.0f, indicator_y_ + 10.0f, 2.5f, 16.0f);
            ctx.fill_rect(led_rect, UIColor::from_rgb(168, 85, 247));
        }

        // 2. Draw 6 centered navigation icons
        float start_y = get_dock_start_y(bounds_.height);
        float cur_y = start_y;
        for (int i = 0; i < 6; ++i) {
            UIRect box(6.0f, cur_y, 36.0f, 36.0f);
            bool is_active = (active_icon_ == i);
            bool is_hover = (hover_icon_ == i && !is_active);

            if (is_hover) {
                ctx.fill_rounded_rect(box, 18.0f, UIColor::from_rgb(28, 29, 35));
            }

            UIColor icon_col = is_active ? UIColor::from_rgb(240, 242, 248) :
                               (is_hover ? UIColor::from_rgb(200, 205, 215) : UIColor::from_rgb(142, 146, 158));

            if (icon_drawer_) {
                icon_drawer_(ctx, i, 24.0f, cur_y + 18.0f, icon_col);
            }
            cur_y += 46.0f;
        }

        // 3. Draw bottom icons (6: Settings, 7: Sidebar toggle)
        float set_y = get_dock_icon_y(6, bounds_.height);
        UIRect set_box(6.0f, set_y, 36.0f, 36.0f);
        bool set_act = (active_icon_ == 6);
        bool set_hov = (hover_icon_ == 6 && !set_act);
        if (set_hov) {
            ctx.fill_rounded_rect(set_box, 18.0f, UIColor::from_rgb(34, 35, 42));
        }
        if (icon_drawer_) {
            UIColor set_col = set_act ? UIColor::from_rgb(240, 242, 248) : UIColor::from_rgb(142, 146, 158);
            icon_drawer_(ctx, 6, 24.0f, set_y + 18.0f, set_col);
        }

        float tog_y = get_dock_icon_y(7, bounds_.height);
        UIRect tog_box(6.0f, tog_y, 36.0f, 36.0f);
        if (hover_icon_ == 7) {
            ctx.fill_rounded_rect(tog_box, 18.0f, UIColor::from_rgb(34, 35, 42));
        }
        if (icon_drawer_) {
            UIColor tog_col = (is_sidebar_open_) ? UIColor::from_rgb(210, 214, 224) : UIColor::from_rgb(142, 146, 158);
            icon_drawer_(ctx, 7, 24.0f, tog_y + 18.0f, tog_col);
        }
    }

    void set_sidebar_open(bool open) { is_sidebar_open_ = open; }
    void set_hover_icon(int idx) { hover_icon_ = idx; }

private:
    int active_icon_ = 0;
    int hover_icon_ = -1;
    float indicator_y_ = 0.0f;
    float indicator_target_y_ = 0.0f;
    bool is_sidebar_open_ = true;

    DrawVectorIconFunc icon_drawer_;
    std::function<void(int)> on_icon_click_;
    std::function<void()> on_toggle_sidebar_;
};

class UITopBar : public UIElement {
public:
    UITopBar() {
        height_ = 50.0f;
        padding_ = UIPadding(10.0f, 16.0f);
    }

    UITopBar* set_tab_title(const std::wstring& title) {
        tab_title_ = title;
        return this;
    }

    UITopBar* set_security_status(bool is_online) {
        is_online_ = is_online;
        return this;
    }

    UITopBar* set_show_new_chat_button(bool show) {
        show_new_chat_ = show;
        return this;
    }

    void set_on_new_chat(std::function<void()> cb) { on_new_chat_ = cb; }
    void set_on_toggle_security(std::function<void()> cb) { on_toggle_security_ = cb; }

    void render_self(UIRenderContext& ctx) override {
        // 1. Top-Left Pill Button: Current Tab Indicator
        float title_w = (tab_title_ == L"Đấu trường" || tab_title_ == L"Workspace") ? 116.0f : 100.0f;
        UIRect title_rect(bounds_.x + 12.0f, bounds_.y + 10.0f, title_w, 30.0f);
        UIColor title_bg = is_title_hovered_ ? UIColor::from_rgb(48, 50, 58) : UIColor::from_rgb(38, 39, 45);
        ctx.fill_rounded_rect(title_rect, 15.0f, title_bg);
        ctx.draw_rounded_rect(title_rect, 15.0f, UIColor::from_rgb(52, 54, 62), 1.0f);

        // Icon 3 lines
        UIColor ic_col = UIColor::from_rgb(210, 214, 222);
        ctx.draw_line(UIPoint(title_rect.x + 12.0f, title_rect.y + 11.0f), UIPoint(title_rect.x + 22.0f, title_rect.y + 11.0f), ic_col, 1.5f);
        ctx.draw_line(UIPoint(title_rect.x + 12.0f, title_rect.y + 15.0f), UIPoint(title_rect.x + 20.0f, title_rect.y + 15.0f), ic_col, 1.5f);
        ctx.draw_line(UIPoint(title_rect.x + 12.0f, title_rect.y + 19.0f), UIPoint(title_rect.x + 22.0f, title_rect.y + 19.0f), ic_col, 1.5f);

        // 2. Right Actions
        float cur_right = bounds_.right() - 14.0f;

        // In Chat Tab: "+ Mới" button
        if (show_new_chat_) {
            float new_w = 60.0f;
            cur_right -= new_w;
            new_chat_rect_ = UIRect(cur_right, bounds_.y + 10.0f, new_w, 30.0f);

            UIColor new_bg = is_new_chat_hovered_ ? UIColor::from_rgb(48, 50, 58) : UIColor::from_rgb(38, 39, 45);
            ctx.fill_rounded_rect(new_chat_rect_, 15.0f, new_bg);
            ctx.draw_rounded_rect(new_chat_rect_, 15.0f, UIColor::from_rgb(52, 54, 62), 1.0f);

            // Plus icon
            ctx.draw_line(UIPoint(new_chat_rect_.x + 16.5f, new_chat_rect_.y + 11.5f), UIPoint(new_chat_rect_.x + 16.5f, new_chat_rect_.y + 17.5f), ic_col, 1.5f);
            ctx.draw_line(UIPoint(new_chat_rect_.x + 13.5f, new_chat_rect_.y + 14.5f), UIPoint(new_chat_rect_.x + 19.5f, new_chat_rect_.y + 14.5f), ic_col, 1.5f);
            cur_right -= 12.0f;
        } else {
            new_chat_rect_ = UIRect(0, 0, 0, 0);
        }

        // Security Status Badge
        float badge_w = is_online_ ? 185.0f : 210.0f;
        cur_right -= badge_w;
        security_badge_rect_ = UIRect(cur_right, bounds_.y + 10.0f, badge_w, 30.0f);

        if (security_badge_rect_.x > title_rect.right() + 16.0f) {
            UIColor badge_bg = is_online_ ? UIColor::from_rgb(14, 38, 64) : UIColor::from_rgb(6, 48, 36);
            UIColor badge_border = is_online_ ? UIColor::from_rgb(14, 165, 233) : UIColor::from_rgb(16, 185, 129);
            ctx.fill_rounded_rect(security_badge_rect_, 15.0f, badge_bg);
            ctx.draw_rounded_rect(security_badge_rect_, 15.0f, badge_border, 1.2f);
        } else {
            security_badge_rect_ = UIRect(0, 0, 0, 0); // Hide if screen too narrow
        }
    }

    bool on_mouse_click(float x, float y, int button) override {
        if (button != 0) return false;
        if (show_new_chat_ && new_chat_rect_.contains(x, y)) {
            if (on_new_chat_) on_new_chat_();
            return true;
        }
        if (security_badge_rect_.contains(x, y)) {
            if (on_toggle_security_) on_toggle_security_();
            return true;
        }
        return false;
    }

    const UIRect& security_badge_rect() const { return security_badge_rect_; }
    const UIRect& new_chat_rect() const { return new_chat_rect_; }

    void set_title_hovered(bool h) { is_title_hovered_ = h; }
    void set_new_chat_hovered(bool h) { is_new_chat_hovered_ = h; }

private:
    std::wstring tab_title_ = L"Đoạn chat";
    bool is_online_ = false;
    bool show_new_chat_ = true;

    bool is_title_hovered_ = false;
    bool is_new_chat_hovered_ = false;

    UIRect new_chat_rect_;
    UIRect security_badge_rect_;

    std::function<void()> on_new_chat_;
    std::function<void()> on_toggle_security_;
};

} // namespace cyber_ui
