#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <timeapi.h>
#include <unknwn.h>
#include <gdiplus.h>
#include <d2d1.h>
#include <dwrite.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <unordered_map>
#include "ai_client.hpp"
#include "sandbox_manager.hpp"
#include "ui/ui_engine.hpp"
#include "anti_screenshot.hpp"
#include "graphics/dx11_device.hpp"
#include "graphics/d2d_renderer.hpp"
#include "graphics/icon_renderer.hpp"
#include "graphics/syntax_highlighter.hpp"
#include "common_types.hpp"
#include "views/arena_view.hpp"
#include "views/topic_modal.hpp"
#include "views/chat_view.hpp"
#include "views/settings_view.hpp"
#include "views/workspace_view.hpp"
#include "views/session_modal.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")

namespace gui {

class NativeGui {
public:
    NativeGui();
    ~NativeGui();

    bool create(HINSTANCE hInstance, int nCmdShow = SW_SHOWNORMAL);
    int run_message_loop();

    // Real-time companion console logger
    static void log_console(const std::string& prefix, const std::string& msg, int color_attr = 7);

private:
    HWND hwnd_main_ = NULL;
    HINSTANCE hinstance_ = NULL;
    ActiveTab current_tab_ = ActiveTab::CHAT;
    ULONG_PTR gdiplus_token_ = 0;
    Gdiplus::Image* muse_avatar_img_ = NULL;

    // Sidebar & Panel state
    bool show_right_panel_ = true;
    int active_dock_icon_ = 0;    // 0: Chat, 1: Search, 2: Workspace, 3: Skills, 4: Tasks, 5: Arena, 6: Settings
    int right_panel_tab_ = 0;     // 0: Tasks, 1: Sandbox, 2: Timeline, 3: AI/Arena
    int right_panel_scroll_y_ = 0;

    // Hover states for GDI+ rendered buttons
    int hover_dock_icon_ = -1;
    int hover_right_tab_ = -1;
    int hover_task_idx_ = -1;
    bool hover_btn_new_ = false;
    bool hover_btn_session_ = false;
    bool hover_btn_plus_ = false;
    bool hover_btn_send_ = false;
    bool hover_btn_close_ = false;
    bool hover_btn_drawer_collapse_ = false;
    bool hover_win_min_ = false;
    bool hover_win_max_ = false;
    bool hover_win_close_ = false;
    Gdiplus::RectF btn_win_min_rect_;
    Gdiplus::RectF btn_win_max_rect_;
    Gdiplus::RectF btn_win_close_rect_;
    Gdiplus::RectF btn_drawer_collapse_rect_;
    bool hover_floating_muse_ = false;
    bool hover_btn_arena_interject_ = false;
    Gdiplus::RectF arena_interject_btn_rect_;
    bool hover_btn_arena_toggle_ = false;
    bool hover_btn_arena_topic_ = false;
    bool hover_btn_arena_export_ = false;
    Gdiplus::RectF arena_toggle_btn_rect_;
    Gdiplus::RectF arena_topic_btn_rect_;
    Gdiplus::RectF arena_export_btn_rect_;

    // Code block interactive hitboxes (Copy & Download)
    std::vector<CodeBlockHitBox> code_hitboxes_;
    int copied_block_id_ = -1;
    int downloaded_block_id_ = -1;

    // Reasoning Stream collapsible hitboxes
    std::vector<ReasoningHitBox> reasoning_hitboxes_;

    // Chat View Controls
    HWND h_chat_input_ = NULL;

    // Arena View Controls
    HWND h_arena_interject_input_ = NULL;
    HWND h_arena_topic_input_ = NULL;

    // Settings View Controls
    HWND h_cfg_provider_combo_ = NULL;
    HWND h_cfg_base_url_ = NULL;
    HWND h_cfg_api_key_ = NULL;
    HWND h_cfg_model_ = NULL;
    HWND h_cfg_test_btn_ = NULL;
    HWND h_cfg_save_btn_ = NULL;
    HWND h_cfg_engine_combo_ = NULL;
    HWND h_cfg_reset_ws_btn_ = NULL;

    // Workspace View Controls (ChatGPT Canvas & File Manager Mode)
    HWND h_ws_listbox_ = NULL;
    HWND h_ws_refresh_btn_ = NULL;
    HWND h_ws_open_folder_btn_ = NULL;
    HWND h_ws_preview_edit_ = NULL;
    HWND h_ws_save_btn_ = NULL;
    HWND h_ws_cmd_input_ = NULL;
    HWND h_ws_cmd_run_btn_ = NULL;
    std::string current_ws_file_;

