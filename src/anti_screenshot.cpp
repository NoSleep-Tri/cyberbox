#include "anti_screenshot.hpp"
#include <iostream>
#include <algorithm>

namespace security {

typedef BOOL (WINAPI *pfnSetWindowDisplayAffinity)(HWND hWnd, DWORD dwAffinity);

AntiScreenshot& AntiScreenshot::instance() {
    static AntiScreenshot inst;
    return inst;
}

AntiScreenshot::AntiScreenshot() = default;

AntiScreenshot::~AntiScreenshot() {
    shutdown();
}

bool AntiScreenshot::initialize(HWND hwnd, AntiCaptureLevel level) {
    hwnd_ = hwnd;
    level_ = level;

    if (level_ == AntiCaptureLevel::DISABLED) {
        enabled_ = false;
        return true;
    }

    return set_level(level_);
}

void AntiScreenshot::shutdown() {
    stop_process_watchdog();
    remove_keyboard_hook();

    if (hwnd_) {
        // Remove clipboard listener if registered
        typedef BOOL (WINAPI *pfnRemoveClipboardFormatListener)(HWND);
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            pfnRemoveClipboardFormatListener pRemove =
                (pfnRemoveClipboardFormatListener)GetProcAddress(hUser32, "RemoveClipboardFormatListener");
            if (pRemove) {
                pRemove(hwnd_);
            }
        }

        // Restore normal window display affinity
        apply_display_affinity(WDA_NONE);
    }

    enabled_ = false;
    wda_active_ = false;
}

bool AntiScreenshot::set_level(AntiCaptureLevel level) {
    level_ = level;

    if (level_ == AntiCaptureLevel::DISABLED) {
        shutdown();
        return true;
    }

    enabled_ = true;

    // 1. Hardware DWM Level Display Affinity
    DWORD targetAffinity = WDA_EXCLUDEFROMCAPTURE;
    if (!apply_display_affinity(targetAffinity)) {
        // Fall back to WDA_MONITOR (Windows 7/8/early 10)
        targetAffinity = WDA_MONITOR;
        apply_display_affinity(targetAffinity);
    }

    // 2. High-Grade Protection (SEB Paranoid)
    if (level_ == AntiCaptureLevel::SEB_PARANOID) {
        install_keyboard_hook();

        // Register Clipboard Format Listener to auto-nuke intercepted screenshot bitmaps
        typedef BOOL (WINAPI *pfnAddClipboardFormatListener)(HWND);
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32 && hwnd_) {
            pfnAddClipboardFormatListener pAdd =
                (pfnAddClipboardFormatListener)GetProcAddress(hUser32, "AddClipboardFormatListener");
            if (pAdd) {
                pAdd(hwnd_);
            }
        }

        start_process_watchdog();
    } else {
        remove_keyboard_hook();
        stop_process_watchdog();
    }

    if (on_status_changed_) {
        on_status_changed_(get_status());
    }

    return true;
}

void AntiScreenshot::toggle() {
    if (enabled_) {
        set_level(AntiCaptureLevel::DISABLED);
    } else {
        set_level(AntiCaptureLevel::SEB_PARANOID);
    }
}

bool AntiScreenshot::apply_display_affinity(DWORD affinity) {
    if (!hwnd_ || !IsWindow(hwnd_)) return false;

    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (!hUser32) hUser32 = LoadLibraryW(L"user32.dll");
    if (!hUser32) return false;

    pfnSetWindowDisplayAffinity pSetAffinity =
        (pfnSetWindowDisplayAffinity)GetProcAddress(hUser32, "SetWindowDisplayAffinity");
    if (!pSetAffinity) return false;

    BOOL ok = pSetAffinity(hwnd_, affinity);
    if (ok) {
        current_wda_mode_ = affinity;
        wda_active_ = (affinity != WDA_NONE);
        return true;
    }
    return false;
}

bool AntiScreenshot::install_keyboard_hook() {
    if (keyboard_hook_) return true;

    keyboard_hook_ = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        AntiScreenshot::LowLevelKeyboardProc,
        GetModuleHandleW(NULL),
        0
    );

    return (keyboard_hook_ != NULL);
}

void AntiScreenshot::remove_keyboard_hook() {
    if (keyboard_hook_) {
        UnhookWindowsHookEx(keyboard_hook_);
        keyboard_hook_ = NULL;
    }
}

