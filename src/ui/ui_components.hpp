#pragma once
#include "ui_element.hpp"
#include "ui_layout.hpp"
#include <string>

namespace cyber_ui {

class UILabel : public UIElement {
public:
    UILabel(const std::wstring& text = L"", const UIColor& color = theme().text_primary)
        : text_(text), text_color_(color) {}

    UILabel* set_text(const std::wstring& text) {
        text_ = text;
        return this;
    }

    const std::wstring& text() const { return text_; }

    UILabel* set_text_color(const UIColor& color) {
        text_color_ = color;
        return this;
    }

    const UIColor& text_color() const { return text_color_; }

    UILabel* set_text_align(TextAlign align) {
        align_ = align;
        return this;
    }

    UILabel* set_font_format(IDWriteTextFormat* format) {
        format_ = format;
        return this;
    }

    UISize measure(float available_width, float available_height) override {
        if (!text_.empty() && format_) {
            // Measure text with available width
            IDWriteFactory* factory = nullptr;
            // If format is provided, we can measure using a temporary context or layout
            IDWriteTextLayout* layout = nullptr;
            // Fallback estimation if factory not directly accessible here:
            // ~8px per char width, ~18px height
            float w = (width_ >= 0.0f) ? width_ : (text_.length() * 8.5f);
            float h = (height_ >= 0.0f) ? height_ : 20.0f;
            desired_size_ = UISize(w + padding_.horizontal(), h + padding_.vertical());
        } else {
            float w = (width_ >= 0.0f) ? width_ : (text_.length() * 8.5f);
            float h = (height_ >= 0.0f) ? height_ : 20.0f;
            desired_size_ = UISize(w + padding_.horizontal(), h + padding_.vertical());
        }
        return desired_size_;
    }

    void render_self(UIRenderContext& ctx) override {
        UIElement::render_self(ctx);

        if (!text_.empty()) {
            IDWriteTextFormat* fmt = format_;
            if (!fmt) {
                // If no specific format provided, we don't draw raw or use default
            } else {
                UIRect inner = bounds_.inset(padding_);
                ctx.draw_text(text_, inner, text_color_, fmt, align_);
            }
        }
    }

protected:
    std::wstring text_;
    UIColor text_color_;
    TextAlign align_ = TextAlign::LEFT;
    IDWriteTextFormat* format_ = nullptr;
};

class UIButton : public UIElement {
public:
    UIButton(const std::wstring& text = L"")
        : text_(text) {
        corner_radius_ = theme().radius_md;
        padding_ = UIPadding(8.0f, 16.0f);
        bg_normal_ = theme().bg_card;
        bg_hover_ = theme().bg_hover;
        bg_active_ = theme().bg_active;
        border_normal_ = theme().border_subtle;
        border_hover_ = theme().border_accent;
        text_color_normal_ = theme().text_secondary;
        text_color_hover_ = theme().text_primary;
        text_color_active_ = theme().accent_cyan;
    }

    UIButton* set_text(const std::wstring& text) { text_ = text; return this; }
    const std::wstring& text() const { return text_; }

    UIButton* set_font_format(IDWriteTextFormat* format) { format_ = format; return this; }
    UIButton* set_is_active(bool active) { is_active_ = active; return this; }
    bool is_active() const { return is_active_; }

    UIButton* set_bg_colors(const UIColor& normal, const UIColor& hover, const UIColor& active) {
        bg_normal_ = normal;
        bg_hover_ = hover;
        bg_active_ = active;
        return this;
    }

    UIButton* set_text_colors(const UIColor& normal, const UIColor& hover, const UIColor& active) {
        text_color_normal_ = normal;
        text_color_hover_ = hover;
        text_color_active_ = active;
        return this;
    }

    UIButton* set_pill_indicator(bool show, const UIColor& color = theme().accent_cyan) {
        has_pill_indicator_ = show;
        pill_indicator_color_ = color;
        return this;
    }

    UISize measure(float available_width, float available_height) override {
        float w = (width_ >= 0.0f) ? width_ : (text_.length() * 9.0f + padding_.horizontal());
        float h = (height_ >= 0.0f) ? height_ : (20.0f + padding_.vertical());
        desired_size_ = UISize(w, h);
        return desired_size_;
    }

    void render_self(UIRenderContext& ctx) override {
        // Interpolate colors based on hover_progress_ and active state
        UIColor current_bg = bg_normal_;
        UIColor current_border = border_normal_;
        UIColor current_text = text_color_normal_;

        if (is_active_) {
            current_bg = bg_active_;
            current_border = border_hover_;
            current_text = text_color_active_;
        } else if (hover_progress_ > 0.001f) {
            current_bg = UIColor::lerp(bg_normal_, bg_hover_, hover_progress_);
            current_border = UIColor::lerp(border_normal_, border_hover_, hover_progress_);
            current_text = UIColor::lerp(text_color_normal_, text_color_hover_, hover_progress_);
        }

        if (is_pressed_) {
            current_bg = bg_active_;
        }

        // Draw button background
        if (current_bg.a > 0.001f) {
            if (corner_radius_ > 0.0f) {
                ctx.fill_rounded_rect(bounds_, corner_radius_, current_bg);
            } else {
                ctx.fill_rect(bounds_, current_bg);
            }
        }

        // Draw button border
        if (border_width_ > 0.0f && current_border.a > 0.001f) {
            if (corner_radius_ > 0.0f) {
                ctx.draw_rounded_rect(bounds_, corner_radius_, current_border, border_width_);
            } else {
                ctx.draw_rect(bounds_, current_border, border_width_);
            }
        }

        // Draw Pill Indicator if active (e.g. glowing left rail LED indicator)
        if (has_pill_indicator_ && is_active_) {
            UIRect pill_rect(bounds_.x + 2.0f, bounds_.y + 6.0f, 3.0f, (std::max)(4.0f, bounds_.height - 12.0f));
            ctx.fill_rounded_rect(pill_rect, 1.5f, pill_indicator_color_);
        }

        // Draw text
        if (!text_.empty() && format_) {
            UIRect inner = bounds_.inset(padding_);
            ctx.draw_text(text_, inner, current_text, format_, TextAlign::CENTER);
        }
    }

protected:
    std::wstring text_;
    IDWriteTextFormat* format_ = nullptr;
    bool is_active_ = false;