    // Interactive Security Badge (Top Bar 1-Click Air-Gap / Online Toggle)
    Gdiplus::RectF security_badge_rect_;

    // Interactive SEB Anti-Screenshot Badge (Top Bar 1-Click Toggle)
    Gdiplus::RectF anti_capture_badge_rect_;
    void on_anti_capture_toggle();

    // GDI & Direct2D / DirectWrite Universal Font & Color Emoji Resources
    HBRUSH bg_brush_ = NULL;
    HBRUSH card_brush_ = NULL;
    HBRUSH input_brush_ = NULL;
    HBRUSH header_brush_ = NULL;
    HFONT font_ui_ = NULL;
    HFONT font_ui_bold_ = NULL;
    HFONT font_mono_ = NULL;
    HFONT font_title_ = NULL;
    HFONT font_small_ = NULL;

    // Persistent High-Res Backbuffer (allocated only on resize, zero per-frame allocation)
    HDC backbuffer_dc_ = NULL;
    HBITMAP backbuffer_bmp_ = NULL;
    HBITMAP backbuffer_old_bmp_ = NULL;
    int backbuffer_w_ = 0;
    int backbuffer_h_ = 0;
    void* backbuffer_bits_ = nullptr;

    // Single-Pass Hardware-Accelerated Direct2D & Cached DirectWrite Resources
    ID2D1Factory* d2d_factory_ = NULL;
    ID2D1DCRenderTarget* d2d_dc_target_ = NULL;
    ID2D1RenderTarget* d2d_target_ = NULL;
    ID2D1RenderTarget* get_d2d_target();
    ID2D1SolidColorBrush* d2d_solid_brush_ = NULL;
    ID2D1StrokeStyle* d2d_stroke_round_ = NULL;
    ID2D1Bitmap* d2d_avatar_bmp_ = NULL;
    IDWriteFactory* dwrite_factory_ = NULL;
    std::unordered_map<std::wstring, IDWriteTextFormat*> text_format_cache_;
    bool d2d_batch_active_ = false;
    HDC current_hdc_ = NULL;
    RECT current_rc_ = { 0, 0, 0, 0 };
    bool has_active_clip_ = false;
    D2D1_RECT_F active_clip_rect_ = { 0.0f, 0.0f, 0.0f, 0.0f };

    // DirectX 11 Hardware GPU Engine (Zero-Copy Flip Model)
    graphics::DX11Device dx11_;
    graphics::D2DRenderer dx11_renderer_;
    bool use_dx11_ = false;

    // Modular UI Views (Zero-Copy GPU Accelerated)
    views::ArenaView arena_view_;
    views::TopicModal topic_modal_view_;
    views::ChatView chat_view_;
    views::SettingsView settings_view_;
    views::WorkspaceView workspace_view_;

    // CyberUI Engine Host
    cyber_ui::UIRoot ui_root_;
    std::shared_ptr<cyber_ui::UIDockRail> ui_dock_rail_;
    std::shared_ptr<cyber_ui::UITopBar> ui_top_bar_;
    void init_ui_engine();

    // Backend Engines
    std::unique_ptr<ai::AiClient> ai_client_;
    std::unique_ptr<sandbox::SandboxManager> sandbox_mgr_;
    std::vector<ai::AiConfig> providers_;
    std::string active_provider_id_ = "ollama";
    std::mutex providers_mutex_;

    // Chat State & Real-Time Streaming Animation
    std::vector<GuiMessage> chat_messages_;
    std::mutex chat_mutex_;
    std::atomic<bool> is_chat_generating_{false};
    std::atomic<bool> chat_interrupt_flag_{false};
    std::atomic<bool> chunk_msg_queued_{false};
    int chat_scroll_y_ = 0;
    int target_chat_scroll_y_ = 0;
    int chat_content_height_ = 0;
    bool auto_scroll_chat_ = true;

    // 120 FPS High-Precision Animation & Smooth Easing State
    LARGE_INTEGER perf_freq_ = { 0 };
    LARGE_INTEGER last_anim_time_ = { 0 };
    int anim_frame_ = 0;
    float pulse_phase_ = 0.0f;
    float chat_scroll_float_ = 0.0f;
    float arena_scroll_float_ = 0.0f;

