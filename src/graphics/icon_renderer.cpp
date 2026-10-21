#include "icon_renderer.hpp"

namespace graphics {

void IconRenderer::draw_dock_icon(D2DRenderer& r, int iconIdx, float cx, float cy, const D2DColor& col) {
    float sw = 1.6f;
    switch (iconIdx) {
        case 0: { // Speech Bubble (Chat)
            r.draw_ellipse(cx, cy - 1.0f, 7.5f, 6.5f, col, sw);
            r.draw_line(cx - 5.5f, cy + 3.5f, cx - 8.0f, cy + 7.5f, col, sw);
            r.draw_line(cx - 8.0f, cy + 7.5f, cx - 2.0f, cy + 6.0f, col, sw);
            break;
        }
        case 1: { // Search Magnifying Glass
            r.draw_ellipse(cx - 1.5f, cy - 1.5f, 5.5f, 5.5f, col, sw);
            r.draw_line(cx + 2.5f, cy + 2.5f, cx + 7.0f, cy + 7.0f, col, sw);
            break;
        }
        case 2: { // Notebook / Workspace Canvas
            r.draw_rounded_box(cx - 6.5f, cy - 7.5f, 13.0f, 15.0f, 2.5f, D2DColor(0,0,0,0), col, 1.5f);
            r.draw_line(cx - 3.0f, cy - 7.5f, cx - 3.0f, cy + 7.5f, col, sw);
            r.draw_line(cx, cy - 3.0f, cx + 3.5f, cy - 3.0f, col, sw);
            r.draw_line(cx, cy + 1.0f, cx + 3.5f, cy + 1.0f, col, sw);
            break;
        }
        case 3: { // Lightbulb / Skills
            r.draw_ellipse(cx, cy - 2.0f, 5.5f, 5.5f, col, sw);
            r.draw_line(cx - 3.5f, cy + 2.0f, cx - 2.5f, cy + 4.5f, col, sw);
            r.draw_line(cx + 3.5f, cy + 2.0f, cx + 2.5f, cy + 4.5f, col, sw);
            r.draw_line(cx - 2.5f, cy + 4.5f, cx + 2.5f, cy + 4.5f, col, sw);
            r.draw_line(cx - 1.5f, cy + 7.5f, cx + 1.5f, cy + 7.5f, col, sw);
            break;
        }
        case 4: { // Checkbox with checkmark
            r.draw_rounded_box(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f, 3.0f, D2DColor(0,0,0,0), col, 1.5f);
            r.draw_line(cx - 3.5f, cy + 0.2f, cx - 0.8f, cy + 3.0f, col, sw);
            r.draw_line(cx - 0.8f, cy + 3.0f, cx + 4.0f, cy - 2.5f, col, sw);
            break;
        }
        case 5: { // Swarm Multi-Agent Grid
            r.draw_rounded_box(cx - 7.5f, cy - 7.5f, 6.0f, 6.0f, 1.5f, D2DColor(0,0,0,0), col, 1.5f);
            r.draw_rounded_box(cx - 7.5f, cy + 1.5f, 6.0f, 6.0f, 1.5f, D2DColor(0,0,0,0), col, 1.5f);
            r.draw_rounded_box(cx + 1.5f, cy + 1.5f, 6.0f, 6.0f, 1.5f, D2DColor(0,0,0,0), col, 1.5f);
            r.draw_line(cx + 4.5f, cy - 8.2f, cx + 8.2f, cy - 4.5f, col, sw);
            r.draw_line(cx + 8.2f, cy - 4.5f, cx + 4.5f, cy - 0.8f, col, sw);
            r.draw_line(cx + 4.5f, cy - 0.8f, cx + 0.8f, cy - 4.5f, col, sw);
            r.draw_line(cx + 0.8f, cy - 4.5f, cx + 4.5f, cy - 8.2f, col, sw);
            break;
        }
        case 6: { // Tray / Settings Gear
            r.draw_rounded_box(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f, 3.0f, D2DColor(0,0,0,0), col, 1.5f);
            r.draw_line(cx, cy - 4.0f, cx, cy + 2.0f, col, sw);
            r.draw_line(cx - 2.8f, cy - 0.5f, cx, cy + 2.5f, col, sw);
            r.draw_line(cx + 2.8f, cy - 0.5f, cx, cy + 2.5f, col, sw);
            r.draw_line(cx - 4.0f, cy + 4.2f, cx + 4.0f, cy + 4.2f, col, sw);
            break;
        }
        case 7: { // '=' Sidebar Toggle
            r.draw_line(cx - 5.5f, cy - 2.5f, cx + 5.5f, cy - 2.5f, col, sw);
            r.draw_line(cx - 5.5f, cy + 2.5f, cx + 5.5f, cy + 2.5f, col, sw);
            break;
        }
    }
}

void IconRenderer::draw_right_tab_icon(D2DRenderer& r, int tabIdx, float cx, float cy, const D2DColor& col) {
    float sw = 1.5f;
    switch (tabIdx) {
        case 0: { // Bullet list icon (≡:)
            for (int i = -1; i <= 1; ++i) {
                float ry = cy + (i * 4.0f);
                r.fill_ellipse(cx - 7.0f, ry, 1.5f, 1.5f, col);
                r.draw_line(cx - 2.5f, ry, cx + 6.5f, ry, col, sw);
            }
            break;
        }
        case 1: { // Shield with checkmark
            r.draw_line(cx, cy - 6.8f, cx + 5.8f, cy - 4.5f, col, sw);
            r.draw_line(cx + 5.8f, cy - 4.5f, cx + 5.2f, cy + 1.5f, col, sw);
            r.draw_line(cx + 5.2f, cy + 1.5f, cx, cy + 6.8f, col, sw);
            r.draw_line(cx, cy + 6.8f, cx - 5.2f, cy + 1.5f, col, sw);
            r.draw_line(cx - 5.2f, cy + 1.5f, cx - 5.8f, cy - 4.5f, col, sw);
            r.draw_line(cx - 5.8f, cy - 4.5f, cx, cy - 6.8f, col, sw);
            r.draw_line(cx - 2.3f, cy + 0.2f, cx - 0.5f, cy + 2.2f, col, sw);
            r.draw_line(cx - 0.5f, cy + 2.2f, cx + 2.8f, cy - 1.8f, col, sw);
            break;
        }
        case 2: { // Clock
            r.draw_ellipse(cx, cy, 6.5f, 6.5f, col, sw);
            r.draw_line(cx, cy - 3.5f, cx, cy + 0.5f, col, sw);
            r.draw_line(cx, cy + 0.5f, cx + 3.0f, cy + 2.0f, col, sw);
            break;
        }
        case 3: { // Multi-Agent concentric arcs / Network
            r.draw_ellipse(cx, cy, 6.5f, 6.5f, col, sw);
            r.draw_ellipse(cx, cy, 3.5f, 3.5f, col, sw);
            r.fill_ellipse(cx, cy, 1.5f, 1.5f, col);
            break;
        }
    }
}

void IconRenderer::draw_window_minimize(D2DRenderer& r, float x, float y, float w, float h, bool hover) {
    if (hover) {
        r.fill_rect(x, y, w, h, D2DColor(38, 40, 50));
    }
    D2DColor col = hover ? D2DColor(255, 255, 255) : D2DColor(160, 164, 178);
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    r.draw_line(cx - 5.0f, cy, cx + 5.0f, cy, col, 1.4f);
}

void IconRenderer::draw_window_maximize(D2DRenderer& r, float x, float y, float w, float h, bool hover, bool isMaximized) {
    if (hover) {
        r.fill_rect(x, y, w, h, D2DColor(38, 40, 50));
    }
    D2DColor col = hover ? D2DColor(255, 255, 255) : D2DColor(160, 164, 178);
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    if (isMaximized) {
        r.draw_rect(cx - 3.0f, cy - 5.0f, 8.0f, 8.0f, col, 1.1f);
        r.fill_rect(cx - 6.0f, cy - 2.0f, 8.0f, 8.0f, D2DColor(20, 21, 25));
        r.draw_rect(cx - 6.0f, cy - 2.0f, 8.0f, 8.0f, col, 1.1f);
    } else {
        r.draw_rect(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, col, 1.2f);
    }
}

void IconRenderer::draw_window_close(D2DRenderer& r, float x, float y, float w, float h, bool hover) {
    if (hover) {
        r.fill_rect(x, y, w, h, D2DColor(232, 17, 35));
    }
    D2DColor col = hover ? D2DColor(255, 255, 255) : D2DColor(160, 164, 178);
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    r.draw_line(cx - 5.0f, cy - 5.0f, cx + 5.0f, cy + 5.0f, col, 1.4f);
    r.draw_line(cx + 5.0f, cy - 5.0f, cx - 5.0f, cy + 5.0f, col, 1.4f);
}

} // namespace graphics