    UIColor bg_normal_;
    UIColor bg_hover_;
    UIColor bg_active_;
    UIColor border_normal_;
    UIColor border_hover_;
    UIColor text_color_normal_;
    UIColor text_color_hover_;
    UIColor text_color_active_;

    bool has_pill_indicator_ = false;
    UIColor pill_indicator_color_ = theme().accent_cyan;
};

class UICard : public UIElement {
public:
    UICard() {
        background_color_ = theme().bg_card;
        border_color_ = theme().border_card;
        border_width_ = 1.0f;
        corner_radius_ = theme().radius_lg;
        padding_ = UIPadding(14.0f);
    }
};

class UIDivider : public UIElement {
public:
    UIDivider(bool vertical = false, float thickness = 1.0f, const UIColor& color = theme().border_subtle)
        : is_vertical_(vertical) {
        border_color_ = color;
        if (vertical) {
            width_ = thickness;
            flex_shrink_ = 0.0f;
        } else {
            height_ = thickness;
            flex_shrink_ = 0.0f;
        }
    }

    void render_self(UIRenderContext& ctx) override {
        ctx.fill_rect(bounds_, border_color_);
    }

private:
    bool is_vertical_ = false;
};

class UIScrollView : public UIElement {
public:
    UIScrollView() {
        clip_children_ = true;
    }

    UIScrollView* set_content(std::shared_ptr<UIElement> content) {
        clear_children();
        content_ = content;
        if (content_) {
            add_child(content_);
        }
        return this;
    }

    std::shared_ptr<UIElement> content() const { return content_; }

    void layout(const UIRect& bounds) override {
        bounds_ = bounds;
        if (!content_ || !content_->is_visible()) return;

        // Content gets full available width
        float avail_w = bounds.width;
        UISize content_sz = content_->measure(avail_w, 1000000.0f);
        content_height_ = content_sz.height;

        // Content bounds positioned with scroll offset
        UIRect content_bounds(bounds.x, bounds.y - scroll_y_float_, bounds.width, content_sz.height);
        content_->layout(content_bounds);

        // Clamp scroll range
        max_scroll_ = (std::max)(0.0f, content_height_ - bounds.height);
        if (target_scroll_y_ > max_scroll_) target_scroll_y_ = max_scroll_;
        if (target_scroll_y_ < 0.0f) target_scroll_y_ = 0.0f;
    }

    void update_animation(float delta_time) override {
        UIElement::update_animation(delta_time);

        // Smooth 120 FPS Sub-pixel exponential decay scrolling
        float speed = 22.0f;
        scroll_y_float_ += (target_scroll_y_ - scroll_y_float_) * (1.0f - std::exp(-speed * delta_time));

        if (std::abs(target_scroll_y_ - scroll_y_float_) < 0.2f) {
            scroll_y_float_ = target_scroll_y_;
        }

        // Re-layout content offset
        if (content_) {
            UIRect content_bounds(bounds_.x, bounds_.y - scroll_y_float_, bounds_.width, content_height_);
            content_->layout(content_bounds);
        }
    }

    bool on_mouse_scroll(float x, float y, float delta) override {
        if (!bounds_.contains(x, y)) return false;

        float scroll_step = 70.0f;
        target_scroll_y_ -= (delta / 120.0f) * scroll_step;
        if (target_scroll_y_ < 0.0f) target_scroll_y_ = 0.0f;
        if (target_scroll_y_ > max_scroll_) target_scroll_y_ = max_scroll_;
        return true;
    }

    void render_self(UIRenderContext& ctx) override {
        UIElement::render_self(ctx);

        // Render scrollbar track and thumb if content overflows
        if (max_scroll_ > 1.0f && bounds_.height > 20.0f) {
            float track_h = bounds_.height - 8.0f;
            float thumb_h = (std::max)(24.0f, track_h * (bounds_.height / content_height_));
            float thumb_y = bounds_.y + 4.0f + (scroll_y_float_ / max_scroll_) * (track_h - thumb_h);
            float thumb_x = bounds_.right() - 6.0f;

            UIRect thumb_rect(thumb_x, thumb_y, 4.0f, thumb_h);
            UIColor thumb_color = theme().border_medium.with_alpha(0.6f);
            ctx.fill_rounded_rect(thumb_rect, 2.0f, thumb_color);
        }
    }

    void scroll_to_bottom() {
        target_scroll_y_ = max_scroll_;
    }

    void scroll_to_top() {
        target_scroll_y_ = 0.0f;
    }

    float scroll_y() const { return scroll_y_float_; }
    float max_scroll() const { return max_scroll_; }

protected:
    std::shared_ptr<UIElement> content_;
    float scroll_y_float_ = 0.0f;
    float target_scroll_y_ = 0.0f;
    float content_height_ = 0.0f;
    float max_scroll_ = 0.0f;
};

} // namespace cyber_ui
