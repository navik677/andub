#pragma once

#include "base_provider.hpp"
#include <string>
#include <vector>

namespace anime {

class AniStarProvider : public BaseProvider {
public:
    std::string name() const override { return "anistar"; }
    std::string display_name() const override { return "AniStar"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    // Genre listing pages exist only without a search query; with one, genres
    // are checked by Catalog (search() would drop non-matching cards and make
    // the page look like the last one).
    std::vector<Anime> browse(const std::string& query, int limit, const CatalogFilter& filter, int page) override {
        return search(query, limit, query.empty() && !filter.genres.empty() ? filter.genres.front() : std::string(), page);
    }
    MetaSupport list_meta() const override {
        MetaSupport m;
        m.year = m.score = m.kind = m.airing = m.genres = true;
        return m;
    }
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string fetch_page(const std::string& url, const std::string& referer = "https://v30.astar.bz/");
    static std::string s_cookie;
    static void update_cookie_from_html(const std::string& html);
};

} // namespace anime
