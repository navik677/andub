#pragma once

#include <string>
#include <map>
#include <vector>

namespace anime {

struct Episode {
    std::string number;
    std::string title;
    std::map<std::string, std::string> stream_urls; // e.g. "1080p" -> url, "720p" -> url
    std::vector<int> opening_skip;                  // [start_sec, end_sec]
    std::vector<int> ending_skip;                   // [start_sec, end_sec]
    std::map<std::string, std::string> meta;

    std::string display_title() const {
        if (!title.empty()) {
            return "Серія " + number + ": " + title;
        }
        return "Серія " + number;
    }
};

} // namespace anime
