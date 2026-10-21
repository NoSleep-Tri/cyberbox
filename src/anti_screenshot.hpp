#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

// Standard Windows Display Affinity constants
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_MONITOR
#define WDA_MONITOR 0x00000001
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace security {

enum class AntiCaptureLevel {
    DISABLED = 0,
    STANDARD = 1,      // Hardware DWM Window Display Affinity only
    SEB_PARANOID = 2   // Full SEB: DWM Affinity + Keyboard Hook + Clipboard Guard + Process Watchdog
};

struct AntiCaptureStatus {
    bool enabled = false;
    AntiCaptureLevel level = AntiCaptureLevel::SEB_PARANOID;
    bool wda_active = false;
    DWORD wda_mode = 0;
    bool keyboard_hook_active = false;
    bool clipboard_listener_active = false;
    bool capture_tool_detected = false;
    std::string detected_tool_name;
    uint64_t blocked_snapshot_count = 0;
    uint64_t clipboard_wipe_count = 0;
};

class AntiScreenshot {
public:
    static AntiScreenshot& instance();

    bool initialize(HWND hwnd, AntiCaptureLevel level = AntiCaptureLevel::SEB_PARANOID);
    void shutdown();

    bool set_level(AntiCaptureLevel level);
    AntiCaptureLevel get_level() const { return level_; }

    bool is_enabled() const { return enabled_; }
    void toggle();

    AntiCaptureStatus get_status() const;

    // Called on window message pump
    void handle_display_change();
    void handle_clipboard_update();
    void handle_focus_change(bool has_focus);

    // Manual clipboard purge
    void wipe_clipboard();

    // Callbacks for UI updates
    void set_on_status_changed(std::function<void(const AntiCaptureStatus&)> cb) {
        on_status_changed_ = cb;
    }

private:
    AntiScreenshot();
    ~AntiScreenshot();

    bool apply_display_affinity(DWORD affinity);
    bool install_keyboard_hook();
    void remove_keyboard_hook();

    void start_process_watchdog();
    void stop_process_watchdog();
    void watchdog_loop();

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = NULL;
    std::atomic<bool> enabled_{false};
    AntiCaptureLevel level_ = AntiCaptureLevel::SEB_PARANOID;

    DWORD current_wda_mode_ = WDA_NONE;
    bool wda_active_ = false;

    HHOOK keyboard_hook_ = NULL;
    std::atomic<uint64_t> blocked_snapshot_count_{0};
    std::atomic<uint64_t> clipboard_wipe_count_{0};

    // Process Watchdog
    std::atomic<bool> watchdog_running_{false};
    std::thread watchdog_thread_;
    std::atomic<bool> capture_tool_detected_{false};
    std::string detected_tool_name_;
    mutable std::mutex mutex_;

    std::function<void(const AntiCaptureStatus&)> on_status_changed_;
};

} // namespace security
