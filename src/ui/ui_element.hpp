#pragma once
#include "ui_types.hpp"
#include "ui_theme.hpp"
#include "ui_render_context.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <algorithm>

namespace cyber_ui {

class UIElement : public std::enable_shared_from_this<UIElement> {
public:
    UIElement() = default;
    virtual ~UIElement() = default;

    // --- Identification ---
    UIElement* set_id(const std::string& id) { id_ = id; return this; }
    const std::string& id() const { return id_; }

    // --- Box Model & Layout Properties ---
    UIElement* set_size(float w, float h) { width_ = w; height_ = h; return this; }
    UIElement* set_width(float w) { width_ = w; return this; }
    UIElement* set_height(float h) { height_ = h; return this; }
    UIElement* set_min_size(float min_w, float min_h) { min_width_ = min_w; min_height_ = min_h; return this; }
    UIElement* set_max_size(float max_w, float max_h) { max_width_ = max_w; max_height_ = max_h; return this; }
    UIElement* set_flex_grow(float grow) { flex_grow_ = grow; return this; }
    UIElement* set_flex_shrink(float shrink) { flex_shrink_ = shrink; return this; }
    UIElement* set_padding(const UIPadding& p) { padding_ = p; return this; }
    UIElement* set_padding(float all) { padding_ = UIPadding(all); return this; }
    UIElement* set_padding(float v, float h) { padding_ = UIPadding(v, h); return this; }
    UIElement* set_margin(const UIMargin& m) { margin_ = m; return this; }
    UIElement* set_margin(float all) { margin_ = UIMargin(all); return this; }

    float width() const { return width_; }
    float height() const { return height_; }
    float flex_grow() const { return flex_grow_; }
    float flex_shrink() const { return flex_shrink_; }
    const UIPadding& padding() const { return padding_; }
    const UIMargin& margin() const { return margin_; }

    // --- Styles ---
    UIElement* set_background(const UIColor& color) { background_color_ = color; return this; }
    UIElement* set_border(const UIColor& color, float width = 1.0f) {
        border_color_ = color;
        border_width_ = width;
        return this;
    }
    UIElement* set_radius(float r) { corner_radius_ = r; return this; }
    UIElement* set_visible(bool v) { visible_ = v; return this; }
    UIElement* set_enabled(bool e) { enabled_ = e; return this; }
    UIElement* set_clip_children(bool clip) { clip_children_ = clip; return this; }

    const UIColor& background() const { return background_color_; }
    const UIColor& border_color() const { return border_color_; }
    float border_width() const { return border_width_; }
    float corner_radius() const { return corner_radius_; }
    bool is_visible() const { return visible_; }
    bool is_enabled() const { return enabled_; }
    bool clips_children() const { return clip_children_; }

    // --- Computed Geometry ---
    const UIRect& bounds() const { return bounds_; }
    const UISize& desired_size() const { return desired_size_; }

    // --- Hierarchy ---
    std::shared_ptr<UIElement> parent() const { return parent_.lock(); }
    const std::vector<std::shared_ptr<UIElement>>& children() const { return children_; }

    UIElement* add_child(std::shared_ptr<UIElement> child) {
        if (child) {
            child->parent_ = shared_from_this();
            children_.push_back(child);
        }
        return this;
    }

    void remove_child(std::shared_ptr<UIElement> child) {
        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            (*it)->parent_.reset();
            children_.erase(it);
        }
    }

    void clear_children() {
        for (auto& child : children_) {
            child->parent_.reset();
        }
        children_.clear();
    }

    // --- State & Animation ---
    bool is_hovered() const { return is_hovered_; }
    bool is_pressed() const { return is_pressed_; }
    float hover_progress() const { return hover_progress_; }

    // --- Virtual Lifecycle Methods ---
    virtual UISize measure(float available_width, float available_height) {
        float content_w = (width_ >= 0.0f) ? width_ : 0.0f;
        float content_h = (height_ >= 0.0f) ? height_ : 0.0f;

        // Measure children
        for (auto& child : children_) {
            if (!child->is_visible()) continue;
            UISize child_sz = child->measure(available_width, available_height);
            content_w = (std::max)(content_w, child_sz.width);
            content_h = (std::max)(content_h, child_sz.height);
        }

        content_w += padding_.horizontal() + margin_.horizontal();
        content_h += padding_.vertical() + margin_.vertical();

        content_w = (std::max)(min_width_, (std::min)(content_w, max_width_));
        content_h = (std::max)(min_height_, (std::min)(content_h, max_height_));

        desired_size_ = UISize(content_w, content_h);
        return desired_size_;
    }

