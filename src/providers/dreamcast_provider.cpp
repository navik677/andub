#include "dreamcast_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include "../utils/kodik_resolver.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

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
    std::string url;
    int fetch_limit = (page > 1) ? (limit * page) : limit;
    if (fetch_limit > 100) fetch_limit = 100;

    if (query.empty()) {
        url = api_base + "/list?token=" + api_token +
              "&translation_id=" + std::to_string(translation_id) +
              "&types=anime-serial,anime&with_material_data=true&limit=" + std::to_string(fetch_limit);
    } else {
        url = api_base + "/search?token=" + api_token +
              "&translation_id=" + std::to_string(translation_id) +
              "&title=" + dreamcast_url_encode(query) +
              "&types=anime-serial,anime&with_material_data=true&limit=" + std::to_string(fetch_limit);
    }

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
    };

    auto resp = http::Client::get(url, headers);
    if (!resp.success()) {
        std::cerr << "[DreamCast] API request failed: " << resp.error << " (status: " << resp.status_code << ")\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto results_val = root["results"];
    if (!results_val.is_array()) return {};

    std::vector<Anime> results;
    std::string lower_genre = utils::utf8_tolower(genre);
    size_t skip_count = (page > 1) ? static_cast<size_t>((page - 1) * limit) : 0;
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

            auto rate_val = md["shikimori_rating"];
            if (rate_val.is_number()) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(1) << rate_val.get_double();
                a.rating = ss.str();
            } else if (rate_val.is_string() && !rate_val.get_str().empty()) {
                a.rating = rate_val.get_str();
            }

            auto g_arr = md["anime_genres"];
            if (!g_arr.is_array() || g_arr.size() == 0) g_arr = md["all_genres"];
            bool matches_genre = genre.empty();

            if (g_arr.is_array()) {
                for (size_t g = 0; g < g_arr.size(); ++g) {
                    std::string gn = g_arr[g].get_str();
                    if (!gn.empty()) {
                        a.genres.push_back(gn);
                        if (!matches_genre && utils::utf8_tolower(gn).find(lower_genre) != std::string::npos) {
                            matches_genre = true;
                        }
                    }
                }
            }

            if (!matches_genre) continue;
        } else if (!genre.empty()) {
            continue;
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
