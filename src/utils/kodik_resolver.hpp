#pragma once

#include "../models/stream.hpp"
#include <string>
#include <vector>

namespace anime {

class KodikResolver {
public:
    // Resolve Kodik player URL (uv, serial, video) into direct HLS .m3u8 stream qualities
    static Stream resolve(const std::string& kodik_url, int episode_num = 1, const std::string& referer = "https://kodikplayer.com/");

    // Discover episode numbers available in a Kodik serial page
    static std::vector<int> get_episodes(const std::string& kodik_url, const std::string& referer = "https://kodikplayer.com/");
};

} // namespace anime
