#pragma once

#include <string>
#include <vector>

namespace anime {

struct Theme {
    std::string id;
    std::string name;
    std::string bg_color;
    std::string header_bg;
    std::string card_bg;
    std::string card_hover;
    std::string accent;
    std::string accent_hover;
    std::string text_primary;
    std::string text_secondary;
    std::string border_color;
    std::string badge_bg;
    std::string badge_text;
    std::string chip_bg;
};

class ThemeManager {
public:
    static const std::vector<Theme>& get_themes();
    static const Theme& get_theme(const std::string& theme_id);
    static std::string get_current_theme_id();
    static void set_current_theme_id(const std::string& theme_id);
    static void apply_theme(const std::string& theme_id);
    static std::string generate_css(const Theme& theme);
    static std::string get_config_file();
};

} // namespace anime
