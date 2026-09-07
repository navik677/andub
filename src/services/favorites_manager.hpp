#pragma once

#include <vector>
#include <string>
#include "../models/anime.hpp"

namespace anime {

class FavoritesManager {
public:
    static std::vector<Anime> get_favorites();
    static bool is_favorite(const std::string& provider, const std::string& anime_id);
    static bool toggle_favorite(const Anime& anime); // Returns true if added, false if removed

private:
    static std::string get_file_path();
};

} // namespace anime
