#pragma once

#include <string>
#include <vector>
#include <map>

namespace anime {

struct Quality {
    std::string label;  // "1080p", "720p", "480p"
    std::string url;    // stream URL (.m3u8 or .mp4)
    std::map<std::string, std::string> headers;
};

struct Stream {
    std::vector<Quality> qualities;

    const Quality* best() const {
        for (const auto& target : {"1080p", "720p", "480p", "360p"}) {
            for (const auto& q : qualities) {
                if (q.label == target) return &q;
            }
        }
        return qualities.empty() ? nullptr : &qualities[0];
    }
};

} // namespace anime
