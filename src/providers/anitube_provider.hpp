#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class AniTubeProvider : public BaseProvider {
public:
    std::string name() const override { return "anitube"; }
    std::string display_name() const override { return "AniTube (UA)"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    // Genre pages and single-year pages (/xfsearch/year/2004/) on the server,
    // the rest from the card: year, genres, "Серій: 9 з 12 (25 хв.)", stars.
    std::vector<Anime> browse(const std::string& query, int limit, const CatalogFilter& filter, int page) override;
    MetaSupport list_meta() const override {
        MetaSupport m;
        m.year = m.score = m.kind = m.airing = m.genres = true;
        return m;
    }
    Anime get_details(const Anime& anime) override;
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::vector<Anime> parse_story_list(const std::string& body, int limit) const;
    std::string base_url = "https://anitube.in.ua";
};

} // namespace anime
