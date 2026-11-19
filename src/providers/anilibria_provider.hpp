#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class AnilibriaProvider : public BaseProvider {
public:
    std::string name() const override { return "anilibria"; }
    std::string display_name() const override { return "АніЛібрія"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    std::vector<Anime> browse(const std::string& query, int limit, const CatalogFilter& filter, int page) override;
    // The catalog API filters and sorts everything on the server
    FilterSupport server_filters() const override { return {true, true, true, true, true, true, true}; }
    MetaSupport list_meta() const override { return {true, false, true, true, true, true, true}; }
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string api_base = "https://anilibria.top/api/v1";
    std::string stream_host = "https://cache.libria.fun";

    Anime parse_anime_item(const class json::Value& item);
};

} // namespace anime
