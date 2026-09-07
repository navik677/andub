#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class AniTubeProvider : public BaseProvider {
public:
    std::string name() const override { return "anitube"; }
    std::string display_name() const override { return "AniTube (UA)"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string base_url = "https://anitube.in.ua";
};

} // namespace anime
