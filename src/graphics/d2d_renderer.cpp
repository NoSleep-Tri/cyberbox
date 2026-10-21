#include "d2d_renderer.hpp"
#include <sstream>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace graphics {

D2DRenderer::D2DRenderer() {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite_factory_);
}

D2DRenderer::~D2DRenderer() {
    cleanup();
}

bool D2DRenderer::initialize(DX11Device& dx11) {
    cleanup();

    // 1. Create Direct2D 1.1 Factory
    D2D1_FACTORY_OPTIONS options = {};
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void**)&d2d_factory_);
    if (FAILED(hr)) return false;

    // 2. Query DXGI Device from D3D11 Device
    IDXGIDevice* dxgiDevice = nullptr;
    hr = dx11.get_d3d_device()->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr)) return false;

    // 3. Create Direct2D 1.1 Device & DeviceContext
    hr = d2d_factory_->CreateDevice(dxgiDevice, &d2d_device_);
    dxgiDevice->Release();
    if (FAILED(hr)) return false;

    hr = d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_context_);
    if (FAILED(hr)) return false;

    // 4. Create DirectWrite Factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite_factory_);
    if (FAILED(hr)) return false;

    // 5. Create default brush & round stroke style
    hr = d2d_context_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &solid_brush_);
    if (FAILED(hr)) return false;

    D2D1_STROKE_STYLE_PROPERTIES ssp = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND
    );
    hr = d2d_factory_->CreateStrokeStyle(&ssp, nullptr, 0, &stroke_round_);

    // 6. Bind to SwapChain BackBuffer Surface
    return on_resize(dx11);
}

void D2DRenderer::pre_resize() {
    if (d2d_context_) {
        d2d_context_->SetTarget(nullptr);
    }
    if (target_bitmap_) {
        target_bitmap_->Release();
        target_bitmap_ = nullptr;
    }
}

bool D2DRenderer::post_resize(DX11Device& dx11) {
    if (!d2d_context_) return false;

    IDXGISurface* surface = dx11.get_backbuffer_surface();
    if (!surface) return false;

    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    HRESULT hr = d2d_context_->CreateBitmapFromDxgiSurface(surface, &bp, &target_bitmap_);
    if (FAILED(hr) || !target_bitmap_) {
        bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
        hr = d2d_context_->CreateBitmapFromDxgiSurface(surface, &bp, &target_bitmap_);
    }
    if (FAILED(hr) || !target_bitmap_) return false;

    d2d_context_->SetTarget(target_bitmap_);
    d2d_context_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    d2d_context_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    return true;
}

bool D2DRenderer::on_resize(DX11Device& dx11) {
    pre_resize();
    return post_resize(dx11);
}

void D2DRenderer::begin_draw() {
    if (d2d_context_) {
        d2d_context_->BeginDraw();
        has_active_clip_ = false;
    }
}

bool D2DRenderer::end_draw() {
    if (!d2d_context_) return false;
    clear_clip();
    HRESULT hr = d2d_context_->EndDraw();
    return SUCCEEDED(hr);
}

void D2DRenderer::clear(const D2DColor& color) {
    ID2D1RenderTarget* rt = get_rt();
    if (rt) {
        rt->Clear(color.to_d2d());
    }
}

void D2DRenderer::set_clip(float x, float y, float w, float h) {
    ID2D1RenderTarget* rt = get_rt();
    if (!rt) return;
    clear_clip();
    rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    has_active_clip_ = true;
}

void D2DRenderer::clear_clip() {
    ID2D1RenderTarget* rt = get_rt();
    if (rt && has_active_clip_) {
        rt->PopAxisAlignedClip();
        has_active_clip_ = false;
    }
}

void D2DRenderer::draw_line(float x1, float y1, float x2, float y2, const D2DColor& color, float strokeWidth) {
    ID2D1RenderTarget* rt = get_rt();
    ID2D1SolidColorBrush* br = get_brush();
    if (!rt || !br) return;
    br->SetColor(color.to_d2d());
    rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), br, strokeWidth, stroke_round_);
}

void D2DRenderer::fill_rect(float x, float y, float w, float h, const D2DColor& color) {
    ID2D1RenderTarget* rt = get_rt();
    ID2D1SolidColorBrush* br = get_brush();
    if (!rt || !br) return;
    br->SetColor(color.to_d2d());
    rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), br);
}

void D2DRenderer::draw_rect(float x, float y, float w, float h, const D2DColor& color, float strokeWidth) {
    ID2D1RenderTarget* rt = get_rt();
    ID2D1SolidColorBrush* br = get_brush();
    if (!rt || !br) return;
    br->SetColor(color.to_d2d());
    rt->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), br, strokeWidth);
}

void D2DRenderer::draw_rounded_box(float x, float y, float w, float h, float radius,
                                   const D2DColor& bgColor, const D2DColor& borderColor, float borderWidth) {
    ID2D1RenderTarget* rt = get_rt();
    ID2D1SolidColorBrush* br = get_brush();
    if (!rt || !br) return;
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);

    if (bgColor.a > 0.0f) {
        br->SetColor(bgColor.to_d2d());
        rt->FillRoundedRectangle(rr, br);
    }
    if (borderColor.a > 0.0f && borderWidth > 0.0f) {
        br->SetColor(borderColor.to_d2d());
        rt->DrawRoundedRectangle(rr, br, borderWidth);
    }
}

