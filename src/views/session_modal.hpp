#pragma once
#ifndef CYBERBOX_SESSION_MODAL_HPP
#define CYBERBOX_SESSION_MODAL_HPP

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "../graphics/d2d_renderer.hpp"

namespace views {

struct SessionInfo {
    std::string filepath;
    std::string filename;
    std::string topic;
    std::string time_str;
    int swarm_size = 3;
    int message_count = 0;
};

enum class SessionModalAction {
    NONE = 0,
    LOAD_SESSION,
    DELETE_SESSION,
    SAVE_NEW,
    CLOSE
};

class SessionModal {
public:
    SessionModal() = default;

    void render(graphics::D2DRenderer& r, const RECT& rc, const std::vector<SessionInfo>& sessions);
    bool handle_mouse_move(int x, int y);
    SessionModalAction handle_lbutton_down(int x, int y, int& outSelectedIndex);

    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

    const Gdiplus::RectF& get_card_rect() const { return modal_card_rect_; }

private:
    bool visible_ = false;

    Gdiplus::RectF modal_card_rect_;
    Gdiplus::RectF modal_btn_close_rect_;
    Gdiplus::RectF modal_btn_save_new_rect_;
    std::vector<Gdiplus::RectF> item_rects_;
    std::vector<Gdiplus::RectF> item_load_btn_rects_;
    std::vector<Gdiplus::RectF> item_del_btn_rects_;

    bool hover_btn_close_ = false;
    bool hover_btn_save_new_ = false;
    int hover_load_idx_ = -1;
    int hover_del_idx_ = -1;
    int hover_item_idx_ = -1;
};

} // namespace views

#endif // CYBERBOX_SESSION_MODAL_HPP
