#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../models/anime.hpp"
#include "../models/episode.hpp"
#include "../models/stream.hpp"

namespace anime {

class BaseProvider {
public:
    virtual ~BaseProvider() = default;

    virtual std::string name() const = 0;
    virtual std::string display_name() const = 0;

    virtual std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) = 0;
    virtual std::vector<Episode> get_episodes(const Anime& anime) = 0;
    virtual Stream get_stream(const Anime& anime, const Episode& episode) = 0;
    virtual Anime get_details(const Anime& anime) { return anime; }
};

} // namespace anime
