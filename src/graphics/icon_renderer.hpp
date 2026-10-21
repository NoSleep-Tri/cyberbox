#pragma once
#ifndef CYBERBOX_ICON_RENDERER_HPP
#define CYBERBOX_ICON_RENDERER_HPP

#include "d2d_renderer.hpp"

namespace graphics {

class IconRenderer {
public:
    static void draw_dock_icon(D2DRenderer& r, int iconIdx, float cx, float cy, const D2DColor& col);
    static void draw_right_tab_icon(D2DRenderer& r, int tabIdx, float cx, float cy, const D2DColor& col);
    static void draw_window_minimize(D2DRenderer& r, float x, float y, float w, float h, bool hover);
    static void draw_window_maximize(D2DRenderer& r, float x, float y, float w, float h, bool hover, bool isMaximized);
    static void draw_window_close(D2DRenderer& r, float x, float y, float w, float h, bool hover);
};

} // namespace graphics

#endif // CYBERBOX_ICON_RENDERER_HPP
