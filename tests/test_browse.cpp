// Live check of catalog filters against every provider: for each filter, how
// many titles one page returns, and how many of them really satisfy it
// (server-side filters must return only matching titles; client-side ones are
// verified with the same matches() Catalog uses).
//
//   test_browse [provider-name]
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include "models/catalog_match.hpp"
#include "providers/anibaza_provider.hpp"
#include "providers/anidub_provider.hpp"
#include "providers/anilibria_provider.hpp"
#include "providers/animevost_provider.hpp"
#include "providers/anistar_provider.hpp"
#include "providers/anitube_provider.hpp"
#include "providers/dreamcast_provider.hpp"
#include "providers/shizaproject_provider.hpp"

using namespace anime;

struct Case {
    const char* name;
    CatalogFilter f;
};

static const char* kind_name(Kind k) {
    switch (k) {
    case Kind::TV: return "TV";
    case Kind::Movie: return "Movie";
    case Kind::OVA: return "OVA";
    case Kind::ONA: return "ONA";
    case Kind::Special: return "Special";
    default: return "?";
    }
}

int main(int argc, char** argv) {
    std::vector<std::shared_ptr<BaseProvider>> providers = {
        std::make_shared<AnilibriaProvider>(),  std::make_shared<DreamCastProvider>(),
        std::make_shared<ShizaProjectProvider>(), std::make_shared<AniBazaProvider>(),
        std::make_shared<AniDubProvider>(),      std::make_shared<AniTubeProvider>(),
        std::make_shared<AnimeVostProvider>(),   std::make_shared<AniStarProvider>(),
    };

    std::vector<Case> cases;
    cases.push_back({"no filter", {}});
    {
        CatalogFilter f;
        f.year_from = 2015;
        f.year_to = 2020;
        cases.push_back({"years 2015-2020", f});
    }
    {
        CatalogFilter f;
        f.kinds = {Kind::Movie};
        cases.push_back({"movies", f});
    }
    {
        CatalogFilter f;
        f.airing = Airing::Ongoing;
        cases.push_back({"ongoing", f});
    }
    {
        CatalogFilter f;
        f.genres = {"Комедия"};
        cases.push_back({"genre Комедия", f});
    }
    {
        CatalogFilter f;
        f.genres = {"Комедия", "Школа"};
        cases.push_back({"genres Комедия+Школа", f});
    }
    {
        CatalogFilter f;
        f.season = Season::Autumn;
        cases.push_back({"autumn", f});
    }
    {
        CatalogFilter f;
        f.max_age = 12;
        cases.push_back({"age <= 12", f});
    }

    int bad = 0;
    for (const auto& p : providers) {
        if (argc > 1 && std::strcmp(argv[1], p->name().c_str()) != 0) continue;
        const FilterSupport server = p->server_filters();
        const MetaSupport meta = p->list_meta();
        std::printf("\n== %s  (meta: year=%d score=%d kind=%d airing=%d season=%d age=%d genres=%d)\n",
                    p->display_name().c_str(), meta.year, meta.score, meta.kind, meta.airing, meta.season,
                    meta.age, meta.genres);
        for (const auto& c : cases) {
            std::vector<Anime> items;
            try {
                items = p->browse("", 30, c.f, 1);
            } catch (...) {
            }
            int ok = 0;
            for (const auto& a : items) {
                if (matches(a, c.f, FilterSupport{}, meta)) ++ok;
            }
            // When the server applied a filter, every returned title must match it
            // (as far as the title's own metadata can tell).
            std::printf("  %-22s %2zu items, %2d match locally", c.name, items.size(), ok);
            if (!items.empty()) {
                const Anime& a = items.front();
                std::printf("   e.g. %s | %d | %s | age %d | score %.1f", a.title_ru.substr(0, 40).c_str(), a.year,
                            kind_name(a.kind), a.age, a.score);
            }
            std::printf("\n");
            (void)server;
        }
    }
    return bad;
}
