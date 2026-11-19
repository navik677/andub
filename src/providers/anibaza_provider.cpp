#include "anibaza_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include "../utils/kodik_resolver.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <sstream>

namespace anime {

static std::string anibaza_url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << (int)(unsigned char)c;
        }
    }
    return escaped.str();
}

namespace {

const std::map<std::string, std::string>& anibaza_headers() {
    static const std::map<std::string, std::string> h = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", "https://anibaza.com/"},
    };
    return h;
}

// Genre ids from the catalog's genre bar: <a class="ct-genre" href="?genre=6" ...>Фэнтези<small>58</small>
std::vector<std::pair<std::string, int>> anibaza_genres(const std::string& base_url) {
    static std::mutex m;
    static std::vector<std::pair<std::string, int>> cache;
    std::lock_guard lock(m);
    if (!cache.empty()) return cache;
    auto resp = anime::http::Client::get(base_url + "/catalog/", anibaza_headers());
    if (!resp.success()) return {};
    static const std::regex re(R"raw(href="\?genre=(\d+)"[^>]*>([^<]+)<)raw");
    for (auto it = std::sregex_iterator(resp.body.begin(), resp.body.end(), re); it != std::sregex_iterator(); ++it)
        cache.emplace_back(anime::utils::trim((*it)[2].str()), std::atoi((*it)[1].str().c_str()));
    return cache;
}

} // namespace

// All cards of one filtered catalog listing (the catalog is small: a few
// pages of 48). Cached for a few minutes so scrolling doesn't refetch it.
std::vector<Anime> AniBazaProvider::fetch_catalog(const std::string& params) {
    using clock = std::chrono::steady_clock;
    static std::mutex m;
    static std::unordered_map<std::string, std::pair<clock::time_point, std::vector<Anime>>> cache;
    {
        std::lock_guard lock(m);
        auto it = cache.find(params);
        if (it != cache.end() && clock::now() - it->second.first < std::chrono::minutes(5)) return it->second.second;
    }

    static const std::regex link_re(R"raw(href="/release/([a-zA-Z0-9_-]+)/")raw");
    // Attributes may sit on separate lines
    static const std::regex img_re(R"raw(<img\s+class="release__card-poster"\s+src="([^"]+)"\s+alt="([^"]*)")raw");
    static const std::regex rating_re(R"raw(air-drawer__rating">[^0-9]*([\d.,]+)<)raw");
    static const std::regex year_re(R"raw(air-drawer__meta">[\s\S]{0,200}?<span>((?:19|20)\d{2})</span>)raw");
    static const std::regex pill_re(R"raw(pill_release_([a-z]+))raw");

    std::vector<Anime> all;
    std::unordered_set<std::string> seen;
    for (int page = 1; page <= 10; ++page) {
        std::string url = base_url + "/catalog/?" + params + (params.empty() ? "" : "&") + "page=" + std::to_string(page);
        auto resp = http::Client::get(url, anibaza_headers());
        if (!resp.success()) break;
        int on_page = 0, fresh = 0;
        for (const std::string& card : utils::split_blocks(resp.body, "<div class=\"release__card\">")) {
            std::smatch mm;
            if (!std::regex_search(card, mm, link_re)) continue;
            ++on_page;
            Anime a;
            a.id = mm[1].str();
            if (!seen.insert(a.id).second) continue; // past the last page the site repeats it
            ++fresh;
            a.meta["slug"] = a.id;
            a.provider = name();
            if (std::regex_search(card, mm, img_re)) {
                const std::string poster = mm[1].str();
                a.poster_url = poster.rfind("http", 0) != 0 ? base_url + poster : poster;
                a.title_ru = utils::trim(mm[2].str());
            }
            if (std::regex_search(card, mm, rating_re)) {
                a.score = parse_score(mm[1].str());
                if (a.score >= 0) {
                    a.rating = mm[1].str();
                    std::replace(a.rating.begin(), a.rating.end(), ',', '.');
                }
            }
            if (std::regex_search(card, mm, year_re)) a.year = std::atoi(mm[1].str().c_str());
            if (std::regex_search(card, mm, pill_re)) {
                const std::string st = mm[1].str();
                a.airing = st == "ongoing" ? Airing::Ongoing : st == "finished" ? Airing::Finished
                         : st == "announced" || st == "anons" ? Airing::Announced : Airing::Unknown;
            }
            all.push_back(std::move(a));
        }
        if (on_page < 48 || fresh == 0) break;
    }

    std::lock_guard lock(m);
    cache[params] = {clock::now(), all};
    return all;
}

