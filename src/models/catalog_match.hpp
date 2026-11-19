#pragma once

// Client-side half of catalog filtering: checks a title against the parts of
// a CatalogFilter the provider did not apply on its server, and sorts lists
// when the server can't.

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include "anime.hpp"
#include "../utils/str_utils.hpp"

namespace anime {

// 2 = applied by the server, 1 = checked on loaded results, 0 = impossible
inline int filter_state(bool server, bool meta) { return server ? 2 : (meta ? 1 : 0); }

inline bool kind_selected(const CatalogFilter& f, Kind k) {
    return std::find(f.kinds.begin(), f.kinds.end(), k) != f.kinds.end();
}

// Unknown values never match an active filter: "only movies" should not show
// titles whose type the site doesn't state.
inline bool matches(const Anime& a, const CatalogFilter& f, const FilterSupport& server, const MetaSupport& meta) {
    if (f.has_years() && !server.years) {
        if (a.year <= 0) return false;
        if (f.year_from > 0 && a.year < f.year_from) return false;
        if (f.year_to > 0 && a.year > f.year_to) return false;
    }
    if (f.season != Season::Unknown && !server.season && a.season != f.season) return false;
    if (!f.kinds.empty() && !server.kinds && !kind_selected(f, a.kind)) return false;
    if (f.airing != Airing::Unknown && !server.airing && a.airing != f.airing) return false;
    if (f.max_age > 0 && !server.age && (a.age < 0 || a.age > f.max_age)) return false;
    if (!f.genres.empty() && !server.genres && meta.genres) {
        for (const auto& want : f.genres) {
            bool found = false;
            for (const auto& g : a.genres) {
                if (genre_matches(g, want)) { found = true; break; }
            }
            if (!found) return false;
        }
    }
    return true;
}

namespace detail {
// Popularity counts ("12500") when there is no score
inline double popularity(const Anime& a) {
    double v = std::strtod(a.rating.c_str(), nullptr);
    return v > 0 ? v : 0;
}
inline std::string title_key(const Anime& a) {
    return utils::utf8_tolower(a.title_ru.empty() ? a.title_en : a.title_ru);
}
} // namespace detail

// Stable, so equal keys keep the site's own order.
inline void sort_items(std::vector<Anime>& items, SortOrder order) {
    switch (order) {
    case SortOrder::Rating:
        std::stable_sort(items.begin(), items.end(), [](const Anime& x, const Anime& y) {
            if ((x.score >= 0) != (y.score >= 0)) return x.score >= 0;
            if (x.score != y.score) return x.score > y.score;
            return detail::popularity(x) > detail::popularity(y);
        });
        break;
    case SortOrder::YearDesc:
        std::stable_sort(items.begin(), items.end(), [](const Anime& x, const Anime& y) { return x.year > y.year; });
        break;
    case SortOrder::YearAsc:
        // Unknown years (0) go last, not first
        std::stable_sort(items.begin(), items.end(), [](const Anime& x, const Anime& y) {
            if ((x.year > 0) != (y.year > 0)) return x.year > 0;
            return x.year < y.year;
        });
        break;
    case SortOrder::Title:
        std::stable_sort(items.begin(), items.end(), [](const Anime& x, const Anime& y) {
            return detail::title_key(x) < detail::title_key(y);
        });
        break;
    case SortOrder::Default:
    case SortOrder::Updated:
        break; // the site's order
    }
}

} // namespace anime