void D2DRenderer::fill_ellipse(float cx, float cy, float rx, float ry, const D2DColor& color) {
    ID2D1RenderTarget* rt = get_rt();
    ID2D1SolidColorBrush* br = get_brush();
    if (!rt || !br) return;
    br->SetColor(color.to_d2d());
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), br);
}

void D2DRenderer::draw_ellipse(float cx, float cy, float rx, float ry, const D2DColor& color, float strokeWidth) {
    ID2D1RenderTarget* rt = get_rt();
    ID2D1SolidColorBrush* br = get_brush();
    if (!rt || !br) return;
    br->SetColor(color.to_d2d());
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), br, strokeWidth);
}

IDWriteTextFormat* D2DRenderer::get_text_format(const wchar_t* fontFamily, float fontSizePt,
                                               bool bold, bool wordWrap, float lineSpacing) {
    if (!dwrite_factory_) return nullptr;

    std::wstring key = std::wstring(fontFamily) + L"_" + std::to_wstring(fontSizePt) +
                       L"_" + (bold ? L"B" : L"R") + L"_" + (wordWrap ? L"W" : L"N") +
                       L"_" + std::to_wstring(lineSpacing);

    auto it = text_format_cache_.find(key);
    if (it != text_format_cache_.end()) return it->second;

    float dipSize = fontSizePt * (96.0f / 72.0f);
    IDWriteTextFormat* fmt = nullptr;
    HRESULT hr = dwrite_factory_->CreateTextFormat(
        fontFamily,
        nullptr,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        dipSize,
        L"vi-vn",
        &fmt
    );

    if (SUCCEEDED(hr) && fmt) {
        fmt->SetWordWrapping(wordWrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
        if (lineSpacing > 0.0f) {
            fmt->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineSpacing, lineSpacing * 0.8f);
        }
        text_format_cache_[key] = fmt;
        return fmt;
    }
    return nullptr;
}

D2D1_SIZE_F D2DRenderer::measure_text(const std::wstring& text, float maxW, const wchar_t* fontFamily,
                                      float fontSizePt, bool bold, bool wordWrap, float lineSpacing) {
    if (!dwrite_factory_ || text.empty()) return D2D1::SizeF(0.0f, 0.0f);

    IDWriteTextFormat* fmt = get_text_format(fontFamily, fontSizePt, bold, wordWrap, lineSpacing);
    if (!fmt) return D2D1::SizeF(0.0f, 0.0f);

    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = dwrite_factory_->CreateTextLayout(
        text.c_str(),
        (UINT32)text.size(),
        fmt,
        maxW > 0.0f ? maxW : 4000.0f,
        4000.0f,
        &layout
    );

    D2D1_SIZE_F res = D2D1::SizeF(0.0f, 0.0f);
    if (SUCCEEDED(hr) && layout) {
        DWRITE_TEXT_METRICS tm;
        if (SUCCEEDED(layout->GetMetrics(&tm))) {
            res.width = tm.width;
            res.height = tm.height;
        }
        layout->Release();
    }
    return res;
}

void D2DRenderer::draw_text(const std::wstring& text, float x, float y, float w, float h,
                            const wchar_t* fontFamily, float fontSizePt, bool bold, const D2DColor& color,
                            int alignH, int alignV, bool wordWrap, float lineSpacing) {
    ID2D1RenderTarget* rt = get_rt();
    ID2D1SolidColorBrush* br = get_brush();
    if (!rt || !br || text.empty()) return;

    IDWriteTextFormat* fmt = get_text_format(fontFamily, fontSizePt, bold, wordWrap, lineSpacing);
    if (!fmt) return;

    fmt->SetTextAlignment(alignH == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER :
                         (alignH == 2 ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING));

    fmt->SetParagraphAlignment(alignV == 1 ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER :
                              (alignV == 2 ? DWRITE_PARAGRAPH_ALIGNMENT_FAR : DWRITE_PARAGRAPH_ALIGNMENT_NEAR));

    br->SetColor(color.to_d2d());
    (rt->DrawText)(
        text.c_str(),
        (UINT32)text.size(),
        fmt,
        D2D1::RectF(x, y, x + w, y + h),
        br,
        static_cast<D2D1_DRAW_TEXT_OPTIONS>(0x00000004) // D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
    );
}

void D2DRenderer::draw_circle_avatar(float cx, float cy, float radius, const D2DColor& bgColor,
                                     const std::wstring& letter, const D2DColor& textColor) {
    fill_ellipse(cx, cy, radius, radius, bgColor);
    if (!letter.empty()) {
        float sz = radius * 1.0f;
        draw_text(letter, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f,
                  L"Segoe UI", sz * 0.72f, true, textColor, 1, 1, false);
    }
}

void D2DRenderer::cleanup() {
    clear_clip();
    for (auto& pair : text_format_cache_) {
        if (pair.second) pair.second->Release();
    }
    text_format_cache_.clear();

    if (stroke_round_) { stroke_round_->Release(); stroke_round_ = nullptr; }
    if (solid_brush_) { solid_brush_->Release(); solid_brush_ = nullptr; }
    if (target_bitmap_) { target_bitmap_->Release(); target_bitmap_ = nullptr; }
    if (dwrite_factory_) { dwrite_factory_->Release(); dwrite_factory_ = nullptr; }
    if (d2d_context_) { d2d_context_->Release(); d2d_context_ = nullptr; }
    if (d2d_device_) { d2d_device_->Release(); d2d_device_ = nullptr; }
    if (d2d_factory_) { d2d_factory_->Release(); d2d_factory_ = nullptr; }
}

} // namespace graphics
