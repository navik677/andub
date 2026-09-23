#include "theme_manager.hpp"
#include <algorithm>
#include <cmath>
#include "../utils/json.hpp"
#include <gtk/gtk.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace anime {

static GtkCssProvider* s_current_provider = nullptr;
static std::string s_active_theme_id = "adwaita";

static const std::vector<Theme> s_themes = {
    {
        "adwaita",
        "Adwaita Dark (GNOME)",
        "#1e1e1e",                     // bg
        "#242424",                     // header
        "#2a2a2a",                     // card
        "#323232",                     // card hover
        "#3584e4",                     // accent (GNOME blue)
        "#1c71d8",                     // accent hover
        "#ffffff",                     // text primary
        "#9a9996",                     // text secondary
        "rgba(255, 255, 255, 0.08)",   // border
        "#f6d32d",                     // badge bg
        "#1e1e1e",                     // badge text
        "rgba(255, 255, 255, 0.06)"    // chip bg
    },
    {
        "catppuccin",
        "Catppuccin Mocha",
        "#1e1e2e",                     // bg
        "#181825",                     // header
        "#25273a",                     // card
        "#313244",                     // card hover
        "#cba6f7",                     // accent (Mauve)
        "#b4befe",                     // accent hover
        "#cdd6f4",                     // text primary
        "#a6adc8",                     // text secondary
        "rgba(203, 166, 247, 0.16)",   // border
        "#f9e2af",                     // badge bg
        "#181825",                     // badge text
        "rgba(203, 166, 247, 0.08)"    // chip bg
    },
    {
        "nord",
        "Nord Arctic",
        "#2e3440",                     // bg
        "#242933",                     // header
        "#3b4252",                     // card
        "#434c5e",                     // card hover
        "#88c0d0",                     // accent (Frost)
        "#81a1c1",                     // accent hover
        "#eceff4",                     // text primary
        "#d8dee9",                     // text secondary
        "rgba(136, 192, 208, 0.16)",   // border
        "#ebcb8b",                     // badge bg
        "#2e3440",                     // badge text
        "rgba(136, 192, 208, 0.08)"    // chip bg
    },
    {
        "tokyo",
        "Tokyo Night",
        "#1a1b26",                     // bg
        "#16161e",                     // header
        "#24283b",                     // card
        "#2f354f",                     // card hover
        "#7aa2f7",                     // accent
        "#89b4fa",                     // accent hover
        "#c0caf5",                     // text primary
        "#9aa5ce",                     // text secondary
        "rgba(122, 162, 247, 0.18)",   // border
        "#e0af68",                     // badge bg
        "#1a1b26",                     // badge text
        "rgba(122, 162, 247, 0.08)"    // chip bg
    },
    {
        "gruvbox",
        "Gruvbox Dark",
        "#282828",                     // bg
        "#1d2021",                     // header
        "#32302f",                     // card
        "#3c3836",                     // card hover
        "#83a598",                     // accent (Muted Aqua Blue)
        "#b8bb26",                     // accent hover
        "#ebdbb2",                     // text primary
        "#a89984",                     // text secondary
        "rgba(255, 255, 255, 0.10)",   // border
        "#fabd2f",                     // badge bg
        "#282828",                     // badge text
        "rgba(255, 255, 255, 0.06)"    // chip bg
    },
    {
        "oled",
        "OLED Pure Black",
        "#000000",                     // bg
        "#080808",                     // header
        "#121212",                     // card
        "#1c1c1c",                     // card hover
        "#ffffff",                     // accent
        "#e0e0e0",                     // accent hover
        "#f5f5f5",                     // text primary
        "#888888",                     // text secondary
        "rgba(255, 255, 255, 0.12)",   // border
        "#ffd700",                     // badge bg
        "#000000",                     // badge text
        "rgba(255, 255, 255, 0.08)"    // chip bg
    }
};

const std::vector<Theme>& ThemeManager::get_themes() {
    return s_themes;
}

const Theme& ThemeManager::get_theme(const std::string& theme_id) {
    for (const auto& t : s_themes) {
        if (t.id == theme_id) return t;
    }
    return s_themes[0];
}

