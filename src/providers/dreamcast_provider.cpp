#include "dreamcast_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include "../utils/kodik_resolver.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

namespace anime {

static std::string dreamcast_url_encode(const std::string& value) {
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

std::vector<Anime> DreamCastProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    CatalogFilter f;
    if (!genre.empty()) f.genres.push_back(genre);
    return browse(query, limit, f, page);
}

std::vector<Anime> DreamCastProvider::browse(const std::string& query, int limit, const CatalogFilter& filter, int page) {
    std::string url;
    size_t target_skip = (page > 1) ? static_cast<size_t>((page - 1) * limit) : 0;
    size_t batches_to_skip = target_skip / 100;
    size_t in_batch_skip = target_skip % 100;

    if (query.empty()) {
        url = api_base + "/list?token=" + api_token +
              "&translation_id=" + std::to_string(translation_id) +
              "&types=anime-serial,anime&with_material_data=true&limit=100";
    } else {
        url = api_base + "/search?token=" + api_token +
              "&translation_id=" + std::to_string(translation_id) +
              "&title=" + dreamcast_url_encode(query) +
              "&types=anime-serial,anime&with_material_data=true&limit=100";
    }

    if (filter.has_years()) {
        // Kodik takes a comma-separated list of years, not a range
        const int from = filter.year_from > 0 ? filter.year_from : 1960;
        const int to = filter.year_to > 0 ? filter.year_to : 2100;
        std::string years;
        for (int y = from; y <= std::min(to, from + 80); ++y) years += (years.empty() ? "" : ",") + std::to_string(y);
        url += "&year=" + years;
    }
    std::string kinds;
    for (Kind k : filter.kinds) {
        const char* v = nullptr;
        switch (k) {
        case Kind::TV: v = "tv,tv_13,tv_24,tv_48"; break;
        case Kind::Movie: v = "movie"; break;
        case Kind::OVA: v = "ova"; break;
        case Kind::ONA: v = "ona"; break;
        case Kind::Special: v = "special,tv_special"; break;
        default: break;
        }
        if (v) kinds += (kinds.empty() ? "" : ",") + std::string(v);
    }
    if (!kinds.empty()) url += "&anime_kind=" + kinds;
    if (filter.airing == Airing::Ongoing) url += "&anime_status=ongoing";
    else if (filter.airing == Airing::Finished) url += "&anime_status=released";
    else if (filter.airing == Airing::Announced) url += "&anime_status=anons";
    if (filter.max_age > 0) {
        std::string mpaa;
        const std::pair<int, const char*> scale[] = {{0, "G"}, {6, "PG"}, {12, "PG-13"}, {16, "R"}, {18, "R%2B"}, {18, "Rx"}};
        for (const auto& [age, v] : scale) {
            if (age <= filter.max_age) mpaa += (mpaa.empty() ? "" : ",") + std::string(v);
        }
        url += "&rating_mpaa=" + mpaa;
    }
    switch (filter.sort) {
    case SortOrder::Updated: url += "&sort=updated_at&order=desc"; break;
    case SortOrder::Rating: url += "&sort=shikimori_rating&order=desc"; break;
    case SortOrder::YearDesc: url += "&sort=year&order=desc"; break;
    case SortOrder::YearAsc: url += "&sort=year&order=asc"; break;
    default: break;
    }

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
    };

    // Advance cursor if needed
    std::string current_url = url;
    for (size_t b = 0; b < batches_to_skip; ++b) {
        auto resp = http::Client::get(current_url, headers);
        if (!resp.success()) break;
        auto root = json::Value::parse(resp.body);
        std::string next_p = root["next_page"].get_str();
        if (next_p.empty()) break;
        current_url = next_p;
    }

