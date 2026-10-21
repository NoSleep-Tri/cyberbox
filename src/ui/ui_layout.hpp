#pragma once
#include "ui_element.hpp"
#include <numeric>

namespace cyber_ui {

class UIFlexBox : public UIElement {
public:
    UIFlexBox(FlexDirection dir = FlexDirection::COLUMN)
        : direction_(dir) {}

    UIFlexBox* set_direction(FlexDirection dir) { direction_ = dir; return this; }
    UIFlexBox* set_gap(float gap) { gap_ = gap; return this; }
    UIFlexBox* set_align(FlexAlign align) { align_items_ = align; return this; }
    UIFlexBox* set_justify(FlexJustify justify) { justify_content_ = justify; return this; }

    FlexDirection direction() const { return direction_; }
    float gap() const { return gap_; }
    FlexAlign align() const { return align_items_; }
    FlexJustify justify() const { return justify_content_; }

    UISize measure(float available_width, float available_height) override {
        float inner_avail_w = (std::max)(0.0f, available_width - padding_.horizontal());
        float inner_avail_h = (std::max)(0.0f, available_height - padding_.vertical());

        float total_main = 0.0f;
        float max_cross = 0.0f;
        int visible_count = 0;

        for (auto& child : children_) {
            if (!child->is_visible()) continue;
            visible_count++;

            UISize sz = child->measure(inner_avail_w, inner_avail_h);
            if (direction_ == FlexDirection::COLUMN) {
                total_main += sz.height;
                max_cross = (std::max)(max_cross, sz.width);
            } else {
                total_main += sz.width;
                max_cross = (std::max)(max_cross, sz.height);
            }
        }

        if (visible_count > 1) {
            total_main += (visible_count - 1) * gap_;
        }

        float desired_w = (direction_ == FlexDirection::COLUMN) ? max_cross : total_main;
        float desired_h = (direction_ == FlexDirection::COLUMN) ? total_main : max_cross;

        if (width_ >= 0.0f) desired_w = width_;
        if (height_ >= 0.0f) desired_h = height_;

        desired_w += padding_.horizontal() + margin_.horizontal();
        desired_h += padding_.vertical() + margin_.vertical();

        desired_w = (std::max)(min_width_, (std::min)(desired_w, max_width_));
        desired_h = (std::max)(min_height_, (std::min)(desired_h, max_height_));

        desired_size_ = UISize(desired_w, desired_h);
        return desired_size_;
    }

    void layout(const UIRect& bounds) override {
        bounds_ = bounds;
        UIRect inner = bounds.inset(padding_);

        std::vector<std::shared_ptr<UIElement>> visible_children;
        for (auto& c : children_) {
            if (c->is_visible()) visible_children.push_back(c);
        }

        if (visible_children.empty()) return;

        bool is_col = (direction_ == FlexDirection::COLUMN);
        float container_main = is_col ? inner.height : inner.width;
        float container_cross = is_col ? inner.width : inner.height;

        float total_gaps = (visible_children.size() > 1) ? (visible_children.size() - 1) * gap_ : 0.0f;
        float non_flex_main = 0.0f;
        float total_flex_grow = 0.0f;

        for (auto& child : visible_children) {
            float grow = child->flex_grow();
            if (grow > 0.0f) {
                total_flex_grow += grow;
            } else {
                float child_main = is_col ? child->desired_size().height : child->desired_size().width;
                if (is_col && child->height() >= 0.0f) child_main = child->height();
                if (!is_col && child->width() >= 0.0f) child_main = child->width();
                non_flex_main += child_main;
            }
        }

        float remaining_main = (std::max)(0.0f, container_main - total_gaps - non_flex_main);

        // Precompute sizes
        std::vector<float> child_main_sizes(visible_children.size());
        for (size_t i = 0; i < visible_children.size(); ++i) {
            auto& child = visible_children[i];
            float grow = child->flex_grow();
            if (grow > 0.0f && total_flex_grow > 0.0f) {
                child_main_sizes[i] = remaining_main * (grow / total_flex_grow);
            } else {
                float child_main = is_col ? child->desired_size().height : child->desired_size().width;
                if (is_col && child->height() >= 0.0f) child_main = child->height();
                if (!is_col && child->width() >= 0.0f) child_main = child->width();
                child_main_sizes[i] = child_main;
            }
        }

        // Justify positioning along main axis if there is leftover space and no elements grew
        float main_offset = 0.0f;
        float item_gap = gap_;
        if (total_flex_grow <= 0.0f) {
            float total_used = non_flex_main + total_gaps;
            float extra = container_main - total_used;
            if (extra > 0.0f) {
                if (justify_content_ == FlexJustify::CENTER) {
                    main_offset = extra * 0.5f;
                } else if (justify_content_ == FlexJustify::END) {
                    main_offset = extra;
                } else if (justify_content_ == FlexJustify::SPACE_BETWEEN && visible_children.size() > 1) {
                    item_gap = gap_ + extra / (visible_children.size() - 1);
                } else if (justify_content_ == FlexJustify::SPACE_AROUND) {
                    float space = extra / visible_children.size();
                    main_offset = space * 0.5f;
                    item_gap = gap_ + space;
                } else if (justify_content_ == FlexJustify::SPACE_EVENLY) {
                    float space = extra / (visible_children.size() + 1);
                    main_offset = space;
                    item_gap = gap_ + space;
                }
            }
        }

        // Layout each child
        float cur_main = (is_col ? inner.y : inner.x) + main_offset;

        for (size_t i = 0; i < visible_children.size(); ++i) {
            auto& child = visible_children[i];
            float child_main = child_main_sizes[i];

            // Cross axis calculation
            float child_cross = is_col ? child->desired_size().width : child->desired_size().height;
            if (is_col && child->width() >= 0.0f) child_cross = child->width();
            if (!is_col && child->height() >= 0.0f) child_cross = child->height();

            if (align_items_ == FlexAlign::STRETCH) {
                child_cross = container_cross;
            }

            float cross_offset = 0.0f;
            if (align_items_ == FlexAlign::CENTER) {
                cross_offset = (container_cross - child_cross) * 0.5f;
            } else if (align_items_ == FlexAlign::END) {
                cross_offset = container_cross - child_cross;
            }

            float cur_cross = (is_col ? inner.x : inner.y) + cross_offset;

            UIRect child_rect;
            if (is_col) {
                child_rect = UIRect(cur_cross, cur_main, child_cross, child_main);
            } else {
                child_rect = UIRect(cur_main, cur_cross, child_main, child_cross);
            }

            child->layout(child_rect);
            cur_main += child_main + item_gap;
        }
    }

protected:
    FlexDirection direction_ = FlexDirection::COLUMN;
    float gap_ = 0.0f;
    FlexAlign align_items_ = FlexAlign::STRETCH;
    FlexJustify justify_content_ = FlexJustify::START;
};

class VBox : public UIFlexBox {
public:
    VBox(float gap = 0.0f) : UIFlexBox(FlexDirection::COLUMN) {
        gap_ = gap;
    }
};

class HBox : public UIFlexBox {
public:
    HBox(float gap = 0.0f) : UIFlexBox(FlexDirection::ROW) {
        gap_ = gap;
    }
};

class Spacer : public UIElement {
public:
    Spacer(float grow = 1.0f) {
        flex_grow_ = grow;
        flex_shrink_ = 1.0f;
    }

    UISize measure(float, float) override {
        return UISize(0.0f, 0.0f);
    }
};

} // namespace cyber_ui