std::string ThemeManager::get_config_file() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::string base;
    if (xdg && *xdg) base = xdg;
    else if (home && *home) base = std::string(home) + "/.config";
    else base = "/tmp";

    std::string dir = base + "/anime-gui";
    try { std::filesystem::create_directories(dir); } catch (...) {}
    return dir + "/config.json";
}

std::string ThemeManager::get_current_theme_id() {
    std::string path = get_config_file();
    if (std::filesystem::exists(path)) {
        try {
            std::ifstream f(path);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            auto root = json::Value::parse(content);
            std::string th = root["theme"].get_str();
            if (!th.empty()) {
                s_active_theme_id = th;
                return th;
            }
        } catch (...) {}
    }
    return s_active_theme_id;
}

void ThemeManager::set_current_theme_id(const std::string& theme_id) {
    s_active_theme_id = theme_id;
    std::string path = get_config_file();
    json::Value root;
    if (std::filesystem::exists(path)) {
        try {
            std::ifstream f(path);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            root = json::Value::parse(content);
        } catch (...) {}
    }
    root["theme"] = theme_id;
    try {
        std::ofstream out(path);
        out << root.dump(2);
    } catch (...) {}
}

// Readable text color for content drawn on top of `hex` (#rrggbb).
static std::string contrast_text_for(const std::string& hex, const std::string& dark) {
    if (hex.size() != 7 || hex[0] != '#') return "#ffffff";
    auto channel = [&](size_t i) {
        double c = std::stoi(hex.substr(i, 2), nullptr, 16) / 255.0;
        return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    double lum = 0.2126 * channel(1) + 0.7152 * channel(3) + 0.0722 * channel(5);
    return lum > 0.35 ? dark : "#ffffff";
}

std::string ThemeManager::generate_css(const Theme& t) {
    const std::string on_accent = contrast_text_for(t.accent, t.bg_color);
    return R"CSS(
* {
    font-family: "Inter", "Cantarell", system-ui, -apple-system, "Segoe UI", "Noto Sans", sans-serif;
}

/* Global slider fix for GTK4 */
slider {
    min-width: 6px;
    min-height: 6px;
}

scrollbar > range > trough > slider {
    min-width: 6px;
    min-height: 6px;
}

window {
    background-color: )CSS" + t.bg_color + R"CSS(;
    color: )CSS" + t.text_primary + R"CSS(;
}

/* ===== Header Bar ===== */
headerbar {
    background-color: )CSS" + t.header_bg + R"CSS(;
    border-bottom: 1px solid rgba(255, 255, 255, 0.06);
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.25);
    color: )CSS" + t.text_primary + R"CSS(;
    padding: 6px 16px;
    min-height: 48px;
}

.brand-logo,
.brand-logo-btn {
    background: transparent;
    background-color: transparent;
    border: none;
    box-shadow: none;
    outline: none;
    font-size: 14px;
    font-weight: 900;
    letter-spacing: 2px;
    color: )CSS" + t.accent + R"CSS(;
    margin-right: 6px;
    padding: 3px 6px;
}

.brand-logo-btn label {
    font-size: 14px;
    font-weight: 900;
    letter-spacing: 2px;
    color: )CSS" + t.accent + R"CSS(;
    transition: color 150ms ease, transform 150ms ease;
}

.brand-logo-btn:hover label {
    color: )CSS" + t.accent_hover + R"CSS(;
}

.brand-logo-btn:hover {
    background: transparent;
    background-color: transparent;
    border: none;
    box-shadow: none;
}

/* Universal suppression of unwanted text/row selection backgrounds */
label selection,
label:selected,
label:hover selection,
label:focus selection,
label:backdrop selection {
    background-color: transparent;
    background: transparent;
    color: inherit;
}

button label,
button label:hover,
button label:selected,
button label:focus,
button label selection,
button box,
button box:hover,
button box:selected {
    background-color: transparent;
    background: transparent;
    color: inherit;
    outline: none;
    box-shadow: none;
}

/* Header Dropdowns (Providers, Genres, Themes) */
dropdown,
dropdown > button {
    outline: none;
    box-shadow: none;
}

dropdown > button:focus,
dropdown > button:focus-visible,
dropdown > button:active,
dropdown > button:checked {
    outline: none;
    box-shadow: none;
}

dropdown > button *,
dropdown > button *:hover,
dropdown > button *:focus,
dropdown > button *:active,
dropdown > button *:checked,
dropdown > button *:selected,
dropdown > button row,
dropdown > button row *,
dropdown > button row:hover,
dropdown > button row *:hover,
dropdown > button row.activatable,
dropdown > button row.activatable:hover,
dropdown > button row.activatable:active,
dropdown > button row.activatable:selected,
dropdown > button row.activatable:selected:hover,
dropdown > button row.activatable.has-open-popup,
dropdown > button box,
dropdown > button box:hover,
dropdown > button stack,
dropdown > button stack:hover,
dropdown > button stack row,
dropdown > button stack row:hover,
dropdown > button label,
dropdown > button label:hover,
dropdown > button label:selected,
dropdown > button label:focus,
dropdown > button label selection,
dropdown > button selection {
    background: transparent;
    background-color: transparent;
    background-image: none;
    color: inherit;
    outline: none;
    box-shadow: none;
    border: none;
}

.header-dropdown {
    background: transparent;
    outline: none;
    box-shadow: none;
}

.header-dropdown > button {
    background-color: rgba(255, 255, 255, 0.05);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 8px;
    color: )CSS" + t.text_primary + R"CSS(;
    font-size: 13px;
    font-weight: 500;
    padding: 5px 10px;
    outline: none;
    box-shadow: none;
    transition: all 160ms ease;
}

.header-dropdown > button:hover {
    background-color: rgba(255, 255, 255, 0.1);
    border-color: rgba(255, 255, 255, 0.18);
    transform: translateY(-1px);
}

.header-dropdown > button:focus,
.header-dropdown > button:focus-visible,
.header-dropdown > button:active,
.header-dropdown > button:checked {
    background-color: rgba(255, 255, 255, 0.08);
    border-color: )CSS" + t.accent + R"CSS(;
    outline: none;
    box-shadow: none;
}

.header-dropdown > button *,
.header-dropdown > button *:hover,
.header-dropdown > button *:focus,
.header-dropdown > button *:active,
.header-dropdown > button *:checked,
.header-dropdown > button *:selected,
.header-dropdown > button row,
.header-dropdown > button row *,
.header-dropdown > button row:hover,
.header-dropdown > button row *:hover,
.header-dropdown > button row.activatable,
.header-dropdown > button row.activatable:hover,
.header-dropdown > button row.activatable:active,
.header-dropdown > button row.activatable:selected,
.header-dropdown > button row.activatable:selected:hover,
.header-dropdown > button row.activatable.has-open-popup,
.header-dropdown > button box,
.header-dropdown > button box:hover,
.header-dropdown > button stack,
.header-dropdown > button stack:hover,
.header-dropdown > button stack row,
.header-dropdown > button stack row:hover,
.header-dropdown > button label,
.header-dropdown > button label:hover,
.header-dropdown > button label:selected,
.header-dropdown > button label:focus,
.header-dropdown > button label selection,
.header-dropdown > button selection {
    background: transparent;
    background-color: transparent;
    background-image: none;
    color: )CSS" + t.text_primary + R"CSS(;
    outline: none;
    box-shadow: none;
    border: none;
}

/* Header Navigation Toggle (Favorites) */
.nav-tab-btn {
    background-color: rgba(255, 255, 255, 0.05);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 8px;
    color: )CSS" + t.text_primary + R"CSS(;
    font-size: 13px;
    font-weight: 500;
    padding: 5px 11px;
    transition: all 160ms ease;
}

.nav-tab-btn:hover {
    background-color: rgba(255, 255, 255, 0.1);
    border-color: rgba(255, 255, 255, 0.18);
    transform: translateY(-1px);
}

.nav-tab-btn:checked,
.nav-tab-btn.active {
    background-color: )CSS" + t.accent + R"CSS(;
    color: )CSS" + on_accent + R"CSS(;
    font-weight: 600;
    border-color: transparent;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.35);
}