    auto resp = http::Client::get(current_url, headers);
    if (!resp.success()) {
        std::cerr << "[DreamCast] API request failed: " << resp.error << " (status: " << resp.status_code << ")\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto results_val = root["results"];
    if (!results_val.is_array()) return {};

    std::vector<Anime> results;
    size_t skip_count = in_batch_skip;
    size_t matched_count = 0;

    for (size_t i = 0; i < results_val.size(); ++i) {
        auto item = results_val[i];
        Anime a;
        a.id = item["id"].get_str();
        a.title_ru = item["title"].get_str();
        a.title_en = item["title_orig"].get_str();
        a.year = item["year"].get_int();
        a.provider = name();

        std::string link = item["link"].get_str();
        if (link.rfind("//", 0) == 0) link = "https:" + link;
        a.meta["link"] = link;

        int eps_cnt = item["episodes_count"].get_int();
        a.meta["episodes_count"] = std::to_string(eps_cnt > 0 ? eps_cnt : 1);

        auto md = item["material_data"];
        if (md.is_object()) {
            std::string desc = md["description"].get_str();
            if (desc.empty()) desc = md["anime_description"].get_str();
            a.description = desc;

            std::string poster = md["poster_url"].get_str();
            if (poster.empty()) poster = md["anime_poster_url"].get_str();
            if (!poster.empty()) {
                if (poster.rfind("//", 0) == 0) poster = "https:" + poster;
                a.poster_url = poster;
            }

            a.kind = parse_kind(md["anime_kind"].get_str());
            a.airing = parse_airing(md["anime_status"].get_str());
            a.age = parse_age(md["rating_mpaa"].get_str());
            const std::string aired = md["aired_at"].get_str(); // "2026-10-04"
            if (aired.size() >= 7) a.season = season_from_month(std::atoi(aired.substr(5, 2).c_str()));

            auto rate_val = md["shikimori_rating"];
            if (rate_val.is_number() && rate_val.get_double() > 0) a.score = rate_val.get_double();
            if (rate_val.is_number()) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(1) << rate_val.get_double();
                a.rating = ss.str();
            } else if (rate_val.is_string() && !rate_val.get_str().empty()) {
                a.rating = rate_val.get_str();
            }

            auto g_arr = md["anime_genres"];
            if (!g_arr.is_array() || g_arr.size() == 0) g_arr = md["all_genres"];
            for (size_t g = 0; g < g_arr.size(); ++g) {
                std::string gn = g_arr[g].get_str();
                if (!gn.empty()) a.genres.push_back(gn);
            }
        }

        matched_count++;
        if (matched_count <= skip_count) continue;

        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= limit) break;
    }

    return results;
}

std::vector<Episode> DreamCastProvider::get_episodes(const Anime& anime) {
    std::string kodik_url = anime.meta.count("link") ? anime.meta.at("link") : "";
    if (kodik_url.empty()) return {};

    int count = 1;
    if (anime.meta.count("episodes_count")) {
        try { count = std::stoi(anime.meta.at("episodes_count")); } catch (...) {}
    }

    // Try finding exact episodes via KodikResolver
    auto parsed_eps = KodikResolver::get_episodes(kodik_url);
    if (!parsed_eps.empty()) {
        std::vector<Episode> episodes;
        for (int ep_num : parsed_eps) {
            Episode ep;
            ep.number = std::to_string(ep_num);
            ep.title = "Серія " + ep.number;
            ep.meta["kodik_url"] = kodik_url;
            ep.meta["episode_num"] = ep.number;
            episodes.push_back(std::move(ep));
        }
        return episodes;
    }

    if (count <= 0) count = 1;
    std::vector<Episode> episodes;
    for (int i = 1; i <= count; ++i) {
        Episode ep;
        ep.number = std::to_string(i);
        ep.title = "Серія " + ep.number;
        ep.meta["kodik_url"] = kodik_url;
        ep.meta["episode_num"] = ep.number;
        episodes.push_back(std::move(ep));
    }
    return episodes;
}

Stream DreamCastProvider::get_stream(const Anime& anime, const Episode& episode) {
    std::string kodik_url;
    if (episode.meta.count("kodik_url")) {
        kodik_url = episode.meta.at("kodik_url");
    } else if (anime.meta.count("link")) {
        kodik_url = anime.meta.at("link");
    }

    if (kodik_url.empty()) return {};

    int ep_num = 1;
    try {
        std::string ep_str = episode.meta.count("episode_num") ? episode.meta.at("episode_num") : episode.number;
        ep_num = std::stoi(ep_str);
    } catch (...) {}

    auto resolved = KodikResolver::resolve(kodik_url, ep_num);
    if (!resolved.qualities.empty()) {
        return resolved;
    }

    // Fallback stream
    Stream stream;
    Quality q;
    q.label = "720p";
    q.url = kodik_url;
    q.headers["Referer"] = "https://shizaproject.com/";
    q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    stream.qualities.push_back(std::move(q));
    return stream;
}

} // namespace anime