    // Fluid Scene & Tab Transition Animation System
    float tab_transition_progress_ = 1.0f;
    float tab_transition_speed_ = 0.18f;
    int tab_transition_from_icon_ = 0;
    int tab_transition_to_icon_ = 0;
    float tab_transition_offset_y_ = 0.0f;

    // Smooth Gliding Left Dock Pill Indicator
    float dock_indicator_y_ = 60.0f;
    float dock_indicator_target_y_ = 60.0f;

    // Smooth Sliding Right Panel (Companion Drawer)
    float right_panel_cur_w_ = 310.0f;
    float right_panel_target_w_ = 310.0f;

    // Smooth Gliding Right Panel Segmented Tabs Indicator
    float right_tab_indicator_x_ = 0.0f;
    float right_tab_target_x_ = 0.0f;

    // Right Panel Today Tasks & Timeline Logs
    std::vector<TodayTaskItem> today_tasks_;
    std::vector<std::string> timeline_logs_;

    // Arena State
    std::vector<GuiMessage> arena_messages_;
    std::vector<AgentInfo> arena_agents_;
    std::string arena_topic_ = "Phân tích cơ chế Sandbox thực tế đang chạy: Win32 Job Object (1GB RAM, 32 PIDs, UI Restrictions), Low-Integrity Token (S-1-16-4096) và Air-Gap.";
    std::atomic<bool> arena_running_{false};
    std::atomic<int> arena_delay_ms_{2500};
    std::thread arena_thread_;
    std::mutex arena_mutex_;
    size_t arena_speaker_idx_ = 0;
    std::string arena_active_speaker_ = "";
    int arena_scroll_y_ = 0;
    int target_arena_scroll_y_ = 0;
    int arena_content_height_ = 0;
    bool auto_scroll_arena_ = true;

    // Arena Swarm & Custom Topic Modal State
    bool show_topic_modal_ = false;
    int swarm_size_ = 3;
    int arena_context_window_ = 3; // Customizable recent turns window (1 to 10, or 0 = unlimited)
    void set_swarm_size(int size);
    void add_swarm_agent();
    void remove_swarm_agent();
    void reset_swarm_roles();
    void cycle_arena_speed();
    void cycle_arena_context_window();
    void apply_new_topic(const std::string& new_topic);

    Gdiplus::RectF arena_swarm_minus_rect_;
    Gdiplus::RectF arena_swarm_plus_rect_;
    Gdiplus::RectF arena_reset_roles_rect_;
    Gdiplus::RectF arena_speed_btn_rect_;
    Gdiplus::RectF arena_context_btn_rect_;
    Gdiplus::RectF modal_card_rect_;
    Gdiplus::RectF modal_btn_apply_rect_;
    Gdiplus::RectF modal_btn_cancel_rect_;
    Gdiplus::RectF modal_preset_rects_[4];
    Gdiplus::RectF modal_swarm_minus_rect_;
    Gdiplus::RectF modal_swarm_plus_rect_;
    Gdiplus::RectF modal_reset_roles_rect_;

    bool hover_btn_swarm_minus_ = false;
    bool hover_btn_swarm_plus_ = false;
    bool hover_btn_reset_roles_ = false;
    bool hover_btn_speed_ = false;
    bool hover_btn_context_ = false;
    bool hover_modal_apply_ = false;
    bool hover_modal_cancel_ = false;
    int hover_modal_preset_ = -1;

    // Control layout & initialization
    void create_child_controls();
    void layout_controls(int width, int height);
    void switch_tab(ActiveTab tab);
    void update_control_visibility();

    // Data init & persistence
    void init_providers();
    void init_arena_agents();
    void init_today_tasks();
    void load_config_from_disk();
    void save_config_to_disk();
    ai::AiConfig get_config_for_agent(const AgentInfo& agent);
    ai::AiConfig get_active_config();

    // Actions
    void on_send_chat();
    void on_reset_chat();
    void on_stop_task();
    void copy_to_clipboard(const std::string& text);

    void on_arena_toggle();
    void on_arena_interject();
    void on_arena_change_topic();
    void on_arena_export();
    void on_arena_save_session();
    void on_arena_open_sessions();
    void on_arena_load_session(int index);
    void on_arena_delete_session(int index);
    void refresh_saved_sessions();
    void auto_save_active_arena();
    void auto_load_active_arena();
    void arena_worker_loop();