/* Header Search Entry */
.header-search {
    background-color: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.09);
    border-radius: 20px;
    color: )CSS" + t.text_primary + R"CSS(;
    font-size: 13px;
    padding: 5px 14px;
    transition: all 200ms ease;
}

.header-search:focus-within {
    border-color: )CSS" + t.accent + R"CSS(;
    background-color: rgba(255, 255, 255, 0.09);
    box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.06);
}

/* Header Action Buttons (Downloads, Player mode) */
.header-action-btn {
    background-color: rgba(255, 255, 255, 0.05);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 8px;
    color: )CSS" + t.text_primary + R"CSS(;
    font-size: 13px;
    font-weight: 500;
    padding: 5px 11px;
    transition: all 160ms ease;
}

.header-action-btn:hover {
    background-color: rgba(255, 255, 255, 0.1);
    border-color: rgba(255, 255, 255, 0.18);
    transform: translateY(-1px);
}

/* ===== Catalog FlowBox & Anime Cards ===== */
flowboxchild {
    background: transparent;
    padding: 4px;
    margin: 0;
    border-radius: 14px;
    outline: none;
    transition: transform 220ms cubic-bezier(0.16, 1, 0.3, 1);
}

flowboxchild:selected, flowboxchild:focus {
    background: transparent;
    outline: none;
    box-shadow: none;
}