LRESULT CALLBACK AntiScreenshot::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (pKey) {
            auto& inst = AntiScreenshot::instance();
            if (inst.enabled_) {
                // 1. Intercept PrintScreen (VK_SNAPSHOT)
                if (pKey->vkCode == VK_SNAPSHOT) {
                    inst.blocked_snapshot_count_++;
                    inst.wipe_clipboard();
                    return 1; // Eat keystroke!
                }

                // 2. Intercept Win + Shift + S (Windows Snipping Tool)
                bool winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);
                bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000);
                if (winDown && shiftDown && (pKey->vkCode == 'S' || pKey->vkCode == 's')) {
                    inst.blocked_snapshot_count_++;
                    inst.wipe_clipboard();
                    return 1; // Eat keystroke!
                }

                // 3. Intercept Win + G (Xbox Game Bar Screen Overlay)
                if (winDown && (pKey->vkCode == 'G' || pKey->vkCode == 'g')) {
                    inst.blocked_snapshot_count_++;
                    return 1; // Eat keystroke!
                }

                // 4. Intercept Win + Alt + R (Game Bar screen record toggle)
                bool altDown = (pKey->flags & LLKHF_ALTDOWN) || (GetAsyncKeyState(VK_MENU) & 0x8000);
                if (winDown && altDown && (pKey->vkCode == 'R' || pKey->vkCode == 'r' || pKey->vkCode == VK_SNAPSHOT)) {
                    inst.blocked_snapshot_count_++;
                    return 1; // Eat keystroke!
                }
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void AntiScreenshot::wipe_clipboard() {
    if (OpenClipboard(hwnd_)) {
        EmptyClipboard();
        CloseClipboard();
        clipboard_wipe_count_++;
    }
}

void AntiScreenshot::handle_display_change() {
    // Re-assert DWM display affinity upon screen resolution or monitor changes
    if (enabled_ && current_wda_mode_ != WDA_NONE) {
        apply_display_affinity(current_wda_mode_);
    }
}

void AntiScreenshot::handle_clipboard_update() {
    if (!enabled_ || level_ != AntiCaptureLevel::SEB_PARANOID) return;

    // If an external screenshot tool placed a raw bitmap on the clipboard, purge immediately
    if (IsClipboardFormatAvailable(CF_BITMAP) ||
        IsClipboardFormatAvailable(CF_DIB) ||
        IsClipboardFormatAvailable(CF_DIBV5)) {
        wipe_clipboard();
    }
}

void AntiScreenshot::handle_focus_change(bool has_focus) {
    if (!enabled_ || level_ != AntiCaptureLevel::SEB_PARANOID) return;

    // Purge clipboard on window blur to prevent copy/paste exfiltration
    if (!has_focus) {
        if (IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB)) {
            wipe_clipboard();
        }
    }
}

void AntiScreenshot::start_process_watchdog() {
    if (watchdog_running_) return;

    watchdog_running_ = true;
    watchdog_thread_ = std::thread(&AntiScreenshot::watchdog_loop, this);
}

void AntiScreenshot::stop_process_watchdog() {
    if (watchdog_running_) {
        watchdog_running_ = false;
        if (watchdog_thread_.joinable()) {
            watchdog_thread_.join();
        }
    }
}

void AntiScreenshot::watchdog_loop() {
    // Known screen recorders, snipping tools, and streaming capture hooks
    const std::vector<std::wstring> suspect_tools = {
        L"snippingtool.exe",
        L"screensketch.exe",
        L"obs64.exe",
        L"obs32.exe",
        L"obs.exe",
        L"sharex.exe",
        L"greenshot.exe",
        L"lightshot.exe",
        L"bandicam.exe",
        L"fraps.exe",
        L"camtasia.exe",
        L"camtasiastudio.exe",
        L"snagit32.exe",
        L"snagit64.exe",
        L"snagiteditor.exe",
        L"screentogif.exe",
        L"action.exe",
        L"loom.exe",
        L"captura.exe"
    };

    while (watchdog_running_) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        bool found = false;
        std::string foundName;

        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);

            if (Process32FirstW(hSnap, &pe32)) {
                do {
                    std::wstring exeName = pe32.szExeFile;
                    std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::towlower);

                    for (const auto& suspect : suspect_tools) {
                        if (exeName == suspect) {
                            found = true;
                            int needed = WideCharToMultiByte(CP_UTF8, 0, exeName.c_str(), -1, NULL, 0, NULL, NULL);
                            if (needed > 0) {
                                std::string s(needed - 1, 0);
                                WideCharToMultiByte(CP_UTF8, 0, exeName.c_str(), -1, &s[0], needed, NULL, NULL);
                                foundName = s;
                            }
                            break;
                        }
                    }
                    if (found) break;
                } while (Process32NextW(hSnap, &pe32));
            }
            CloseHandle(hSnap);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            bool state_changed = (capture_tool_detected_ != found) || (detected_tool_name_ != foundName);
            capture_tool_detected_ = found;
            detected_tool_name_ = foundName;

            if (found && enabled_) {
                // If a capture tool launched, strictly re-enforce DWM affinity
                apply_display_affinity(current_wda_mode_);
            }

            if (state_changed && on_status_changed_) {
                on_status_changed_(get_status());
            }
        }

        // Check every 2.5 seconds (minimal CPU overhead)
        for (int i = 0; i < 25 && watchdog_running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

AntiCaptureStatus AntiScreenshot::get_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    AntiCaptureStatus st;
    st.enabled = enabled_;
    st.level = level_;
    st.wda_active = wda_active_;
    st.wda_mode = current_wda_mode_;
    st.keyboard_hook_active = (keyboard_hook_ != NULL);
    st.clipboard_listener_active = enabled_;
    st.capture_tool_detected = capture_tool_detected_;
    st.detected_tool_name = detected_tool_name_;
    st.blocked_snapshot_count = blocked_snapshot_count_;
    st.clipboard_wipe_count = clipboard_wipe_count_;
    return st;
}

} // namespace security