    virtual void layout(const UIRect& bounds) {
        bounds_ = bounds;
        UIRect inner = bounds.inset(padding_);

        // Default layout assigns inner bounds to all children
        for (auto& child : children_) {
            if (child->is_visible()) {
                child->layout(inner);
            }
        }
    }

    virtual void render(UIRenderContext& ctx) {
        if (!visible_) return;

        render_self(ctx);

        if (clip_children_) {
            ctx.push_clip(bounds_);
        }

        render_children(ctx);

        if (clip_children_) {
            ctx.pop_clip();
        }
    }

    virtual void render_self(UIRenderContext& ctx) {
        // Draw background
        if (background_color_.a > 0.001f) {
            if (corner_radius_ > 0.0f) {
                ctx.fill_rounded_rect(bounds_, corner_radius_, background_color_);
            } else {
                ctx.fill_rect(bounds_, background_color_);
            }
        }

        // Draw border
        if (border_width_ > 0.0f && border_color_.a > 0.001f) {
            if (corner_radius_ > 0.0f) {
                ctx.draw_rounded_rect(bounds_, corner_radius_, border_color_, border_width_);
            } else {
                ctx.draw_rect(bounds_, border_color_, border_width_);
            }
        }
    }

    virtual void render_children(UIRenderContext& ctx) {
        for (auto& child : children_) {
            if (child->is_visible()) {
                child->render(ctx);
            }
        }
    }

    virtual void update_animation(float delta_time) {
        // Smooth hover transition at 120 FPS
        float target_hover = is_hovered_ ? 1.0f : 0.0f;
        float speed = 14.0f; // Rapid, responsive transition
        hover_progress_ += (target_hover - hover_progress_) * (1.0f - std::exp(-speed * delta_time));

        for (auto& child : children_) {
            child->update_animation(delta_time);
        }
    }

    // --- Hit-Testing & Interaction ---
    virtual std::shared_ptr<UIElement> hit_test(float x, float y) {
        if (!visible_ || !enabled_) return nullptr;
        if (!bounds_.contains(x, y)) return nullptr;

        // Traverse children in reverse order (topmost first)
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            auto hit = (*it)->hit_test(x, y);
            if (hit) return hit;
        }

        return shared_from_this();
    }

    virtual void on_mouse_enter() {
        is_hovered_ = true;
        if (on_hover) on_hover(true);
    }

    virtual void on_mouse_leave() {
        is_hovered_ = false;
        is_pressed_ = false;
        if (on_hover) on_hover(false);
    }

    virtual bool on_mouse_down(float x, float y, int button) {
        if (button == 0) is_pressed_ = true;
        return true;
    }

    virtual bool on_mouse_up(float x, float y, int button) {
        bool was_pressed = is_pressed_;
        is_pressed_ = false;
        if (button == 0 && was_pressed && bounds_.contains(x, y)) {
            return on_mouse_click(x, y, button);
        }
        return false;
    }

    virtual bool on_mouse_click(float x, float y, int button) {
        if (on_click) {
            on_click();
            return true;
        }
        return false;
    }

    virtual bool on_mouse_scroll(float x, float y, float delta) {
        auto p = parent();
        if (p) return p->on_mouse_scroll(x, y, delta);
        return false;
    }

    // Callbacks
    std::function<void()> on_click;
    std::function<void(bool)> on_hover;

protected:
    std::string id_;
    float width_ = -1.0f;
    float height_ = -1.0f;
    float min_width_ = 0.0f;
    float min_height_ = 0.0f;
    float max_width_ = 100000.0f;
    float max_height_ = 100000.0f;
    float flex_grow_ = 0.0f;
    float flex_shrink_ = 1.0f;
    UIPadding padding_;
    UIMargin margin_;

    UIColor background_color_ = UIColor(0, 0, 0, 0.0f);
    UIColor border_color_ = UIColor(0, 0, 0, 0.0f);
    float border_width_ = 0.0f;
    float corner_radius_ = 0.0f;
    bool visible_ = true;
    bool enabled_ = true;
    bool clip_children_ = false;

    bool is_hovered_ = false;
    bool is_pressed_ = false;
    float hover_progress_ = 0.0f;

    UIRect bounds_;
    UISize desired_size_;

    std::weak_ptr<UIElement> parent_;
    std::vector<std::shared_ptr<UIElement>> children_;
};

} // namespace cyber_ui
