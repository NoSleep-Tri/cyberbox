#include "workspace_view.hpp"

namespace views {

static std::wstring s2ws(const std::string& raw) {
    if (raw.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), NULL, 0);
    std::wstring w(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), &w[0], size);
    return w;
}

void WorkspaceView::render(graphics::D2DRenderer& r, const RECT& rc, const std::string& currentWsFile) {
    int chat_left = 48;
    r.draw_text(L"DANH SÁCH TỆP TRONG SANDBOX",
                (float)chat_left + 20.0f, 64.0f, 300.0f, 24.0f,
                L"Segoe UI", 11.0f, true, graphics::D2DColor(241, 245, 249), 0, 0, false);

    std::wstring previewTitle = L"TRÌNH SOẠN THẢO CANVAS (CHATGPT CANVAS MODE)";
    if (!currentWsFile.empty()) {
        previewTitle += L" — " + s2ws(currentWsFile);
    } else {
        previewTitle += L" — (Chưa chọn tệp)";
    }

    r.draw_text(previewTitle,
                (float)chat_left + 328.0f, 64.0f, 520.0f, 24.0f,
                L"Segoe UI", 11.0f, true, graphics::D2DColor(129, 140, 248), 0, 0, false);
}

} // namespace views