flowboxchild:hover {
    background: transparent;
    outline: none;
    box-shadow: none;
    transform: translateY(-6px);
}

.anime-card {
    background: transparent;
}

/* Poster Frame */
.anime-poster-frame {
    border-radius: 12px;
    background-color: )CSS" + t.card_bg + R"CSS(;
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.38);
    border: 1px solid rgba(255, 255, 255, 0.07);
    transition: box-shadow 220ms ease, border-color 220ms ease;
}

flowboxchild:hover .anime-poster-frame {
    box-shadow: 0 14px 32px rgba(0, 0, 0, 0.6),
                0 0 0 1.5px )CSS" + t.accent + R"CSS(;
    border-color: transparent;
}

.anime-poster-frame picture {
    border-radius: 12px;
}

/* Badges on Poster */
.poster-rating {
    background: rgba(12, 14, 20, 0.82);
    backdrop-filter: blur(8px);
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 6px;
    padding: 2px 7px;
    font-size: 11px;
    font-weight: 700;
    color: )CSS" + t.badge_bg + R"CSS(;
    margin: 8px;
    letter-spacing: 0.3px;
    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.4);
}

.poster-age-badge {
    background: rgba(12, 14, 20, 0.78);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 4px;
    padding: 1px 5px;
    font-size: 10px;
    font-weight: 700;
    color: rgba(255, 255, 255, 0.85);
    margin: 8px;
}

/* Info Box Below Poster */
.card-info-box {
    margin-top: 4px;
}

.card-title {
    font-weight: 600;
    font-size: 13px;
    color: )CSS" + t.text_primary + R"CSS(;
    line-height: 1.3;
    transition: color 160ms ease;
}

flowboxchild:hover .card-title {
    color: )CSS" + t.accent + R"CSS(;
}

.card-meta-line {
    margin-top: 2px;
}

.card-year-chip {
    background-color: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-radius: 4px;
    padding: 1px 6px;
    font-size: 10.5px;
    font-weight: 500;
    color: )CSS" + t.text_secondary + R"CSS(;
}

.card-prov-chip {
    font-size: 11px;
    font-weight: 500;
    color: )CSS" + t.text_secondary + R"CSS(;
    padding: 1px 2px;
}

.chip-tag {
    background-color: )CSS" + t.chip_bg + R"CSS(;
    color: )CSS" + t.text_secondary + R"CSS(;
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-radius: 4px;
    padding: 1px 6px;
    font-size: 10.5px;
}

/* ===== Pagination ===== */
.page-btn {
    background-color: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(255, 255, 255, 0.08);
    color: )CSS" + t.text_secondary + R"CSS(;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 600;
    min-width: 38px;
    min-height: 38px;
    padding: 0 12px;
    transition: all 160ms ease;
}

.page-btn:hover {
    background-color: rgba(255, 255, 255, 0.1);
    color: #ffffff;
    border-color: rgba(255, 255, 255, 0.2);
    transform: translateY(-1px);
}

