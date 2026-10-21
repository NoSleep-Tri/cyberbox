#pragma once
#include "ui_types.hpp"
#include "ui_theme.hpp"
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include <memory>

namespace cyber_ui {

class UIRenderContext {
public:
    UIRenderContext(ID2D1RenderTarget* rt, IDWriteFactory* dwrite, ID2D1SolidColorBrush* brush = nullptr)
        : rt_(rt), dwrite_(dwrite), brush_(brush), owns_brush_(false) {
        if (!brush_ && rt_) {
            rt_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_);
            owns_brush_ = true;
        }
    }

    ~UIRenderContext() {
        // Clean up clip stack if any left
        while (!clip_stack_.empty()) {
            pop_clip();
        }
        if (owns_brush_ && brush_) {
            brush_->Release();
            brush_ = nullptr;
        }
    }

    ID2D1RenderTarget* render_target() const { return rt_; }
    IDWriteFactory* dwrite_factory() const { return dwrite_; }

    // --- Transform / Offset Stack ---
    void push_offset(float dx, float dy) {
        current_offset_.x += dx;
        current_offset_.y += dy;
        offset_stack_.push_back({ dx, dy });
    }

    void pop_offset() {
        if (!offset_stack_.empty()) {
            UIPoint top = offset_stack_.back();
            offset_stack_.pop_back();
            current_offset_.x -= top.x;
            current_offset_.y -= top.y;
        }
    }

    UIPoint current_offset() const { return current_offset_; }

    // --- Scissor Clip Stack ---
    void push_clip(const UIRect& rect) {
        UIRect world_rect = rect.offset(current_offset_.x, current_offset_.y);
        UIRect effective_clip = world_rect;

        if (!clip_stack_.empty()) {
            effective_clip = clip_stack_.back().intersect(world_rect);
        }

        clip_stack_.push_back(effective_clip);
        if (rt_) {
            rt_->PushAxisAlignedClip(effective_clip.to_d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        }
    }

    void pop_clip() {
        if (!clip_stack_.empty()) {
            clip_stack_.pop_back();
            if (rt_) {
                rt_->PopAxisAlignedClip();
            }
        }
    }

    // --- Drawing Helpers ---
    void fill_rect(const UIRect& rect, const UIColor& color) {
        if (!rt_ || !brush_) return;
        UIRect r = rect.offset(current_offset_.x, current_offset_.y);
        brush_->SetColor(color.to_d2d());
        rt_->FillRectangle(r.to_d2d(), brush_);
    }

    void draw_rect(const UIRect& rect, const UIColor& color, float stroke_w = 1.0f) {
        if (!rt_ || !brush_) return;
        UIRect r = rect.offset(current_offset_.x, current_offset_.y);
        brush_->SetColor(color.to_d2d());
        rt_->DrawRectangle(r.to_d2d(), brush_, stroke_w);
    }

    void fill_rounded_rect(const UIRect& rect, float radius, const UIColor& color) {
        if (!rt_ || !brush_) return;
        UIRect r = rect.offset(current_offset_.x, current_offset_.y);
        brush_->SetColor(color.to_d2d());
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(r.to_d2d(), radius, radius);
        rt_->FillRoundedRectangle(rr, brush_);
    }

    void draw_rounded_rect(const UIRect& rect, float radius, const UIColor& color, float stroke_w = 1.0f) {
        if (!rt_ || !brush_) return;
        UIRect r = rect.offset(current_offset_.x, current_offset_.y);
        brush_->SetColor(color.to_d2d());
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(r.to_d2d(), radius, radius);
        rt_->DrawRoundedRectangle(rr, brush_, stroke_w);
    }

    void draw_line(const UIPoint& p1, const UIPoint& p2, const UIColor& color, float stroke_w = 1.0f) {
        if (!rt_ || !brush_) return;
        brush_->SetColor(color.to_d2d());
        D2D1_POINT_2F pt1 = D2D1::Point2F(p1.x + current_offset_.x, p1.y + current_offset_.y);
        D2D1_POINT_2F pt2 = D2D1::Point2F(p2.x + current_offset_.x, p2.y + current_offset_.y);
        rt_->DrawLine(pt1, pt2, brush_, stroke_w);
    }

    void draw_text(const std::wstring& text, const UIRect& rect, const UIColor& color,
                   IDWriteTextFormat* format, TextAlign align = TextAlign::LEFT) {
        if (!rt_ || !brush_ || !format || text.empty()) return;
        UIRect r = rect.offset(current_offset_.x, current_offset_.y);
        brush_->SetColor(color.to_d2d());

        DWRITE_TEXT_ALIGNMENT dwrite_align = DWRITE_TEXT_ALIGNMENT_LEADING;
        if (align == TextAlign::CENTER) dwrite_align = DWRITE_TEXT_ALIGNMENT_CENTER;
        else if (align == TextAlign::RIGHT) dwrite_align = DWRITE_TEXT_ALIGNMENT_TRAILING;

        format->SetTextAlignment(dwrite_align);
        rt_->DrawText(
            text.c_str(),
            static_cast<UINT32>(text.length()),
            format,
            r.to_d2d(),
            brush_,
            D2D1_DRAW_TEXT_OPTIONS_CLIP
        );
    }

    UISize measure_text(const std::wstring& text, IDWriteTextFormat* format, float max_width = 10000.0f) {
        if (!dwrite_ || !format || text.empty()) return UISize(0.0f, 0.0f);

        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwrite_->CreateTextLayout(
            text.c_str(),
            static_cast<UINT32>(text.length()),
            format,
            max_width,
            10000.0f,
            &layout
        );

        if (SUCCEEDED(hr) && layout) {
            DWRITE_TEXT_METRICS metrics;
            layout->GetMetrics(&metrics);
            layout->Release();
            return UISize(metrics.widthIncludingTrailingWhitespace, metrics.height);
        }
        return UISize(0.0f, 0.0f);
    }

private:
    ID2D1RenderTarget* rt_ = nullptr;
    IDWriteFactory* dwrite_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    bool owns_brush_ = false;

    UIPoint current_offset_ = { 0.0f, 0.0f };
    std::vector<UIPoint> offset_stack_;
    std::vector<UIRect> clip_stack_;
};

} // namespace cyber_ui
