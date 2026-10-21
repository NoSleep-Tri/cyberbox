#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d2d1.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cyber_ui {

struct UIPoint {
    float x = 0.0f;
    float y = 0.0f;

    UIPoint() = default;
    constexpr UIPoint(float x_, float y_) : x(x_), y(y_) {}
};

struct UISize {
    float width = 0.0f;
    float height = 0.0f;

    UISize() = default;
    constexpr UISize(float w, float h) : width(w), height(h) {}
};

struct UIPadding {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    UIPadding() = default;
    constexpr explicit UIPadding(float all) : top(all), right(all), bottom(all), left(all) {}
    constexpr UIPadding(float vertical, float horizontal) : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    constexpr UIPadding(float t, float r, float b, float l) : top(t), right(r), bottom(b), left(l) {}

    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
};

using UIMargin = UIPadding;

struct UIColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    UIColor() = default;
    constexpr UIColor(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr UIColor from_rgb(uint8_t red, uint8_t green, uint8_t blue, float alpha = 1.0f) {
        return UIColor(red / 255.0f, green / 255.0f, blue / 255.0f, alpha);
    }

    static constexpr UIColor from_hex(uint32_t hex, float alpha = 1.0f) {
        return UIColor(
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f,
            alpha
        );
    }

    static constexpr UIColor from_rgba_hex(uint32_t rgba) {
        return UIColor(
            ((rgba >> 24) & 0xFF) / 255.0f,
            ((rgba >> 16) & 0xFF) / 255.0f,
            ((rgba >> 8) & 0xFF) / 255.0f,
            (rgba & 0xFF) / 255.0f
        );
    }

    UIColor with_alpha(float new_alpha) const {
        return UIColor(r, g, b, new_alpha);
    }

    D2D1_COLOR_F to_d2d() const {
        return D2D1::ColorF(r, g, b, a);
    }

    static UIColor lerp(const UIColor& c1, const UIColor& c2, float t) {
        t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
        return UIColor(
            c1.r + (c2.r - c1.r) * t,
            c1.g + (c2.g - c1.g) * t,
            c1.b + (c2.b - c1.b) * t,
            c1.a + (c2.a - c1.a) * t
        );
    }
};

struct UIRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    UIRect() = default;
    constexpr UIRect(float x_, float y_, float w, float h) : x(x_), y(y_), width(w), height(h) {}

    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + width; }
    float bottom() const { return y + height; }

    bool contains(float px, float py) const {
        return px >= x && px <= right() && py >= y && py <= bottom();
    }

    bool contains(const UIPoint& p) const {
        return contains(p.x, p.y);
    }

    UIRect offset(float dx, float dy) const {
        return UIRect(x + dx, y + dy, width, height);
    }

    UIRect inset(const UIPadding& pad) const {
        return UIRect(
            x + pad.left,
            y + pad.top,
            (std::max)(0.0f, width - pad.horizontal()),
            (std::max)(0.0f, height - pad.vertical())
        );
    }

    UIRect intersect(const UIRect& other) const {
        float x1 = (std::max)(x, other.x);
        float y1 = (std::max)(y, other.y);
        float x2 = (std::min)(right(), other.right());
        float y2 = (std::min)(bottom(), other.bottom());
        if (x2 < x1 || y2 < y1) return UIRect(0, 0, 0, 0);
        return UIRect(x1, y1, x2 - x1, y2 - y1);
    }

    D2D1_RECT_F to_d2d() const {
        return D2D1::RectF(x, y, x + width, y + height);
    }
};

enum class FlexDirection {
    ROW,
    COLUMN
};

enum class FlexAlign {
    START,
    CENTER,
    END,
    STRETCH
};

enum class FlexJustify {
    START,
    CENTER,
    END,
    SPACE_BETWEEN,
    SPACE_AROUND,
    SPACE_EVENLY
};

enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

} // namespace cyber_ui
