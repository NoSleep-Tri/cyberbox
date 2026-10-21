#pragma once
#include "ui_types.hpp"
#include <string>

namespace cyber_ui {

struct ThemeTokens {
    // Backgrounds
    UIColor bg_canvas           = UIColor::from_hex(0x0b0c0e);
    UIColor bg_dock             = UIColor::from_hex(0x131418);
    UIColor bg_topbar           = UIColor::from_hex(0x111215);
    UIColor bg_card             = UIColor::from_hex(0x181a20);
    UIColor bg_card_secondary   = UIColor::from_hex(0x1c1d24);
    UIColor bg_input            = UIColor::from_hex(0x14151a);
    UIColor bg_hover            = UIColor::from_hex(0x232630);
    UIColor bg_active           = UIColor::from_hex(0x2a2e3b);

    // Borders
    UIColor border_subtle       = UIColor::from_hex(0x22252e);
    UIColor border_medium       = UIColor::from_hex(0x2e323e);
    UIColor border_card         = UIColor::from_hex(0x262933);
    UIColor border_accent       = UIColor::from_hex(0x00d2ff, 0.6f);
    UIColor border_focus        = UIColor::from_hex(0x38bdf8);

    // Text
    UIColor text_primary        = UIColor::from_hex(0xf1f3f7);
    UIColor text_secondary      = UIColor::from_hex(0x9ca3af);
    UIColor text_muted          = UIColor::from_hex(0x606775);
    UIColor text_accent         = UIColor::from_hex(0x38bdf8);

    // Accents / Brand
    UIColor accent_cyan         = UIColor::from_hex(0x00d2ff);
    UIColor accent_sky          = UIColor::from_hex(0x38bdf8);
    UIColor accent_purple       = UIColor::from_hex(0x8b5cf6);
    UIColor accent_violet       = UIColor::from_hex(0xa855f7);
    UIColor accent_emerald      = UIColor::from_hex(0x10b981);
    UIColor accent_amber        = UIColor::from_hex(0xf59e0b);
    UIColor accent_rose         = UIColor::from_hex(0xef4444);

    // Corner Radii
    float radius_xs = 3.0f;
    float radius_sm = 6.0f;
    float radius_md = 10.0f;
    float radius_lg = 14.0f;
    float radius_pill = 999.0f;

    // Font specs
    std::wstring font_family_ui   = L"Segoe UI";
    std::wstring font_family_code = L"Consolas";
    float font_size_small = 11.0f;
    float font_size_body  = 13.0f;
    float font_size_title = 16.0f;
    float font_size_h1    = 20.0f;

    static ThemeTokens make_cyberpunk_dark() {
        return ThemeTokens();
    }

    static ThemeTokens make_oled_black() {
        ThemeTokens t;
        t.bg_canvas = UIColor::from_hex(0x000000);
        t.bg_dock   = UIColor::from_hex(0x050505);
        t.bg_topbar = UIColor::from_hex(0x050505);
        t.bg_card   = UIColor::from_hex(0x0c0c0e);
        t.bg_card_secondary = UIColor::from_hex(0x121214);
        t.bg_input  = UIColor::from_hex(0x08080a);
        t.border_subtle = UIColor::from_hex(0x1a1a1f);
        t.border_card   = UIColor::from_hex(0x202026);
        return t;
    }
};

class UITheme {
public:
    static UITheme& get() {
        static UITheme instance;
        return instance;
    }

    const ThemeTokens& tokens() const { return tokens_; }
    ThemeTokens& tokens() { return tokens_; }

    void set_tokens(const ThemeTokens& tokens) {
        tokens_ = tokens;
    }

private:
    UITheme() : tokens_(ThemeTokens::make_cyberpunk_dark()) {}
    ThemeTokens tokens_;
};

inline const ThemeTokens& theme() {
    return UITheme::get().tokens();
}

} // namespace cyber_ui
