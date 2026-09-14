#include "anibaza_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include "../utils/kodik_resolver.hpp"
#include <iostream>
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

std::vector<Anime> AniBazaProvider::search(const std::string& query, int limit, const std::string& /*genre*/, int page) {
    size_t skip_count = (page > 1) ? static_cast<size_t>((page - 1) * limit) : 0;

    if (query.empty()) {
        std::map<std::string, std::string> home_headers = {
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
            {"Referer", base_url + "/"}
        };
        auto resp = http::Client::get(base_url, home_headers);
        if (resp.success()) {
            std::regex card_re(R"raw(<div class="release__card">[\s\S]*?<a\s+href="/release/([a-zA-Z0-9_-]+)/"[\s\S]*?<img[^>]+src="([^"]+)"[^>]+alt="([^"]+)")raw");
            auto begin = std::sregex_iterator(resp.body.begin(), resp.body.end(), card_re);
            auto end = std::sregex_iterator();
            std::vector<Anime> all_items;
            for (auto it = begin; it != end; ++it) {
                std::smatch m = *it;
                Anime a;
                a.id = m[1].str();
                a.meta["slug"] = a.id;
                a.title_ru = utils::trim(m[3].str());
                a.provider = name();
                std::string poster = m[2].str();
                if (!poster.empty()) {
                    a.poster_url = (poster.rfind("http", 0) != 0) ? (base_url + poster) : poster;
                }
                bool duplicate = false;
                for (const auto& existing : all_items) {
                    if (existing.id == a.id) { duplicate = true; break; }
                }
                if (!duplicate) {
                    all_items.push_back(std::move(a));
                }
            }

            std::vector<Anime> results;
            for (size_t i = skip_count; i < all_items.size(); ++i) {
                results.push_back(std::move(all_items[i]));
                if (static_cast<int>(results.size()) >= limit) break;
            }
            return results;
        }
    }

    std::string url = base_url + "/search/?query=" + anibaza_url_encode(query);

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"X-Requested-With", "XMLHttpRequest"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(url, headers);
    if (!resp.success()) {
        std::cerr << "[AniBaza] Search error: " << resp.error << " (status: " << resp.status_code << ")\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto results_val = root["results"];
    if (!results_val.is_array()) return {};

    std::vector<Anime> results;
    for (size_t i = skip_count; i < results_val.size(); ++i) {
        auto item = results_val[i];
        Anime a;
        a.id = item["slug"].get_str();
        a.title_ru = item["rus_title"].get_str();
        a.title_en = item["original_title"].get_str();
        a.provider = name();
        a.meta["slug"] = a.id;

        std::string poster = item["poster_url"].get_str();
        if (!poster.empty()) {
            if (poster.rfind("http", 0) != 0) {
                a.poster_url = base_url + poster;
            } else {
                a.poster_url = poster;
            }
        }

        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= limit) break;
    }

    return results;
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
