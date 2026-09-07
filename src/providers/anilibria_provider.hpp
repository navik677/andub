#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class AnilibriaProvider : public BaseProvider {
public:
    std::string name() const override { return "anilibria"; }
    std::string display_name() const override { return "АніЛібрія"; }

    std::vector<Anime> search(const std::string& query, int limit = 30) override;
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string api_base = "https://anilibria.top/api/v1";
    std::string stream_host = "https://cache.libria.fun";

    Anime parse_anime_item(const class json::Value& item);
};

} // namespace anime