.page-btn.active {
    background-color: )CSS" + t.accent + R"CSS(;
    color: )CSS" + on_accent + R"CSS(;
    font-weight: 700;
    border-color: transparent;
    box-shadow: 0 3px 12px rgba(0, 0, 0, 0.35);
}

/* ===== Details View ===== */
.details-title {
    font-weight: 800;
    font-size: 24px;
    color: )CSS" + t.text_primary + R"CSS(;
    line-height: 1.25;
}

.details-desc {
    font-size: 13.5px;
    line-height: 1.6;
    color: )CSS" + t.text_secondary + R"CSS(;
}

.genre-chip {
    background-color: )CSS" + t.chip_bg + R"CSS(;
    color: )CSS" + t.text_primary + R"CSS(;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    border-radius: 14px;
    padding: 4px 12px;
    font-size: 12px;
    font-weight: 500;
    transition: transform 160ms cubic-bezier(0.2, 0.8, 0.2, 1),
                background-color 160ms ease,
                border-color 160ms ease;
}

.genre-chip:hover {
    transform: translateY(-2px);
    border-color: )CSS" + t.accent + R"CSS(;
    box-shadow: 0 4px 10px rgba(0, 0, 0, 0.3);
}

.episode-row {
    background-color: )CSS" + t.card_bg + R"CSS(;
    border-radius: 10px;
    padding: 10px 16px;
    margin: 0;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    transition: transform 160ms cubic-bezier(0.2, 0.9, 0.3, 1),
                background-color 160ms ease,
                border-color 160ms ease,
                box-shadow 160ms ease;
}

.episode-row:hover {
    transform: translateX(6px);
    background-color: )CSS" + t.card_hover + R"CSS(;
    border-color: )CSS" + t.accent + R"CSS(;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.3);
}

.watched-badge {
    color: #57e389;
    font-weight: 800;
    font-size: 14px;
}

/* ===== Buttons ===== */
button {
    background-color: )CSS" + t.chip_bg + R"CSS(;
    background-image: none;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    color: )CSS" + t.text_primary + R"CSS(;
    box-shadow: none;
    border-radius: 8px;
    font-weight: 500;
    transition: transform 140ms cubic-bezier(0.2, 0.9, 0.3, 1),
                box-shadow 180ms ease,
                background-color 180ms ease,
                border-color 180ms ease;
}

button:hover {
    background-color: )CSS" + t.card_hover + R"CSS(;
    border-color: )CSS" + t.accent + R"CSS(;
    transform: translateY(-1px);
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
}

button:disabled {
    opacity: 0.5;
}

button:focus-visible {
    outline: 2px solid )CSS" + t.accent + R"CSS(;
    outline-offset: 1px;
}

/* Window controls keep a minimal look */
windowcontrols button {
    background-color: transparent;
    border: none;
    box-shadow: none;
    color: )CSS" + t.text_secondary + R"CSS(;
    min-width: 24px;
    min-height: 24px;
    padding: 2px;
}

windowcontrols button:hover {
    background-color: )CSS" + t.chip_bg + R"CSS(;
    color: )CSS" + t.text_primary + R"CSS(;
    transform: none;
    box-shadow: none;
}

.header-section,
.header-section > viewport {
    background: transparent;
}

button:active {
    transform: translateY(1px) scale(0.97);
}

button.suggested-action {
    background-color: )CSS" + t.accent + R"CSS(;
    background-image: none;
    color: )CSS" + on_accent + R"CSS(;
    font-weight: 600;
    border-radius: 8px;
    padding: 7px 18px;
    border: none;
}

button.suggested-action:hover {
    background-color: )CSS" + t.accent_hover + R"CSS(;
    border-color: transparent;
    box-shadow: 0 6px 18px rgba(0, 0, 0, 0.4);
}

button.flat {
    background: transparent;
    border: none;
    color: )CSS" + t.text_primary + R"CSS(;
    border-radius: 8px;
}

button.flat:hover {
    background-color: )CSS" + t.chip_bg + R"CSS(;
}

/* ===== Dropdowns ===== */
dropdown {
    border-radius: 8px;
    transition: transform 140ms ease, box-shadow 180ms ease;
}