    views::SessionModal session_modal_;
    std::vector<views::SessionInfo> cached_sessions_;
    Gdiplus::RectF arena_save_btn_rect_;
    Gdiplus::RectF arena_sessions_btn_rect_;
    bool hover_btn_arena_save_ = false;
    bool hover_btn_arena_sessions_ = false;

    void on_settings_provider_changed();
    void on_settings_test();
    void on_settings_save();
    void on_airgap_toggle();

    void on_ws_refresh();
    void on_ws_file_select();
    void on_ws_save_file();
    void on_ws_open_folder();
    void on_ws_run_cmd();

    // Single-Pass Direct2D / DirectWrite High-Resolution GPU Engine
    IDWriteTextFormat* get_text_format(const wchar_t* fontFamily, float fontSizePt, bool bold, bool wordWrap, float lineSpacing = 0.0f);
    void ensure_d2d_resources();
    void d2d_fill_rect(float x, float y, float w, float h, Gdiplus::Color col);
    void d2d_fill_ellipse(float x, float y, float w, float h, Gdiplus::Color col);
    void d2d_draw_ellipse(float x, float y, float w, float h, Gdiplus::Color col, float strokeWidth = 1.5f);
    void d2d_draw_line(float x1, float y1, float x2, float y2, Gdiplus::Color col, float strokeWidth = 1.5f);
    void d2d_draw_arc(float x, float y, float w, float h, float startDeg, float sweepDeg, Gdiplus::Color col, float strokeWidth = 1.5f);

    void set_render_clip(float x, float y, float w, float h);
    void clear_render_clip();
    D2D1_SIZE_F measure_text_dw(const std::wstring& text, float maxW, const wchar_t* fontFamily,
                                float fontSizePt, bool bold = false, bool wordWrap = true);
    void draw_text_dw(Gdiplus::Graphics& g, const std::wstring& text, const Gdiplus::RectF& rect,
                      const wchar_t* fontFamily, float fontSizePt, bool bold, Gdiplus::Color color,
                      int alignH = 0, int alignV = 0, bool wordWrap = true);
    float measure_rich_text_line(const std::string& line, float maxW);

    void draw_custom_button(LPDRAWITEMSTRUCT pDIS);
    void draw_rounded_box(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, float radius,
                          Gdiplus::Color bgColor, Gdiplus::Color borderColor, float borderWidth = 1.0f);
    void draw_circle_avatar(Gdiplus::Graphics& g, Gdiplus::Image* img, float x, float y, float size, const std::wstring& fallbackLetter);
    void draw_dock_vector_icon(Gdiplus::Graphics& g, int iconIdx, float cx, float cy, Gdiplus::Color col);
    float get_dock_start_y(float clientH);
    float get_dock_icon_y(int iconIdx, float clientH);
    void draw_right_tab_vector_icon(Gdiplus::Graphics& g, int tabIdx, float cx, float cy, Gdiplus::Color col);
    void draw_code_highlighted(Gdiplus::Graphics& g, const std::string& lang, const std::string& code,
                               const Gdiplus::RectF& rect, Gdiplus::Font& fontCode);
    void draw_rich_text_line(Gdiplus::Graphics& g, const std::string& line, float x, float y, float maxW,
                             Gdiplus::Font& fontReg, Gdiplus::Font& fontBold, Gdiplus::Font& fontMono);
    void render_window(HDC hdc, const RECT& rc);
    void render_left_dock(Gdiplus::Graphics& g, const RECT& rc);
    void render_top_bar(Gdiplus::Graphics& g, const RECT& rc);
    void render_right_panel(Gdiplus::Graphics& g, const RECT& rc);
    void render_chat_view(Gdiplus::Graphics& g, const RECT& rc);
    void render_arena_view(Gdiplus::Graphics& g, const RECT& rc);
    void render_topic_modal(Gdiplus::Graphics& g, const RECT& rc);
    void render_settings_view(Gdiplus::Graphics& g, const RECT& rc);
    void render_workspace_view(Gdiplus::Graphics& g, const RECT& rc);
    void render_window_controls(Gdiplus::Graphics& g, const RECT& rc);
    bool is_point_on_top_bar_control(int x, int y);

    // Helpers
    std::vector<TextPart> parse_markdown_blocks(const std::string& text);
    std::string build_system_prompt();
    std::string build_system_prompt(const std::string& agent_name);
    std::string get_current_time_str();
    std::string read_file_content(const std::string& path, const std::string& fallback = "");
    void write_file_content(const std::string& path, const std::string& content);

    // Win32 message loop thunk
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handle_message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

} // namespace gui
