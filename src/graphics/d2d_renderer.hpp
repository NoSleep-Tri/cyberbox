#pragma once
#ifndef CYBERBOX_D2D_RENDERER_HPP
#define CYBERBOX_D2D_RENDERER_HPP

#include <windows.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dwrite.h>
#include <string>
#include <unordered_map>
#include "dx11_device.hpp"

namespace graphics {

struct D2DColor {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    D2DColor() = default;
    D2DColor(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    D2DColor(int r, int g, int b, int a = 255)
        : r(r / 255.0f), g(g / 255.0f), b(b / 255.0f), a(a / 255.0f) {}
    D2DColor(int r, int g, int b, float a)
        : r(r / 255.0f), g(g / 255.0f), b(b / 255.0f), a(a) {}
    D2DColor(COLORREF c, float alpha = 1.0f)
        : r(GetRValue(c) / 255.0f), g(GetGValue(c) / 255.0f), b(GetBValue(c) / 255.0f), a(alpha) {}

    D2D1_COLOR_F to_d2d() const { return D2D1::ColorF(r, g, b, a); }
};

class D2DRenderer {
public:
    D2DRenderer();
    ~D2DRenderer();

    bool initialize(DX11Device& dx11);
    void cleanup();

    // Rebind target bitmap when swapchain resizes
    void pre_resize();
    bool post_resize(DX11Device& dx11);
    bool on_resize(DX11Device& dx11);

    // Frame lifecycle
    void begin_draw();
    bool end_draw();

    // Clear background
    void clear(const D2DColor& color);

    // Clipping
    void set_clip(float x, float y, float w, float h);
    void clear_clip();

    // Primitives
    void draw_line(float x1, float y1, float x2, float y2, const D2DColor& color, float strokeWidth = 1.0f);
    void fill_rect(float x, float y, float w, float h, const D2DColor& color);
    void draw_rect(float x, float y, float w, float h, const D2DColor& color, float strokeWidth = 1.0f);
    void draw_rounded_box(float x, float y, float w, float h, float radius,
                          const D2DColor& bgColor, const D2DColor& borderColor, float borderWidth = 1.0f);
    void fill_ellipse(float cx, float cy, float rx, float ry, const D2DColor& color);
    void draw_ellipse(float cx, float cy, float rx, float ry, const D2DColor& color, float strokeWidth = 1.0f);

    // Text & DirectWrite
    D2D1_SIZE_F measure_text(const std::wstring& text, float maxW, const wchar_t* fontFamily,
                             float fontSizePt, bool bold = false, bool wordWrap = true, float lineSpacing = 0.0f);

    void draw_text(const std::wstring& text, float x, float y, float w, float h,
                   const wchar_t* fontFamily, float fontSizePt, bool bold, const D2DColor& color,
                   int alignH = 0, int alignV = 0, bool wordWrap = true, float lineSpacing = 0.0f);

    // Vector Avatars & Icons
    void draw_circle_avatar(float cx, float cy, float radius, const D2DColor& bgColor,
                            const std::wstring& letter, const D2DColor& textColor);

    ID2D1DeviceContext* get_context() const { return d2d_context_; }
    ID2D1Factory1* get_factory() const { return d2d_factory_; }
    IDWriteFactory* get_dwrite_factory() const { return dwrite_factory_; }

    void set_override_target(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* br = nullptr) {
        override_target_ = rt;
        override_brush_ = br;
    }

private:
    ID2D1Factory1* d2d_factory_ = nullptr;
    ID2D1Device* d2d_device_ = nullptr;
    ID2D1DeviceContext* d2d_context_ = nullptr;
    ID2D1RenderTarget* override_target_ = nullptr;
    ID2D1SolidColorBrush* override_brush_ = nullptr;
    ID2D1Bitmap1* target_bitmap_ = nullptr;
    ID2D1SolidColorBrush* solid_brush_ = nullptr;
    ID2D1StrokeStyle* stroke_round_ = nullptr;
    IDWriteFactory* dwrite_factory_ = nullptr;

    ID2D1RenderTarget* get_rt() const {
        return override_target_ ? override_target_ : static_cast<ID2D1RenderTarget*>(d2d_context_);
    }
    ID2D1SolidColorBrush* get_brush() const {
        return override_brush_ ? override_brush_ : solid_brush_;
    }

    bool has_active_clip_ = false;

    // Cache for DirectWrite text formats
    std::unordered_map<std::wstring, IDWriteTextFormat*> text_format_cache_;
    IDWriteTextFormat* get_text_format(const wchar_t* fontFamily, float fontSizePt, bool bold,
                                       bool wordWrap, float lineSpacing = 0.0f);
};

} // namespace graphics

#endif // CYBERBOX_D2D_RENDERER_HPP