dropdown:hover {
    transform: translateY(-1px);
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.25);
}

/* ===== Search ===== */
searchentry, entry {
    background-color: )CSS" + t.card_bg + R"CSS(;
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-radius: 20px;
    color: )CSS" + t.text_primary + R"CSS(;
    padding: 6px 16px;
    transition: box-shadow 200ms ease, border-color 200ms ease;
}

searchentry:focus-within, entry:focus {
    border-color: )CSS" + t.accent + R"CSS(;
    box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.06);
}

progressbar > trough {
    background-color: )CSS" + t.chip_bg + R"CSS(;
    border-radius: 6px;
    min-height: 6px;
}

progressbar > trough > progress {
    background-color: )CSS" + t.accent + R"CSS(;
    border-radius: 6px;
}

.details-bg-wrapper {
    background-color: transparent;
}

.details-backdrop {
}

.details-bg-overlay {
    background: linear-gradient(180deg, 
        rgba(15, 17, 26, 0.60) 0%, 
        rgba(15, 17, 26, 0.76) 42%, 
        )CSS" + t.bg_color + R"CSS( 98%);
}

/* ===== Embedded Player ===== */
.player-root {
    background-color: #000000;
}

.player-ui-overlay {
    transition: opacity 200ms cubic-bezier(0.4, 0, 0.2, 1);
    opacity: 1;
}

.player-ui-overlay.controls-hidden {
    opacity: 0;
}

.player-bar-top {
    background: linear-gradient(180deg, rgba(14, 16, 22, 0.88) 0%, rgba(14, 16, 22, 0.40) 70%, transparent 100%);
    padding: 14px 20px 24px 20px;
}

.player-bar-bottom {
    background: linear-gradient(0deg, rgba(14, 16, 22, 0.92) 0%, rgba(14, 16, 22, 0.45) 70%, transparent 100%);
    padding: 20px 20px 14px 20px;
}

.player-title {
    font-size: 14px;
    font-weight: 500;
    color: #e2e8f0;
    text-shadow: 0 1px 3px rgba(0, 0, 0, 0.8);
}

.player-badge {
    background-color: rgba(255, 255, 255, 0.08);
    color: #94a3b8;
    border-radius: 4px;
    padding: 2px 7px;
    font-size: 11px;
    font-weight: 500;
    letter-spacing: 0.3px;
}

.player-btn-mpv {
    background: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.12);
    color: #cbd5e1;
    border-radius: 6px;
    padding: 5px 12px;
    font-size: 12px;
    font-weight: 500;
}

.player-btn-mpv:hover {
    background: rgba(255, 255, 255, 0.12);
    border-color: rgba(255, 255, 255, 0.22);
    color: #ffffff;
}

.player-btn {
    background: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.10);
    color: #cbd5e1;
    border-radius: 6px;
    padding: 5px 12px;
    font-size: 12px;
    font-weight: 500;
}

.player-btn:hover {
    background: rgba(255, 255, 255, 0.12);
    color: #ffffff;
}

.player-btn-play {
    background: rgba(255, 255, 255, 0.12);
    border: 1px solid rgba(255, 255, 255, 0.16);
    color: #ffffff;
    border-radius: 6px;
    min-width: 36px;
    min-height: 32px;
    padding: 4px 8px;
}

.player-btn-play:hover {
    background: rgba(255, 255, 255, 0.20);
    color: #ffffff;
}

.player-btn-icon {
    background: transparent;
    border: none;
    color: #94a3b8;
    border-radius: 6px;
    min-width: 32px;
    min-height: 32px;
    padding: 4px 6px;
}

.player-btn-icon:hover {
    background: rgba(255, 255, 255, 0.08);
    color: #f1f5f9;
}

.player-btn-text {
    background: transparent;
    border: none;
    color: #94a3b8;
    border-radius: 6px;
    padding: 4px 8px;
    font-size: 12px;
    font-weight: 500;
}

.player-btn-text:hover {
    background: rgba(255, 255, 255, 0.08);
    color: #f1f5f9;
}

.player-btn-chip {
    background: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.12);
    color: #cbd5e1;
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 500;
}

