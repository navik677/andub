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

std::vector<Anime> AnilibriaProvider::search(const std::string& query, int limit, const std::string& genre) {
    std::string url;
    if (!query.empty()) {
        url = api_base + "/app/search/releases?query=" + url_encode(query);
    } else if (!genre.empty()) {
        std::string lower_g = utils::utf8_tolower(genre);
        const auto& gmap = get_anilibria_genre_map();
        auto it = gmap.find(lower_g);
        if (it != gmap.end()) {
            url = api_base + "/anime/genres/" + std::to_string(it->second) + "/releases";
        } else {
            url = api_base + "/anime/releases/latest";
        }
    } else {
        url = api_base + "/anime/releases/latest";
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
        if (!genre.empty()) {
            bool matches = false;
            for (const auto& g : a.genres) {
                if (utils::utf8_contains_ci(g, genre)) {
                    matches = true;
                    break;
                }
            }
            if (!matches) continue;
        }
        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= limit) break;
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
