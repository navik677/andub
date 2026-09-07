#pragma once

#include <string>
#include <vector>
#include <map>
#include "episode.hpp"

namespace anime {

struct Anime {
    std::string id;
    std::string title_ru;
    std::string title_en;
    std::string description;
    int year = 0;
    std::vector<std::string> genres;
    std::string status;
    std::string provider; // "anilibria", "animevost", "rezka", etc.
    std::string poster_url;
    std::string rating;
    std::string age_rating;
    std::vector<std::string> comments;
    std::map<std::string, std::string> meta;

    std::string display_title() const {
        std::string res = title_ru;
        if (!title_en.empty() && title_en != title_ru) {
            res += " (" + title_en + ")";
        }
        if (year > 0) {
            res += " [" + std::to_string(year) + "]";
        }
        return res;
    }

    std::string genres_str() const {
        std::string res;
        for (size_t i = 0; i < genres.size(); ++i) {
            res += genres[i];
            if (i + 1 < genres.size()) res += ", ";
        }
        return res;
    }
};

} // namespace anime
