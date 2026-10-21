#pragma once
#include "ui_element.hpp"
#include "ui_render_context.hpp"
#include "ui_theme.hpp"
#include <memory>

namespace cyber_ui {

class UIRoot {
public:
    UIRoot() = default;

    void set_root(std::shared_ptr<UIElement> root) {
        root_element_ = root;
        if (root_element_ && viewport_w_ > 0.0f && viewport_h_ > 0.0f) {
            relayout();
        }
    }

    std::shared_ptr<UIElement> root() const { return root_element_; }

    void resize(float width, float height) {
        viewport_w_ = width;
        viewport_h_ = height;
        relayout();
    }

    void relayout() {
        if (!root_element_) return;
        root_element_->measure(viewport_w_, viewport_h_);
        root_element_->layout(UIRect(0.0f, 0.0f, viewport_w_, viewport_h_));
    }

    void render(UIRenderContext& ctx) {
        if (!root_element_) return;
        root_element_->render(ctx);
    }

    void update_animation(float delta_time) {
        if (!root_element_) return;
        root_element_->update_animation(delta_time);
    }

    void on_mouse_move(float x, float y) {
        last_mouse_x_ = x;
        last_mouse_y_ = y;
        if (!root_element_) return;

        auto hit = root_element_->hit_test(x, y);
        if (hit != hovered_element_) {
            if (hovered_element_) {
                hovered_element_->on_mouse_leave();
            }
            hovered_element_ = hit;
            if (hovered_element_) {
                hovered_element_->on_mouse_enter();
            }
        }
    }

    bool on_mouse_down(float x, float y, int button) {
        last_mouse_x_ = x;
        last_mouse_y_ = y;
        if (!root_element_) return false;

        auto hit = root_element_->hit_test(x, y);
        pressed_element_ = hit;
        if (hit) {
            return hit->on_mouse_down(x, y, button);
        }
        return false;
    }

    bool on_mouse_up(float x, float y, int button) {
        last_mouse_x_ = x;
        last_mouse_y_ = y;
        if (pressed_element_) {
            bool handled = pressed_element_->on_mouse_up(x, y, button);
            pressed_element_ = nullptr;
            return handled;
        }
        if (!root_element_) return false;
        auto hit = root_element_->hit_test(x, y);
        if (hit) {
            return hit->on_mouse_up(x, y, button);
        }
        return false;
    }

    bool on_mouse_wheel(float x, float y, float delta) {
        if (!root_element_) return false;
        auto hit = root_element_->hit_test(x, y);
        if (hit) {
            return hit->on_mouse_scroll(x, y, delta);
        }
        return false;
    }

    void on_mouse_leave_window() {
        if (hovered_element_) {
            hovered_element_->on_mouse_leave();
            hovered_element_ = nullptr;
        }
        if (pressed_element_) {
            pressed_element_->on_mouse_leave();
            pressed_element_ = nullptr;
        }
    }

    std::shared_ptr<UIElement> hovered_element() const { return hovered_element_; }

private:
    std::shared_ptr<UIElement> root_element_;
    std::shared_ptr<UIElement> hovered_element_;
    std::shared_ptr<UIElement> pressed_element_;

    float viewport_w_ = 0.0f;
    float viewport_h_ = 0.0f;
    float last_mouse_x_ = 0.0f;
    float last_mouse_y_ = 0.0f;
};

} // namespace cyber_ui
