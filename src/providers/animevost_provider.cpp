#include "animevost_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/json.hpp"
#include <iostream>
#include <regex>

namespace anime {

std::vector<Anime> AnimeVostProvider::search(const std::string& query, int limit) {
    http::Response resp;
    if (query.empty()) {
        resp = http::Client::get(api_base + "/last");
    } else {
        std::string post_data = "name=" + query;
        resp = http::Client::post(api_base + "/search", post_data, {
            {"Content-Type", "application/x-www-form-urlencoded"}
        });
    }

    if (!resp.success()) {
        std::cerr << "[AnimeVost] Search error: " << resp.error << "\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto data_val = root["data"];
    if (!data_val.is_array()) return {};

    std::vector<Anime> results;
    size_t count = std::min(data_val.size(), static_cast<size_t>(limit));
    for (size_t i = 0; i < count; ++i) {
        results.push_back(parse_anime_item(data_val[i]));
    }
    return results;
}

std::vector<Episode> AnimeVostProvider::get_episodes(const Anime& anime) {
    std::string post_data = "id=" + anime.id;
    auto resp = http::Client::post(api_base + "/playlist", post_data, {
        {"Content-Type", "application/x-www-form-urlencoded"}
    });

    if (!resp.success()) {
        std::cerr << "[AnimeVost] Playlist fetch error: " << resp.error << "\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    if (!root.is_array()) return {};

    std::vector<Episode> episodes;
    for (size_t i = 0; i < root.size(); ++i) {
        const auto& item = root[i];
        Episode ep;
        std::string ep_name = item["name"].get_str("Епізод");
        ep.title = ep_name;

        // Try extracting number
        std::smatch match;
        std::regex num_reg(R"((\d+))");
        if (std::regex_search(ep_name, match, num_reg)) {
            ep.number = match.str(1);
        } else {
            ep.number = std::to_string(i + 1);
        }

        std::string hd = item["hd"].get_str();
        std::string std_url = item["std"].get_str();
        if (!hd.empty()) ep.stream_urls["720p"] = hd;
        if (!std_url.empty()) ep.stream_urls["480p"] = std_url;

        episodes.push_back(ep);
    }
    return episodes;
}

Stream AnimeVostProvider::get_stream(const Anime& /*anime*/, const Episode& episode) {
    Stream stream;
    for (const auto& [label, url] : episode.stream_urls) {
        stream.qualities.push_back(Quality{label, url, {}});
    }
    return stream;
}

Anime AnimeVostProvider::parse_anime_item(const json::Value& item) {
    Anime a;
    a.id = std::to_string(item["id"].get_int());
    if (a.id == "0") a.id = item["id"].get_str();

    std::string raw_title = item["title"].get_str("Без назви");
    // Split "Title RU / Title EN"
    size_t slash = raw_title.find('/');
    if (slash != std::string::npos) {
        a.title_ru = raw_title.substr(0, slash);
        // Trim right
        while (!a.title_ru.empty() && std::isspace(static_cast<unsigned char>(a.title_ru.back()))) {
            a.title_ru.pop_back();
        }
        a.title_en = raw_title.substr(slash + 1);
        while (!a.title_en.empty() && std::isspace(static_cast<unsigned char>(a.title_en.front()))) {
            a.title_en.erase(a.title_en.begin());
        }
    } else {
        a.title_ru = raw_title;
    }

    std::string yr_str = item["year"].get_str();
    if (!yr_str.empty()) {
        try { a.year = std::stoi(yr_str); } catch (...) {}
    } else {
        a.year = item["year"].get_int();
    }

    a.description = item["description"].get_str();
    // Replace <br> with newline
    std::regex br_regex(R"(<br\s*/?>)");
    a.description = std::regex_replace(a.description, br_regex, "\n");

    a.provider = name();
    a.poster_url = item["urlImagePreview"].get_str();

    return a;
}

} // namespace anime
