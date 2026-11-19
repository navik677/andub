#include "anilibria_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/json.hpp"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include "../utils/str_utils.hpp"

namespace anime {

static std::string url_encode(const std::string& value) {
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

static const std::unordered_map<std::string, int>& get_anilibria_genre_map() {
    static const std::unordered_map<std::string, int> m = {
        {"боевые искусства", 15},
        {"вампиры", 24},
        {"гарем", 32},
        {"демоны", 16},
        {"детектив", 25},
        {"дзёсей", 33},
        {"драма", 8},
        {"игры", 17},
        {"исекай", 34},
        {"исторический", 26},
        {"киберпанк", 30},
        {"комедия", 1},
        {"магия", 18},
        {"меха", 2},
        {"мистика", 9},
        {"музыка", 19},
        {"пародия", 36},
        {"повседневность", 10},
        {"приключения", 27},
        {"психологическое", 3},
        {"романтика", 11},
        {"сверхъестественное", 28},
        {"сейнен", 5},
        {"спорт", 12},
        {"супер сила", 21},
        {"сёдзе", 20},
        {"сёдзе-ай", 31},
        {"сёнен", 4},
        {"триллер", 6},
        {"ужасы", 13},
        {"фантастика", 22},
        {"фэнтези", 29},
        {"школа", 7},
        {"экшен", 14},
        {"этти", 23}
    };
    return m;
}

std::vector<Anime> AnilibriaProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    CatalogFilter f;
    if (!genre.empty()) f.genres.push_back(genre);
    return browse(query, limit, f, page);
}

std::vector<Anime> AnilibriaProvider::browse(const std::string& query, int limit, const CatalogFilter& filter, int page) {
    int effective_limit = std::clamp(limit, 1, 30);
    int effective_page = std::max(1, page);

    std::string url = api_base + "/anime/catalog/releases?page=" + std::to_string(effective_page) +
                      "&limit=" + std::to_string(effective_limit);

    if (!query.empty()) {
        url += "&f[search]=" + url_encode(query);
    }

    // Several genres: comma-separated ids, the API keeps titles having all of them
    std::string genre_ids;
    const auto& gmap = get_anilibria_genre_map();
    for (const auto& g : filter.genres) {
        auto it = gmap.find(utils::utf8_tolower(g));
        if (it == gmap.end()) return {}; // a genre AniLibria doesn't have: nothing can match
        if (!genre_ids.empty()) genre_ids += ",";
        genre_ids += std::to_string(it->second);
    }
    if (!genre_ids.empty()) url += "&f[genres]=" + genre_ids;

    switch (filter.sort) {
    case SortOrder::Updated: url += "&f[sorting]=FRESH_AT_DESC"; break;
    case SortOrder::Rating: url += "&f[sorting]=RATING_DESC"; break;
    case SortOrder::YearDesc: url += "&f[sorting]=YEAR_DESC"; break;
    case SortOrder::YearAsc: url += "&f[sorting]=YEAR_ASC"; break;
    default: break; // Title: not offered by the API, sorted on the client
    }

    if (filter.year_from > 0) url += "&f[years][from_year]=" + std::to_string(filter.year_from);
    if (filter.year_to > 0) url += "&f[years][to_year]=" + std::to_string(filter.year_to);

    static const char* seasons[] = {"", "winter", "spring", "summer", "autumn"};
    if (filter.season != Season::Unknown) url += std::string("&f[seasons]=") + seasons[static_cast<int>(filter.season)];

    std::string types;
    for (Kind k : filter.kinds) {
        const char* v = nullptr;
        switch (k) {
        case Kind::TV: v = "TV"; break;
        case Kind::Movie: v = "MOVIE"; break;
        case Kind::OVA: v = "OVA,OAD"; break;
        case Kind::ONA: v = "ONA,WEB"; break;
        case Kind::Special: v = "SPECIAL"; break;
        default: break;
        }
        if (!v) continue;
        if (!types.empty()) types += ",";
        types += v;
    }
    if (!types.empty()) url += "&f[types]=" + types;

    if (filter.airing == Airing::Ongoing) url += "&f[publish_statuses]=IS_ONGOING";
    else if (filter.airing == Airing::Finished) url += "&f[publish_statuses]=IS_NOT_ONGOING";

    if (filter.max_age > 0) {
        std::string ages;
        for (int age : {0, 6, 12, 16, 18}) {
            if (age > filter.max_age) break;
            if (!ages.empty()) ages += ",";
            ages += "R" + std::to_string(age) + "_PLUS";
        }
        url += "&f[age_ratings]=" + ages;
    }

    auto resp = http::Client::get(url);
    if (!resp.success()) {
        std::cerr << "[Anilibria] Search error: " << resp.error << " (HTTP " << resp.status_code << ")\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    const json::Value* items = nullptr;
    if (root.is_array()) {
        items = &root;
    } else if (root["data"].is_array()) {
        items = &root["data"];
    }
    if (!items) return {};

    std::vector<Anime> results;
    for (size_t i = 0; i < items->size(); ++i) {
        auto a = parse_anime_item((*items)[i]);
        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= effective_limit) break;
    }
    return results;
}

std::vector<Episode> AnilibriaProvider::get_episodes(const Anime& anime) {
    std::string url = api_base + "/anime/releases/" + anime.id;
    auto resp = http::Client::get(url);
    if (!resp.success()) {
        std::cerr << "[Anilibria] Episode fetch error: " << resp.error << "\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto eps_val = root["episodes"];
    if (!eps_val.is_array()) return {};

    std::vector<Episode> episodes;
    for (size_t i = 0; i < eps_val.size(); ++i) {
        const auto& item = eps_val[i];
        Episode ep;
        ep.number = item["ordinal"].get_str();
        if (ep.number.empty()) {
            double ord = item["ordinal"].get_double();
            if (ord > 0) {
                if (ord == static_cast<int>(ord)) ep.number = std::to_string(static_cast<int>(ord));
                else ep.number = std::to_string(ord);
            } else {
                ep.number = std::to_string(i + 1);
            }
        }
        ep.title = item["name"].get_str();

        auto opening = item["opening"];
        if (opening.is_object() && opening.contains("start") && opening.contains("stop")) {
            ep.opening_skip = {opening["start"].get_int(), opening["stop"].get_int()};
        }
        auto ending = item["ending"];
        if (ending.is_object() && ending.contains("start") && ending.contains("stop")) {
            ep.ending_skip = {ending["start"].get_int(), ending["stop"].get_int()};
        }

        std::string hls1080 = item["hls_1080"].get_str();
        std::string hls720 = item["hls_720"].get_str();
        std::string hls480 = item["hls_480"].get_str();

        if (!hls1080.empty()) ep.stream_urls["1080p"] = hls1080;
        if (!hls720.empty()) ep.stream_urls["720p"] = hls720;
        if (!hls480.empty()) ep.stream_urls["480p"] = hls480;

        episodes.push_back(ep);
    }
    return episodes;
}

Stream AnilibriaProvider::get_stream(const Anime& /*anime*/, const Episode& episode) {
    Stream stream;
    for (const auto& [label, rel_url] : episode.stream_urls) {
        std::string full_url = rel_url;
        if (!full_url.empty() && full_url[0] == '/') {
            full_url = stream_host + full_url;
        }
        stream.qualities.push_back(Quality{label, full_url, {}});
    }
    return stream;
}

Anime AnilibriaProvider::parse_anime_item(const json::Value& item) {
    Anime a;
    a.id = std::to_string(item["id"].get_int());
    if (a.id == "0") a.id = item["id"].get_str();

    auto name = item["name"];
    a.title_ru = name["main"].get_str();
    if (a.title_ru.empty()) a.title_ru = name["english"].get_str();
    if (a.title_ru.empty()) a.title_ru = "Без назви";
    a.title_en = name["english"].get_str();

    a.year = item["year"].get_int();
    a.description = item["description"].get_str();
    a.status = item["type"]["description"].get_str();
    a.kind = parse_kind(item["type"]["value"].get_str());
    a.airing = item["is_ongoing"].get_bool() ? Airing::Ongoing : Airing::Finished;
    a.season = parse_season(item["season"]["value"].get_str());
    a.provider = this->name();

    auto genres_val = item["genres"];
    if (genres_val.is_array()) {
        for (size_t i = 0; i < genres_val.size(); ++i) {
            std::string g_name = genres_val[i]["name"].get_str();
            if (!g_name.empty()) a.genres.push_back(g_name);
        }
    }

    auto ar_val = item["age_rating"];
    if (ar_val.is_object()) {
        a.age_rating = ar_val["label"].get_str();
        a.age = parse_age(ar_val["value"].get_str());
    }

    int favs = item["added_in_users_favorites"].get_int();
    if (favs > 0) {
        a.rating = "★ " + std::to_string(favs);
    }

    auto poster = item["poster"];
    if (poster.is_object()) {
        std::string src = poster["src"].get_str();
        if (!src.empty()) {
            if (src[0] == '/') a.poster_url = "https://anilibria.top" + src;
            else a.poster_url = src;
        }
    }

    return a;
}

} // namespace anime
