#pragma once
#ifndef CYBERBOX_TOPIC_MODAL_HPP
#define CYBERBOX_TOPIC_MODAL_HPP

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include "../graphics/d2d_renderer.hpp"

namespace views {

enum class ModalAction {
    NONE = 0,
    APPLY,
    CANCEL,
    PRESET_SELECTED,
    SWARM_MINUS,
    SWARM_PLUS,
    RESET_ROLES,
    DISMISS_OUTSIDE
};

class TopicModal {
public:
    TopicModal() = default;

    void render(graphics::D2DRenderer& r, const RECT& rc, int swarmSize);
    bool handle_mouse_move(int x, int y);
    ModalAction handle_lbutton_down(int x, int y, int& outPresetIdx);

    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

    const Gdiplus::RectF& get_card_rect() const { return modal_card_rect_; }

private:
    bool visible_ = false;

    Gdiplus::RectF modal_card_rect_;
    Gdiplus::RectF modal_btn_apply_rect_;
    Gdiplus::RectF modal_btn_cancel_rect_;
    Gdiplus::RectF modal_preset_rects_[4];
    Gdiplus::RectF modal_swarm_minus_rect_;
    Gdiplus::RectF modal_swarm_plus_rect_;
    Gdiplus::RectF modal_reset_roles_rect_;

    bool hover_modal_apply_ = false;
    bool hover_modal_cancel_ = false;
    int hover_modal_preset_ = -1;
    bool hover_btn_swarm_minus_ = false;
    bool hover_btn_swarm_plus_ = false;
    bool hover_btn_reset_roles_ = false;
};

} // namespace views

#endif // CYBERBOX_TOPIC_MODAL_HPP
