#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class ShizaProjectProvider : public BaseProvider {
public:
    std::string name() const override { return "shizaproject"; }
    std::string display_name() const override { return "Shiza Project"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    std::vector<Anime> browse(const std::string& query, int limit, const CatalogFilter& filter, int page) override;
    // GraphQL filters: genres (all of them), type, status, season, aired year, rating
    FilterSupport server_filters() const override { return {true, true, true, true, true, true, true}; }
    MetaSupport list_meta() const override { return {true, true, true, true, true, true, true}; }
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string graphql_url = "https://shizaproject.com/graphql";
};

} // namespace anime
