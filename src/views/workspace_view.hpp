#pragma once
#ifndef CYBERBOX_WORKSPACE_VIEW_HPP
#define CYBERBOX_WORKSPACE_VIEW_HPP

#include <windows.h>
#include <string>
#include "../graphics/d2d_renderer.hpp"

namespace views {

class WorkspaceView {
public:
    WorkspaceView() = default;

    void render(graphics::D2DRenderer& r, const RECT& rc, const std::string& currentWsFile);
};

} // namespace views

#endif // CYBERBOX_WORKSPACE_VIEW_HPP
