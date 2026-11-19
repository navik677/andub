#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class AnimeVostProvider : public BaseProvider {
public:
    std::string name() const override { return "animevost"; }
    std::string display_name() const override { return "AnimeVost"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    // The API has no filters; list items carry year, type, genres, votes and
    // "[1-3 из 12]" episode counts, so everything is filtered by Catalog.
    std::vector<Anime> browse(const std::string& query, int limit, const CatalogFilter&, int page) override {
        return search(query, limit, "", page);
    }
    MetaSupport list_meta() const override {
        MetaSupport m;
        m.year = m.score = m.kind = m.airing = m.genres = true;
        return m;
    }
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string api_base = "https://api.animevost.org/v1";

    Anime parse_anime_item(const class json::Value& item);
};

} // namespace anime
