#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../models/anime.hpp"
#include "../models/episode.hpp"
#include "../models/stream.hpp"
#include "../models/catalog_filter.hpp"

namespace anime {

class BaseProvider {
public:
    virtual ~BaseProvider() = default;

    virtual std::string name() const = 0;
    virtual std::string display_name() const = 0;

    virtual std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) = 0;

    // Catalog listing with filters. Providers that filter on their server
    // override this and report it in server_filters(); the default passes the
    // first genre to search() and leaves everything else to the client.
    virtual std::vector<Anime> browse(const std::string& query, int limit, const CatalogFilter& filter, int page) {
        return search(query, limit, filter.genres.empty() ? std::string() : filter.genres.front(), page);
    }
    virtual FilterSupport server_filters() const { return {}; }
    // Normalized fields that list results carry (for client-side filtering)
    virtual MetaSupport list_meta() const { return {}; }
    virtual std::vector<Episode> get_episodes(const Anime& anime) = 0;
    virtual Stream get_stream(const Anime& anime, const Episode& episode) = 0;
    virtual Anime get_details(const Anime& anime) { return anime; }
};

} // namespace anime
