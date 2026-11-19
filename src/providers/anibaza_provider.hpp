#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class AniBazaProvider : public BaseProvider {
public:
    std::string name() const override { return "anibaza"; }
    std::string display_name() const override { return "AniBaza"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    // /catalog/ filters on the server: one type, status, one year, genres (all
    // of them), sorting. Cards carry score, year and status.
    std::vector<Anime> browse(const std::string& query, int limit, const CatalogFilter& filter, int page) override;
    FilterSupport server_filters() const override {
        FilterSupport s;
        s.kinds = s.airing = s.genres = true;
        return s;
    }
    MetaSupport list_meta() const override {
        MetaSupport m;
        m.year = m.score = m.airing = true;
        return m;
    }
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string base_url = "https://anibaza.com";
    std::vector<Anime> fetch_catalog(const std::string& params);
};

} // namespace anime