std::vector<Anime> AniBazaProvider::browse(const std::string& query, int limit, const CatalogFilter& filter, int page) {
    std::string params;
    auto add = [&](const std::string& k, const std::string& v) {
        params += (params.empty() ? "" : "&") + k + "=" + anibaza_url_encode(v);
    };
    if (!query.empty()) add("q", query);
    if (!filter.genres.empty()) {
        const auto genres = anibaza_genres(base_url);
        for (const auto& want : filter.genres) {
            auto it = std::find_if(genres.begin(), genres.end(),
                                   [&](const auto& g) { return genre_matches(g.first, want); });
            if (it == genres.end()) return {}; // a genre this site doesn't have: nothing can match
            add("genre", std::to_string(it->second));
        }
    }
    if (filter.airing == Airing::Ongoing) add("status", "В эфире");
    else if (filter.airing == Airing::Finished) add("status", "Закончен");
    else if (filter.airing == Airing::Announced) add("status", "Анонсирован");
    if (filter.year_from > 0 && filter.year_from == filter.year_to) add("year", std::to_string(filter.year_from));
    switch (filter.sort) {
    case SortOrder::Rating: add("sort", "rating"); break;
    case SortOrder::YearDesc: add("sort", "year"); break;
    default: break; // the rest is sorted by Catalog
    }

    // The site takes a single type; several types are separate listings merged
    std::vector<Anime> all;
    if (filter.kinds.empty()) {
        all = fetch_catalog(params);
    } else {
        std::unordered_set<std::string> seen;
        for (Kind k : filter.kinds) {
            const char* type = nullptr;
            switch (k) {
            case Kind::TV: type = "Аниме"; break;
            case Kind::Movie: type = "Фильм"; break;
            case Kind::OVA: type = "ОVA"; break; // sic: Cyrillic "О"
            case Kind::ONA: type = "ONA"; break;
            case Kind::Special: type = "Другое"; break;
            default: break;
            }
            if (!type) continue;
            std::string p = params + (params.empty() ? "" : "&") + "type=" + anibaza_url_encode(type);
            for (auto& a : fetch_catalog(p)) {
                a.kind = k;
                if (seen.insert(a.id).second) all.push_back(std::move(a));
            }
        }
    }

    std::vector<Anime> out;
    const size_t offset = static_cast<size_t>(std::max(0, page - 1) * limit);
    for (size_t i = offset; i < all.size() && static_cast<int>(out.size()) < limit; ++i) out.push_back(all[i]);
    return out;
}

std::vector<Anime> AniBazaProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    CatalogFilter f;
    if (!genre.empty()) f.genres.push_back(genre);
    return browse(query, limit, f, page);
}


std::vector<Episode> AniBazaProvider::get_episodes(const Anime& anime) {
    std::string slug = anime.meta.count("slug") ? anime.meta.at("slug") : anime.id;
    std::string page_url = base_url + "/release/" + slug + "/";

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(page_url, headers);
    if (!resp.success()) {
        std::cerr << "[AniBaza] Failed to load release page: " << resp.error << "\n";
        return {};
    }

    // Look for Kodik link in HTML: e.g. //kodikplayer.com/serial/... or https://kodikplayer.com/...
    std::regex kodik_re(R"((?:https?:)?//(kodik(?:player)?\.(?:com|info|biz)/serial/[a-zA-Z0-9_/]+))");
    std::smatch match;
    std::string kodik_url;

    if (std::regex_search(resp.body, match, kodik_re)) {
        kodik_url = "https://" + match[1].str();
    }

    std::vector<Episode> episodes;
    if (!kodik_url.empty()) {
        // Fetch Kodik page to discover episode count
        std::map<std::string, std::string> kodik_headers = {
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
            {"Referer", page_url}
        };

        auto kodik_resp = http::Client::get(kodik_url, kodik_headers);
        int ep_count = 0;
        if (kodik_resp.success()) {
            std::regex count_re(R"raw(data-episode-count="(\d+)")raw");
            std::smatch cmatch;
            if (std::regex_search(kodik_resp.body, cmatch, count_re)) {
                try {
                    ep_count = std::stoi(cmatch[1].str());
                } catch (...) {}
            }
        }

        if (ep_count <= 0) ep_count = 1; // Fallback

        for (int e = 1; e <= ep_count; ++e) {
            Episode ep;
            ep.number = std::to_string(e);
            ep.title = "Серія " + ep.number;
            ep.meta["kodik_url"] = kodik_url;
            ep.meta["episode_num"] = ep.number;
            episodes.push_back(std::move(ep));
        }
    } else {
        // Single episode fallback
        Episode ep;
        ep.number = "1";
        ep.title = "Фільм / Серія 1";
        ep.meta["page_url"] = page_url;
        episodes.push_back(std::move(ep));
    }

    return episodes;
}

Stream AniBazaProvider::get_stream(const Anime& /*anime*/, const Episode& episode) {
    if (episode.meta.count("kodik_url")) {
        std::string kodik_base = episode.meta.at("kodik_url");
        int ep_num = 1;
        try {
            std::string ep_str = episode.meta.count("episode_num") ? episode.meta.at("episode_num") : episode.number;
            ep_num = std::stoi(ep_str);
        } catch (...) {}

        auto resolved = KodikResolver::resolve(kodik_base, ep_num, base_url + "/");
        if (!resolved.qualities.empty()) {
            return resolved;
        }

        // Fallback
        Stream stream;
        Quality q;
        q.label = "720p";
        q.url = kodik_base + "?season=1&episode=" + std::to_string(ep_num);
        q.headers["Referer"] = base_url + "/";
        q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
        stream.qualities.push_back(std::move(q));
        return stream;
    }

    return {};
}

} // namespace anime
