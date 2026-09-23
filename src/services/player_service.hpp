#pragma once

#include <string>
#include "../models/stream.hpp"
#include "../models/episode.hpp"

namespace anime {

enum class PlayerMode {
    Embedded,
    ExternalMpv
};

class PlayerService {
public:
    static PlayerMode get_player_mode();
    static void set_player_mode(PlayerMode mode);

    // Generic string values in settings.json
    static std::string get_setting(const std::string& key, const std::string& fallback = "");
    static void set_setting(const std::string& key, const std::string& value);

    static bool play(const Quality& quality, const std::string& anime_title, const Episode* episode = nullptr);
    static bool play_external(const Quality& quality, const std::string& anime_title, const Episode* episode = nullptr, double start_pos = 0.0);

private:
    static std::string get_config_path();
};

} // namespace anime