.player-btn-chip:hover {
    background: rgba(255, 255, 255, 0.12);
    border-color: rgba(255, 255, 255, 0.22);
    color: #ffffff;
}

.player-time {
    color: #94a3b8;
    font-size: 12px;
    font-family: monospace;
    font-weight: 400;
}

.player-osd {
    background: rgba(14, 16, 22, 0.85);
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 6px;
    padding: 8px 18px;
    color: #e2e8f0;
    font-size: 14px;
    font-weight: 500;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.5);
}

.player-slider scale > trough {
    background-color: rgba(255, 255, 255, 0.15);
    border-radius: 2px;
    min-height: 4px;
}

.player-slider scale > trough > highlight {
    background-color: )CSS" + t.accent + R"CSS(;
    border-radius: 2px;
}

.player-slider scale > trough > slider {
    min-width: 14px;
    min-height: 14px;
    margin: -5px;
    border-radius: 50%;
    background-color: #ffffff;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.4);
}

.player-vol-slider scale > trough {
    background-color: rgba(255, 255, 255, 0.15);
    border-radius: 2px;
    min-height: 4px;
}

.player-vol-slider scale > trough > highlight {
    background-color: )CSS" + t.accent + R"CSS(;
    border-radius: 2px;
}

/* Ambient Blurred Background & Catalog Overlay */
.catalog-overlay {
    background-color: )CSS" + t.bg_color + R"CSS(;
}

.catalog-backdrop-scrim {
    background: linear-gradient(180deg, rgba(16, 18, 27, 0.70) 0%, rgba(16, 18, 27, 0.86) 100%);
}

.catalog-content-box {
    background: transparent;
    background-color: transparent;
}

.catalog-scrolled,
.catalog-scrolled > viewport,
.catalog-scrolled viewport {
    background: transparent;
    background-color: transparent;
}

/* Favorites View Banner & Exit Button */
.favorites-banner {
    background-color: )CSS" + t.card_bg + R"CSS(;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    border-radius: 12px;
    padding: 10px 18px;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.25);
}

.favorites-banner-title {
    font-size: 15px;
    font-weight: 700;
    color: )CSS" + t.badge_bg + R"CSS(;
    letter-spacing: 0.5px;
}

.fav-exit-btn {
    background-color: rgba(255, 255, 255, 0.08);
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    border-radius: 8px;
    color: )CSS" + t.accent + R"CSS(;
    font-size: 13px;
    font-weight: 600;
    padding: 6px 14px;
    transition: all 160ms ease;
}

.fav-exit-btn:hover {
    background-color: )CSS" + t.accent + R"CSS(;
    color: )CSS" + on_accent + R"CSS(;
    border-color: transparent;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.35);
    transform: translateY(-1px);
}

.fav-empty-btn {
    background-color: )CSS" + t.accent + R"CSS(;
    color: )CSS" + on_accent + R"CSS(;
    font-size: 14px;
    font-weight: 600;
    border-radius: 10px;
    border: none;
    padding: 10px 22px;
    margin-top: 8px;
    transition: all 160ms ease;
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.3);
}

.fav-empty-btn:hover {
    background-color: )CSS" + t.accent_hover + R"CSS(;
    transform: translateY(-1px);
    box-shadow: 0 6px 18px rgba(0, 0, 0, 0.4);
}

/* ===== Responsive (windowed) layout ===== */
window.layout-compact headerbar {
    padding: 6px 10px;
}

window.layout-narrow headerbar {
    padding: 4px 6px;
}

window.layout-narrow .nav-tab-btn,
window.layout-narrow .header-action-btn {
    padding: 5px 8px;
}

window.layout-compact .details-title {
    font-size: 21px;
}

window.layout-narrow .details-title {
    font-size: 19px;
}

window.layout-narrow .details-desc {
    font-size: 13px;
}

window.layout-narrow .episode-row {
    padding: 8px 10px;
}

/* ===== Entrance & state animations ===== */
@keyframes rise-in {
    from { opacity: 0; transform: translateY(18px) scale(0.97); }
    to   { opacity: 1; transform: none; }
}

@keyframes slide-in-left {
    from { opacity: 0; transform: translateX(-24px); }
    to   { opacity: 1; transform: none; }
}

