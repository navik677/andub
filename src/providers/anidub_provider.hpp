#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class AniDubProvider : public BaseProvider {
public:
    std::string name() const override { return "anidub"; }
    std::string display_name() const override { return "AniDub"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    // Cards carry score, year and category (type + ongoing); genres are only
    // searchable one at a time on the site.
    MetaSupport list_meta() const override {
        MetaSupport m;
        m.year = m.score = m.kind = m.airing = true;
        return m;
    }
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string base_url = "https://v14.anidub.digital";
};

} // namespace anime
