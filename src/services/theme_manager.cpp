#include "theme_manager.hpp"
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
        "#fe8019",                     // accent (Orange)
        "#fabd2f",                     // accent hover
        "#ebdbb2",                     // text primary
        "#a89984",                     // text secondary
        "rgba(254, 128, 25, 0.18)",    // border
        "#fabd2f",                     // badge bg
        "#282828",                     // badge text
        "rgba(254, 128, 25, 0.08)"     // chip bg
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

std::string ThemeManager::generate_css(const Theme& t) {
    return R"CSS(
* {
    font-family: system-ui, -apple-system, "Segoe UI", "Noto Sans", "Cantarell", sans-serif;
}

window {
    background-color: )CSS" + t.bg_color + R"CSS(;
    color: )CSS" + t.text_primary + R"CSS(;
}

headerbar {
    background-color: )CSS" + t.header_bg + R"CSS(;
    border-bottom: 1px solid )CSS" + t.border_color + R"CSS(;
    color: )CSS" + t.text_primary + R"CSS(;
    padding: 8px 16px;
}

flowboxchild {
    background: transparent;
    padding: 0;
    margin: 0;
    border-radius: 12px;
}

flowboxchild:selected, flowboxchild:focus {
    background: transparent;
    outline: none;
}

.anime-card {
    min-width: 210px;
    background-color: )CSS" + t.card_bg + R"CSS(;
    border-radius: 12px;
    padding: 8px;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    transition: background-color 150ms ease, border-color 150ms ease, box-shadow 150ms ease;
}

.anime-card:hover {
    background-color: )CSS" + t.card_hover + R"CSS(;
    border-color: )CSS" + t.accent + R"CSS(;
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.35);
}

.anime-card picture {
    border-radius: 8px;
}

.poster-overlay-rating {
    background-color: rgba(18, 18, 24, 0.85);
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 6px;
    padding: 3px 8px;
    font-size: 11px;
    font-weight: 700;
    color: )CSS" + t.badge_bg + R"CSS(;
    margin: 8px;
}

.card-title {
    font-weight: 600;
    font-size: 13.5px;
    color: )CSS" + t.text_primary + R"CSS(;
    margin-top: 4px;
    line-height: 1.35;
}

.card-meta {
    font-size: 11px;
    color: )CSS" + t.text_secondary + R"CSS(;
}

.chip-tag {
    background-color: )CSS" + t.chip_bg + R"CSS(;
    color: )CSS" + t.text_secondary + R"CSS(;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    border-radius: 6px;
    padding: 2px 8px;
    font-size: 11px;
}

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
}

.episode-row {
    background-color: )CSS" + t.card_bg + R"CSS(;
    border-radius: 8px;
    padding: 10px 16px;
    margin: 0;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    transition: background-color 120ms ease;
}

.episode-row:hover {
    background-color: )CSS" + t.card_hover + R"CSS(;
}

.watched-badge {
    color: #57e389;
    font-weight: 800;
    font-size: 14px;
}

button.suggested-action {
    background-color: )CSS" + t.accent + R"CSS(;
    color: #ffffff;
    font-weight: 600;
    border-radius: 8px;
    padding: 7px 18px;
    border: none;
}

button.suggested-action:hover {
    background-color: )CSS" + t.accent_hover + R"CSS(;
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

entry {
    background-color: )CSS" + t.card_bg + R"CSS(;
    border: 1px solid )CSS" + t.border_color + R"CSS(;
    border-radius: 8px;
    color: )CSS" + t.text_primary + R"CSS(;
    padding: 6px 12px;
}

entry:focus {
    border-color: )CSS" + t.accent + R"CSS(;
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
)CSS";
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
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

} // namespace anime