@keyframes slide-in-right {
    from { opacity: 0; transform: translateX(24px); }
    to   { opacity: 1; transform: none; }
}

@keyframes skeleton-pulse {
    from { background-color: )CSS" + t.card_bg + R"CSS(; }
    to   { background-color: )CSS" + t.card_hover + R"CSS(; }
}

@keyframes pop-a {
    0%   { transform: scale(1); }
    40%  { transform: scale(1.08); }
    100% { transform: scale(1); }
}

@keyframes pop-b {
    0%   { transform: scale(1); }
    40%  { transform: scale(1.08); }
    100% { transform: scale(1); }
}

.anime-card.enter {
    animation: rise-in 420ms cubic-bezier(0.16, 1, 0.3, 1) backwards;
}

.episode-row.enter {
    animation: slide-in-right 320ms cubic-bezier(0.16, 1, 0.3, 1) backwards;
}

)CSS" + stagger_css() + R"CSS(

.anime-poster-frame.loading,
.details-poster-frame.loading {
    animation: skeleton-pulse 900ms ease-in-out infinite alternate;
}

.poster-image {
    opacity: 0;
    transition: opacity 360ms ease-out;
}

.poster-image.loaded {
    opacity: 1;
}

.details-poster-frame {
    border-radius: 14px;
    background-color: )CSS" + t.card_bg + R"CSS(;
    border: 1px solid rgba(255, 255, 255, 0.07);
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.45);
}

.details-poster-frame picture {
    border-radius: 14px;
}

.details-enter-left {
    animation: slide-in-left 460ms cubic-bezier(0.16, 1, 0.3, 1) backwards;
}

.details-enter-right {
    animation: slide-in-right 460ms cubic-bezier(0.16, 1, 0.3, 1) 80ms backwards;
}

.details-topbar {
    animation: rise-in 360ms cubic-bezier(0.16, 1, 0.3, 1) backwards;
}

button.fav-toggle.is-fav {
    background-color: )CSS" + t.accent + R"CSS(;
    color: )CSS" + on_accent + R"CSS(;
    border-color: transparent;
}

button.fav-toggle.is-fav:hover {
    background-color: )CSS" + t.accent_hover + R"CSS(;
}

button.fav-toggle.pop-a {
    animation: pop-a 320ms cubic-bezier(0.2, 0.9, 0.3, 1.4);
}

button.fav-toggle.pop-b {
    animation: pop-b 320ms cubic-bezier(0.2, 0.9, 0.3, 1.4);
}

/* Existing results dim while a new page/search is loading */
.catalog-grid {
    transition: opacity 220ms ease;
}

.catalog-grid.refreshing {
    opacity: 0.35;
}

/* Player OSD fades and scales instead of popping */
.player-osd {
    transition: opacity 180ms ease, transform 180ms cubic-bezier(0.2, 0.9, 0.3, 1.2);
    opacity: 1;
    transform: none;
}

.player-osd.osd-hidden {
    opacity: 0;
    transform: scale(0.92);
}
)CSS";
}

std::string ThemeManager::stagger_class(size_t index) {
    size_t step = std::min(index, static_cast<size_t>(STAGGER_STEPS - 1));
    return "stagger-" + std::to_string(step);
}

std::string ThemeManager::stagger_css() {
    // Delay classes for staggered list entrances; later items share the last step
    std::string css;
    for (int i = 0; i < STAGGER_STEPS; ++i) {
        css += ".enter.stagger-" + std::to_string(i) + " { animation-delay: " + std::to_string(i * 35) + "ms; }\n";
    }
    return css;
}

void ThemeManager::apply_theme(const std::string& theme_id) {
    const auto& theme = get_theme(theme_id);
    set_current_theme_id(theme.id);

    GdkDisplay* display = gdk_display_get_default();
    if (!display) return;

    if (s_current_provider) {
        gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(s_current_provider));
        g_object_unref(s_current_provider);
        s_current_provider = nullptr;
    }

    s_current_provider = gtk_css_provider_new();
    std::string css = generate_css(theme);
    gtk_css_provider_load_from_string(s_current_provider, css.c_str());

    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(s_current_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
}

} // namespace anime
