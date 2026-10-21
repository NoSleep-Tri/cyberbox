#pragma once
#ifndef CYBERBOX_ARENA_VIEW_HPP
#define CYBERBOX_ARENA_VIEW_HPP

#include <windows.h>
#include <string>
#include <vector>
#include "../common_types.hpp"
#include "../graphics/d2d_renderer.hpp"

namespace views {

enum class ArenaClickAction {
    NONE = 0,
    SPEED_TOGGLE,
    RESET_ROLES,
    SWARM_PLUS,
    SWARM_MINUS,
    CHANGE_TOPIC
};

class ArenaView {
public:
    ArenaView() = default;

    void render(
        graphics::D2DRenderer& r,
        const RECT& rc,
        float rightPanelW,
        const std::string& currentTopic,
        int swarmSize,
        int speedMs,
        const std::vector<gui::AgentInfo>& agents,
        const std::vector<gui::GuiMessage>& messages,
        bool isRunning,
        const std::string& activeSpeaker,
        int scrollY,
        float pulsePhase,
        int& outContentHeight
    );

    bool handle_mouse_move(int x, int y);
    ArenaClickAction handle_lbutton_down(int x, int y, int chatLeft, int chatRight);

    const Gdiplus::RectF& get_speed_btn_rect() const { return speed_btn_rect_; }
    const Gdiplus::RectF& get_reset_roles_rect() const { return reset_roles_rect_; }
    const Gdiplus::RectF& get_swarm_plus_rect() const { return swarm_plus_rect_; }
    const Gdiplus::RectF& get_swarm_minus_rect() const { return swarm_minus_rect_; }

private:
    Gdiplus::RectF speed_btn_rect_;
    Gdiplus::RectF reset_roles_rect_;
    Gdiplus::RectF swarm_plus_rect_;
    Gdiplus::RectF swarm_minus_rect_;

    bool hover_speed_ = false;
    bool hover_reset_roles_ = false;
    bool hover_swarm_plus_ = false;
    bool hover_swarm_minus_ = false;
};

} // namespace views

#endif // CYBERBOX_ARENA_VIEW_HPP
