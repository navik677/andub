#pragma once

#include <string>
#include "../models/stream.hpp"
#include "../models/episode.hpp"

namespace anime {

class PlayerService {
public:
    static bool play(const Quality& quality, const std::string& anime_title, const Episode* episode = nullptr);
};

} // namespace anime
